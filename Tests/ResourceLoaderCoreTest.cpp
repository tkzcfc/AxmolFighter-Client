#include "doctest/doctest.h"

#include "resource/LoadTask.h"
#include "resource/Resource.h"
#include "resource/ResourceLoaderRegistry.h"
#include "resource/detail/LoaderCore.h"

#include <memory>
#include <string>
#include <vector>

using namespace gameres;

namespace
{

class FakeResource : public Resource
{
public:
    FakeResource(ResourceType type, std::string key) : Resource(type), m_key(std::move(key)) {}

    std::string getKey() const override { return m_key; }

private:
    std::string m_key;
};

using FakePtr = std::shared_ptr<FakeResource>;

constexpr ResourceType customType(uint16_t offset)
{
    return static_cast<ResourceType>(static_cast<uint16_t>(ResourceType::CustomBegin) + offset);
}

constexpr auto kSyncType      = customType(1);  // 加载函数内同步完成
constexpr auto kAsyncType     = customType(2);  // 由测试代码在“下一帧”手动完成
constexpr auto kCompositeType = customType(3);  // 派生两个 kAsyncType 子资源
constexpr auto kFailType      = customType(4);  // 加载函数内同步失败

// 模拟真实的异步：kAsyncType 的任务先攒起来，测试代码在 tick 之后统一放行
struct AsyncQueue
{
    std::vector<LoadTaskPtr> tasks;

    void completeAll()
    {
        auto pending = std::move(tasks);
        tasks.clear();
        for (auto& t : pending)
            t->complete();
    }
};

void registerFakes(ResourceLoaderRegistry& registry, AsyncQueue& async)
{
    registry.registerType<FakeResource>(kSyncType, [](FakePtr, const LoadTaskPtr& task) { task->complete(); });
    registry.registerType<FakeResource>(kFailType, [](FakePtr, const LoadTaskPtr& task) { task->fail("nope"); });
    registry.registerType<FakeResource>(kAsyncType, [&async](FakePtr res, const LoadTaskPtr& task) {
        if (res->getKey().find("bad") != std::string::npos)
            task->fail("boom");
        else
            async.tasks.push_back(task);
    });
    registry.registerType<FakeResource>(kCompositeType, [](FakePtr res, const LoadTaskPtr& task) {
        task->spawn<FakeResource>(kAsyncType, res->getKey() + "-child-1");
        task->spawn<FakeResource>(kAsyncType, res->getKey() + "-child-2");
        task->onChildrenDone([task]() {
            if (task->anyChildFailed())
                task->fail("child failed");
            else
                task->complete();
        });
    }, 2.0f);
}

// LoaderCore 依赖 enable_shared_from_this，必须由 shared_ptr 持有
std::shared_ptr<detail::LoaderCore> makeCore(ResourceLoaderRegistry& registry)
{
    return std::make_shared<detail::LoaderCore>(registry);
}

ResourcePtr makeFake(ResourceType type, std::string key)
{
    return std::make_shared<FakeResource>(type, std::move(key));
}

}  // namespace

