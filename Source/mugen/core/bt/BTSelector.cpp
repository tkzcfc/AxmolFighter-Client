#include "mugen/core/bt/BTSelector.h"
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

void BTSelector::onEnter(BTContext& ctx)
{
    enterConditions(ctx);
    memoryAt(ctx, memorySlot) = -1;
}

void BTSelector::onExit(BTContext& ctx)
{
    int8_t& mem = memoryAt(ctx, memorySlot);
    if (mem >= 0 && mem < static_cast<int8_t>(children.size()))
    {
        children[static_cast<size_t>(mem)]->onExit(ctx);
        mem = -1;
    }
    exitConditions(ctx);
}

BTStatus BTSelector::tick(BTContext& ctx, int32_t dtMs)
{
    if (children.empty())
        return BTStatus::Failure;

    int8_t& mem = memoryAt(ctx, memorySlot);

    // 粘滞：当前子节点条件仍通过则继续（对齐黑月 AEBTSelector）
    if (mem >= 0 && mem < static_cast<int8_t>(children.size()))
    {
        auto* child = children[static_cast<size_t>(mem)].get();
        if (!child->checkAll(ctx))
        {
            // 条件失败：exit 当前，本节点 Success，由父级重选
            child->onExit(ctx);
            mem = -1;
            return BTStatus::Success;
        }

        const BTStatus st = child->tick(ctx, dtMs);
        if (st == BTStatus::Running)
            return BTStatus::Running;

        // Success/Failure：exit 后环形尝试后续兄弟；皆非 Running → 本节点 Success
        child->onExit(ctx);
        const size_t start = static_cast<size_t>(mem) + 1;
        mem                = -1;
        const size_t n     = children.size();
        for (size_t k = 0; k < n; ++k)
        {
            const size_t i = (start + k) % n;
            auto* sib      = children[i].get();
            if (!sib->checkAll(ctx))
                continue;
            sib->onEnter(ctx);
            const BTStatus st2 = sib->tick(ctx, dtMs);
            if (st2 == BTStatus::Running)
            {
                mem = static_cast<int8_t>(i);
                return BTStatus::Running;
            }
            sib->onExit(ctx);
        }
        return BTStatus::Success;
    }

    // 无粘滞：按优先级扫描
    for (size_t i = 0; i < children.size(); ++i)
    {
        auto* child = children[i].get();
        if (!child->checkAll(ctx))
            continue;
        child->onEnter(ctx);
        mem               = static_cast<int8_t>(i);
        const BTStatus st = child->tick(ctx, dtMs);
        if (st == BTStatus::Running)
            return BTStatus::Running;
        child->onExit(ctx);
        mem = -1;
        // 同帧继续找下一个 Running（简化环形首扫）
    }
    return BTStatus::Failure;
}

NS_MG_END
