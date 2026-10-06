#include "resource/ResourceLoader.h"

#include "resource/detail/LoaderCore.h"

#include "axmol.h"

namespace gameres
{

namespace
{
constexpr std::string_view kScheduleKey = "gameres.ResourceLoader";
}  // namespace

ResourceLoader::ResourceLoader() : m_core(std::make_shared<detail::LoaderCore>()) {}

ResourceLoader::~ResourceLoader()
{
    cancel();

    auto held = m_core->collectHeldResources();
    if (!held.empty())
    {
        ax::Director::getInstance()->getScheduler()->runOnAxmolThread([held = std::move(held)]() {
            for (auto& resource : held)
                resource->releaseHold();
        });
    }
}

ResourcePtr ResourceLoader::addResource(ResourcePtr resource)
{
    return m_core->addRoot(std::move(resource));
}

void ResourceLoader::setMaxConcurrent(size_t maxConcurrent)
{
    m_core->setMaxConcurrent(maxConcurrent);
}

void ResourceLoader::setFrameBudget(float milliseconds)
{
    m_core->setFrameBudget(milliseconds);
}

void ResourceLoader::start(ProgressCallback onProgress, CompleteCallback onComplete)
{
    m_core->start(std::move(onProgress), std::move(onComplete));

    // 调度目标用 LoaderCore 而不是 this：lambda 持有 shared_ptr<LoaderCore>，
    // 即使 ResourceLoader 在 onComplete 回调里被销毁，lambda 剩余代码也不会访问它。
    m_scheduled = true;
    ax::Director::getInstance()->getScheduler()->schedule([core = m_core](float dt) {
        core->tick(dt);
        if (core->isDone())
            ax::Director::getInstance()->getScheduler()->unschedule(kScheduleKey, core.get());
    }, m_core.get(), 0.0f, false, kScheduleKey);
}

void ResourceLoader::unschedule()
{
    if (!m_scheduled)
        return;
    m_scheduled = false;
    ax::Director::getInstance()->getScheduler()->unschedule(kScheduleKey, m_core.get());
}

void ResourceLoader::cancel()
{
    m_core->cancel();
    unschedule();
}

float ResourceLoader::getProgress() const
{
    return m_core->getProgress();
}

bool ResourceLoader::isFinished() const
{
    return m_core->isFinished();
}

}  // namespace gameres