TEST_SUITE("ResourceLoader.Core")
{
    TEST_CASE("synchronous completion finishes on the first tick")
    {
        ResourceLoaderRegistry registry;
        AsyncQueue async;
        registerFakes(registry, async);

        auto core = makeCore(registry);
        core->addRoot(makeFake(kSyncType, "a"));

        bool completed = false;
        core->start(nullptr, [&](const std::vector<ResourcePtr>& failed) {
            completed = true;
            CHECK(failed.empty());
        });

        core->tick(0.0f);
        CHECK(core->isFinished());
        CHECK(completed);
    }

    TEST_CASE("duplicate type + key is deduplicated")
    {
        ResourceLoaderRegistry registry;
        int callCount = 0;
        registry.registerType<FakeResource>(kSyncType, [&callCount](FakePtr, const LoadTaskPtr& task) {
            ++callCount;
            task->complete();
        });

        auto core = makeCore(registry);
        auto a    = core->addRoot(makeFake(kSyncType, "same"));
        auto b    = core->addRoot(makeFake(kSyncType, "same"));
        CHECK_EQ(a.get(), b.get());

        core->start(nullptr, nullptr);
        core->tick(0.0f);
        CHECK_EQ(callCount, 1);
    }

    TEST_CASE("root weight comes from the registry")
    {
        ResourceLoaderRegistry registry;
        AsyncQueue async;
        registerFakes(registry, async);
        registry.setWeight(kAsyncType, 3.0f);

        auto core = makeCore(registry);
        core->addRoot(makeFake(kSyncType, "light"));    // 权重 1
        core->addRoot(makeFake(kAsyncType, "heavy"));   // 权重 3
        core->start(nullptr, nullptr);

        core->tick(0.0f);
        CHECK_EQ(core->getProgress(), doctest::Approx(0.25f));

        async.completeAll();
        CHECK_EQ(core->getProgress(), doctest::Approx(1.0f));
    }

    TEST_CASE("progress is reported only when a task finishes")
    {
        ResourceLoaderRegistry registry;
        AsyncQueue async;
        registerFakes(registry, async);

        auto core = makeCore(registry);
        core->addRoot(makeFake(kAsyncType, "a"));
        core->addRoot(makeFake(kAsyncType, "b"));

        std::vector<float> reports;
        bool completed = false;
        core->start([&](const LoadProgress& p) { reports.push_back(p.percent); }, [&](const auto&) {
            completed = true;
        });

        core->tick(0.0f);  // 首帧上报初始值 0
        core->tick(0.0f);  // 没有任务结束，不上报
        core->tick(0.0f);
        REQUIRE_EQ(reports.size(), 1);
        CHECK_EQ(reports[0], doctest::Approx(0.0f));

        async.completeAll();
        core->tick(0.0f);  // 两个任务结束，上报 100%，随后触发 onComplete
        REQUIRE_EQ(reports.size(), 2);
        CHECK_EQ(reports[1], doctest::Approx(1.0f));
        CHECK(completed);
    }

    TEST_CASE("cancel turns LoadTask methods into no-ops")
    {
        ResourceLoaderRegistry registry;
        AsyncQueue async;
        registerFakes(registry, async);

        auto core     = makeCore(registry);
        auto resource = core->addRoot(makeFake(kAsyncType, "a"));
        bool completed = false;
        core->start(nullptr, [&](const auto&) { completed = true; });
        core->tick(0.0f);
        CHECK_EQ(resource->getState(), ResourceState::Loading);

        core->cancel();
        CHECK_EQ(resource->getState(), ResourceState::Cancelled);

        async.completeAll();
        core->tick(0.0f);
        CHECK_EQ(resource->getState(), ResourceState::Cancelled);
        CHECK_FALSE(completed);
    }

    TEST_CASE("child failure makes the parent fail and is reported in onComplete")
    {
        ResourceLoaderRegistry registry;
        AsyncQueue async;
        registerFakes(registry, async);

        auto core   = makeCore(registry);
        auto parent = core->addRoot(makeFake(kCompositeType, "bad"));  // 子资源 key 含 bad，同步失败

        std::vector<ResourcePtr> failedResult;
        core->start(nullptr, [&](const std::vector<ResourcePtr>& failed) { failedResult = failed; });

        for (int i = 0; i < 10 && !core->isFinished(); ++i)
        {
            core->tick(0.0f);
            async.completeAll();
        }
        core->tick(0.0f);

        CHECK_EQ(parent->getState(), ResourceState::Failed);
        REQUIRE_EQ(failedResult.size(), 1);
        CHECK_EQ(failedResult.front().get(), parent.get());
    }

    TEST_CASE("collectHeldResources includes every started resource, including failed ones")
    {
        ResourceLoaderRegistry registry;
        AsyncQueue async;
        registerFakes(registry, async);

        auto core = makeCore(registry);
        core->setMaxConcurrent(1);
        core->addRoot(makeFake(kSyncType, "ok"));
        core->addRoot(makeFake(kFailType, "fail"));
        core->start(nullptr, nullptr);

        core->tick(0.0f);
        CHECK(core->isFinished());
        CHECK_EQ(core->collectHeldResources().size(), 2);
    }

    TEST_CASE("frame budget of 0 processes one main-thread work item per tick")
    {
        ResourceLoaderRegistry registry;
        AsyncQueue async;
        registerFakes(registry, async);

        auto core = makeCore(registry);
        core->setFrameBudget(0.0f);
        core->addRoot(makeFake(kSyncType, "a"));
        core->addRoot(makeFake(kSyncType, "b"));
        core->addRoot(makeFake(kSyncType, "c"));
        core->start(nullptr, nullptr);

        core->tick(0.0f);
        CHECK_FALSE(core->isFinished());
        core->tick(0.0f);
        CHECK_FALSE(core->isFinished());
        core->tick(0.0f);
        CHECK(core->isFinished());
    }

    TEST_CASE("composite finalization (onChildrenDone) is also deferred by the frame budget")
    {
        ResourceLoaderRegistry registry;
        AsyncQueue async;
        registerFakes(registry, async);

        auto core = makeCore(registry);
        core->setFrameBudget(0.0f);
        core->addRoot(makeFake(kCompositeType, "composite"));
        core->start(nullptr, nullptr);

        core->tick(0.0f);  // 启动组合资源，派生两个子资源
        core->tick(0.0f);  // 启动 child-1
        core->tick(0.0f);  // 启动 child-2
        async.completeAll();
        CHECK_FALSE(core->isFinished());  // 收尾回调只入队，尚未执行

        core->tick(0.0f);  // 执行收尾回调
        CHECK(core->isFinished());
    }

    TEST_CASE("composite resources release their concurrency slot while waiting on children (deadlock regression)")
    {
        ResourceLoaderRegistry registry;
        AsyncQueue async;
        registerFakes(registry, async);

        auto core = makeCore(registry);
        core->setMaxConcurrent(1);
        core->addRoot(makeFake(kCompositeType, "composite-1"));
        core->addRoot(makeFake(kCompositeType, "composite-2"));
        core->start(nullptr, nullptr);

        int frame = 0;
        for (; frame < 30 && !core->isFinished(); ++frame)
        {
            core->tick(0.0f);
            async.completeAll();
        }
        CHECK_MESSAGE(core->isFinished(), "loading stalled after " << frame << " frames");
    }
}
