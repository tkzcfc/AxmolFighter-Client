#include "mugen/core/bt/BTAction.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

void BTAction::onEnter(BTContext& ctx)
{
    entered = true;
    onActionEnter(ctx);
}

void BTAction::onExit(BTContext& ctx)
{
    if (entered)
        onActionExit(ctx);
    entered = false;
}

BTStatus BTAction::tick(BTContext& ctx, int32_t dtMs)
{
    if (!entered)
        onEnter(ctx);
    return onActionTick(ctx, dtMs);
}

NS_MG_END
