#include "mugen/core/bt/BTNode.h"
#include "mugen/core/StdC.h"

NS_MG_BEGIN

BTNode::BTNode() : m_status(BTStatus::Readied) {}

BTNode::~BTNode() {}

bool BTNode::enter(BTContext& ctx)
{
    const bool ok = onEnter(ctx);
    m_status      = ok ? BTStatus::Running : BTStatus::Failure;
    return ok;
}

void BTNode::update(BTContext& ctx, int32_t dtMs)
{
    if (!isRunning())
        return;
    const BTStatus next = onUpdate(ctx, dtMs);
    // 确保返回状态正确,节点不应在 update 中直接回到 Readied 状态
    MG_ASSERT(next != BTStatus::Readied);
    m_status = next;
}

void BTNode::exit(BTContext& ctx)
{
    onExit(ctx);
    m_status = BTStatus::Readied;
}

NS_MG_END
