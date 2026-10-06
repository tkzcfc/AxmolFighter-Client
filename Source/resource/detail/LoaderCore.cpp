#include "resource/detail/LoaderCore.h"

#include "resource/LoadTask.h"

#include "axmol.h"

#include <algorithm>
#include <chrono>

namespace gameres::detail
{

namespace
{
std::string makeKey(const ResourcePtr& resource)
{
    return std::to_string(static_cast<uint16_t>(resource->getType())) + "#" + resource->getKey();
}
}  // namespace

float computeEntryProgress(const Entry& e)
{
    if (e.finished)
        return 1.0f;
    if (e.children.empty())
        return 0.0f;

    float sum = 0.0f;
    for (auto* c : e.children)
        sum += computeEntryProgress(*c);
    return sum / static_cast<float>(e.children.size());
}

float computeWeightedProgress(const std::vector<Entry*>& roots)
{
    if (roots.empty())
        return 1.0f;

    float totalWeight = 0.0f;
    float sum         = 0.0f;
    size_t finished   = 0;
    for (auto* r : roots)
    {
        totalWeight += r->weight;
        sum += r->weight * computeEntryProgress(*r);
        if (r->finished)
            ++finished;
    }

    if (totalWeight <= 0.0f)
        return static_cast<float>(finished) / static_cast<float>(roots.size());
    return sum / totalWeight;
}

LoaderCore::LoaderCore(gameres::ResourceLoaderRegistry& registry) : m_registry(registry) {}

Entry* LoaderCore::findOrCreateEntry(const ResourcePtr& resource, bool isRoot)
{
    const std::string key = makeKey(resource);
    if (auto it = m_byKey.find(key); it != m_byKey.end())
        return it->second;

    auto owned    = std::make_unique<Entry>();
    Entry* raw    = owned.get();
    raw->resource = resource;
    raw->isRoot   = isRoot;
    m_entries.push_back(std::move(owned));
    m_byKey.emplace(key, raw);

    if (isRoot)
    {
        raw->weight = m_registry.getWeight(resource->getType());
        m_roots.push_back(raw);
        m_pending.push_back(raw);
    }
    else
    {
        // 子条目排到队头：深度优先，先把已经开始的组合资源做完
        m_pending.push_front(raw);
    }
    return raw;
}

ResourcePtr LoaderCore::addRoot(ResourcePtr resource)
{
    AXASSERT(!m_started, "ResourceLoader: add() must be called before start()");
    return findOrCreateEntry(resource, true)->resource;
}

void LoaderCore::setMaxConcurrent(size_t n)
{
    m_maxConcurrent = std::max<size_t>(n, 1);
}

void LoaderCore::setFrameBudget(float milliseconds)
{
    m_frameBudgetMs = std::max(milliseconds, 0.0f);
}

void LoaderCore::start(gameres::ResourceLoader::ProgressCallback onProgress,
                       gameres::ResourceLoader::CompleteCallback onComplete)
{
    AXASSERT(!m_started, "ResourceLoader::start called more than once");
    m_started    = true;
    m_onProgress = std::move(onProgress);
    m_onComplete = std::move(onComplete);
}

void LoaderCore::cancel()
{
    if (m_cancelled)
        return;
    m_cancelled = true;
    m_pending.clear();
    m_mainThreadJobs.clear();
    for (auto& e : m_entries)
    {
        if (!e->finished)
            e->resource->markCancelled();
    }
}

bool LoaderCore::canStartEntry() const
{
    return m_inFlight < m_maxConcurrent && !m_pending.empty();
}

void LoaderCore::runMainThreadWork()
{
    using Clock = std::chrono::steady_clock;
    // 必须先转成整数时长：float 时长与 time_point 相加会得到 float 表示的时间点，精度只有毫秒级
    const auto deadline = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                             std::chrono::duration<float, std::milli>(m_frameBudgetMs));

    // 收尾工作优先：它们会结束条目、释放名额；然后才启动新任务。
    // 加载函数里 spawn 的子条目排在 m_pending 队头，同一个循环里继续被处理。
    bool first = true;
    while (!m_cancelled && (first || Clock::now() < deadline))
    {
        if (!m_mainThreadJobs.empty())
        {
            auto job = std::move(m_mainThreadJobs.front());
            m_mainThreadJobs.pop_front();
            job();
        }
        else if (canStartEntry())
        {
            Entry* e = m_pending.front();
            m_pending.pop_front();
            startEntry(e);
        }
        else
        {
            break;
        }
        first = false;

        // 预算为 0：每帧只处理一项
        if (m_frameBudgetMs <= 0.0f)
            break;
    }
}

