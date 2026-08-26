#include "mugen/core/bt/BTAction.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

BTAction::BTAction() {}

BTAction::~BTAction() {}

void BTAction::enter(BTContext& ctx)
{
    status = BTStatus::Running;
    onActionEnter(ctx);
}

void BTAction::exit(BTContext& ctx)
{
    onActionExit(ctx);
    status = BTStatus::Readied;
}

void BTAction::update(BTContext& ctx, int32_t dtMs)
{
    if (status != BTStatus::Running)
        return;
    onActionUpdate(ctx, dtMs);
}

NS_MG_END
