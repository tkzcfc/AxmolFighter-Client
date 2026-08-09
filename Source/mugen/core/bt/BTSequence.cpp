#include "mugen/core/bt/BTSequence.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/component/BehaviorTreeComponent.h"

NS_MG_BEGIN

namespace
{
int8_t& memoryAt(BTContext& ctx, int32_t slot)
{
    static int8_t s_dummy = -1;
    if (!ctx.bt || slot < 0)
        return s_dummy;
    if (static_cast<int32_t>(ctx.bt->selectorMemory.size()) <= slot)
        ctx.bt->selectorMemory.resize(static_cast<size_t>(slot) + 1, -1);
    return ctx.bt->selectorMemory[static_cast<size_t>(slot)];
}
}  // namespace

void BTSequence::onEnter(BTContext& ctx)
{
    enterConditions(ctx);
    int8_t& mem = memoryAt(ctx, memorySlot);
    mem         = 0;
    if (!children.empty())
        children[0]->onEnter(ctx);
}

void BTSequence::onExit(BTContext& ctx)
{
    int8_t& mem = memoryAt(ctx, memorySlot);
    if (mem >= 0 && mem < static_cast<int8_t>(children.size()))
        children[static_cast<size_t>(mem)]->onExit(ctx);
    mem = -1;
    exitConditions(ctx);
}

BTStatus BTSequence::tick(BTContext& ctx, int32_t dtMs)
{
    if (children.empty())
        return BTStatus::Success;

    int8_t& mem = memoryAt(ctx, memorySlot);
    if (mem < 0)
    {
        mem = 0;
        children[0]->onEnter(ctx);
    }

    while (mem < static_cast<int8_t>(children.size()))
    {
        auto* child = children[static_cast<size_t>(mem)].get();
        if (!child->checkAll(ctx))
        {
            child->onExit(ctx);
            mem = -1;
            return BTStatus::Failure;
        }

        const BTStatus st = child->tick(ctx, dtMs);
        if (st == BTStatus::Running)
            return BTStatus::Running;
        if (st == BTStatus::Failure)
        {
            child->onExit(ctx);
            mem = -1;
            return BTStatus::Failure;
        }

        // Success → 推进
        child->onExit(ctx);
        ++mem;
        if (mem < static_cast<int8_t>(children.size()))
            children[static_cast<size_t>(mem)]->onEnter(ctx);
    }

    mem = -1;
    return BTStatus::Success;
}

NS_MG_END