void LoaderCore::startEntry(Entry* e)
{
    e->started   = true;
    e->holdsSlot = true;
    ++m_inFlight;
    e->resource->markLoading();

    const auto* loaderFn = m_registry.find(e->resource->getType());
    if (!loaderFn)
    {
        finishEntry(e, false, "no loader registered for this resource type");
        return;
    }

    auto task = std::shared_ptr<LoadTask>(new LoadTask(weak_from_this(), e));
    (*loaderFn)(e->resource, task);
}

void LoaderCore::completeEntry(Entry* e)
{
    finishEntry(e, true, {});
}

void LoaderCore::failEntry(Entry* e, std::string error)
{
    finishEntry(e, false, std::move(error));
}

void LoaderCore::finishEntry(Entry* e, bool success, std::string error)
{
    if (e->finished)
        return;

    e->finished = true;
    e->success  = success;
    ++m_finishedEntries;
    if (success)
    {
        e->resource->markLoaded();
    }
    else
    {
        AXLOGW("ResourceLoader: failed to load {} '{}': {}", gameres::toString(e->resource->getType()),
               e->resource->getKey(), error);
        e->resource->markFailed(std::move(error));
    }

    if (e->holdsSlot)
    {
        e->holdsSlot = false;
        --m_inFlight;
    }

    for (auto* parent : e->parents)
        checkChildrenDone(parent);
}

Entry* LoaderCore::spawnChild(Entry* parent, ResourcePtr child)
{
    Entry* c = findOrCreateEntry(child, false);
    if (c == parent || std::find(parent->children.begin(), parent->children.end(), c) != parent->children.end())
        return c;

    parent->children.push_back(c);
    c->parents.push_back(parent);

    // 父条目开始等待子条目，让出并发名额（见 Entry::holdsSlot）
    if (parent->holdsSlot)
    {
        parent->holdsSlot = false;
        --m_inFlight;
    }
    return c;
}

void LoaderCore::setOnChildrenDone(Entry* parent, std::function<void()> fn)
{
    parent->onChildrenDoneFn = std::move(fn);
    checkChildrenDone(parent);
}

bool LoaderCore::anyChildFailed(const Entry* parent) const
{
    return std::any_of(parent->children.begin(), parent->children.end(), [](const Entry* c) { return !c->success; });
}

void LoaderCore::checkChildrenDone(Entry* parent)
{
    if (parent->finished || parent->childrenNotified || !parent->onChildrenDoneFn)
        return;
    if (!std::all_of(parent->children.begin(), parent->children.end(), [](const Entry* c) { return c->finished; }))
        return;

    // 收尾回调可能很重（例如 FGUI 接管纹理、解析 plist），排队到 tick 的时间预算内执行
    parent->childrenNotified = true;
    postMainThreadJob(std::move(parent->onChildrenDoneFn));
}

void LoaderCore::postMainThreadJob(std::function<void()> job)
{
    if (!m_cancelled)
        m_mainThreadJobs.push_back(std::move(job));
}

float LoaderCore::getProgress() const
{
    return computeWeightedProgress(m_roots);
}

bool LoaderCore::isFinished() const
{
    return std::all_of(m_roots.begin(), m_roots.end(), [](const Entry* r) { return r->finished; });
}

std::vector<ResourcePtr> LoaderCore::getFailedResources() const
{
    std::vector<ResourcePtr> result;
    for (auto* r : m_roots)
    {
        if (r->finished && !r->success)
            result.push_back(r->resource);
    }
    return result;
}

std::vector<ResourcePtr> LoaderCore::collectHeldResources() const
{
    std::vector<ResourcePtr> result;
    for (auto& e : m_entries)
    {
        if (e->started)
            result.push_back(e->resource);
    }
    return result;
}

void LoaderCore::reportProgress()
{
    // 只在有任务（含子任务）结束时上报；首次 tick 也上报一次作为初始值
    if (!m_onProgress || m_finishedEntries == m_reportedEntries)
        return;
    m_reportedEntries = m_finishedEntries;

    LoadProgress progress;
    progress.percent    = getProgress();
    progress.totalCount = m_roots.size();
    progress.finishedCount =
        static_cast<size_t>(std::count_if(m_roots.begin(), m_roots.end(), [](const Entry* r) { return r->finished; }));
    m_onProgress(progress);
}

void LoaderCore::tick(float /*dt*/)
{
    if (m_cancelled || m_completedCalled)
        return;

    runMainThreadWork();
    reportProgress();

    if (isFinished())
    {
        m_completedCalled = true;
        std::vector<ResourcePtr> failed;
        for (auto* r : m_roots)
        {
            if (!r->success)
                failed.push_back(r->resource);
        }
        if (m_onComplete)
            m_onComplete(failed);
    }
}

}  // namespace gameres::detail
