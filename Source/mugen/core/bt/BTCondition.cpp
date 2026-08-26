#include "mugen/core/bt/BTCondition.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

BTCondition::BTCondition() {}

BTCondition::~BTCondition() {}

void BTCondition::enter(BTContext& ctx)
{
    status = BTStatus::Running;
    onEnter(ctx);
}

void BTCondition::exit(BTContext& ctx)
{
    onExit(ctx);
    status = BTStatus::Readied;
}

NS_MG_END
