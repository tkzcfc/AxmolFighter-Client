#include "resource/LoadTask.h"

#include "resource/detail/LoaderCore.h"

#include "axmol.h"

namespace gameres
{

void LoadTask::complete()
{
    if (!isCancelled())
        m_core.lock()->completeEntry(m_entry);
}

void LoadTask::fail(std::string_view error)
{
    if (!isCancelled())
        m_core.lock()->failEntry(m_entry, std::string(error));
}

bool LoadTask::isCancelled() const
{
    auto core = m_core.lock();
    return !core || core->isCancelledFlag();
}

void LoadTask::runOnWorker(std::function<void()> work, std::function<void()> then)
{
    ax::Director::getInstance()->getJobSystem()->enqueue(std::move(work), [core = m_core, then = std::move(then)]() {
        // then 进入 LoaderCore 的主线程工作队列，在 tick 的时间预算内执行；已取消时被丢弃
        if (auto locked = core.lock())
            locked->postMainThreadJob(then);
    });
}

ResourcePtr LoadTask::spawnResource(ResourcePtr child)
{
    if (isCancelled())
        return child;
    return m_core.lock()->spawnChild(m_entry, std::move(child))->resource;
}

void LoadTask::onChildrenDone(std::function<void()> fn)
{
    if (!isCancelled())
        m_core.lock()->setOnChildrenDone(m_entry, std::move(fn));
}

bool LoadTask::anyChildFailed() const
{
    auto core = m_core.lock();
    return core && core->anyChildFailed(m_entry);
}

}  // namespace gameres
