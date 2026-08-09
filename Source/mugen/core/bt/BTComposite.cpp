#include "mugen/core/bt/BTComposite.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

void BTComposite::addCondition(std::unique_ptr<BTCondition> cond)
{
    if (cond)
        conditions.push_back(std::move(cond));
}

void BTComposite::addChild(std::unique_ptr<BTNode> child)
{
    if (child)
        children.push_back(std::move(child));
}

void BTComposite::clearChildren()
{
    children.clear();
}

bool BTComposite::checkAll(BTContext& ctx)
{
    for (auto& cond : conditions)
    {
        if (!cond || !cond->check(ctx))
            return false;
    }
    return true;
}

void BTComposite::enterConditions(BTContext& ctx)
{
    if (conditionsEntered)
        return;
    for (auto& cond : conditions)
    {
        if (cond)
            cond->onEnter(ctx);
    }
    conditionsEntered = true;
}

void BTComposite::exitConditions(BTContext& ctx)
{
    if (!conditionsEntered)
        return;
    for (auto& cond : conditions)
    {
        if (cond)
            cond->onExit(ctx);
    }
    conditionsEntered = false;
}

NS_MG_END
