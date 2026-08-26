#include "mugen/core/bt/BTAction.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

BTAction::BTAction() {}

BTAction::~BTAction() {}

bool BTAction::onEnter(BTContext& ctx)
{
    onActionEnter(ctx);
    return true;
}

void BTAction::onExit(BTContext& ctx)
{
    onActionExit(ctx);
}

BTStatus BTAction::onUpdate(BTContext& ctx, int32_t dtMs)
{
    return onActionUpdate(ctx, dtMs);
}

NS_MG_END
