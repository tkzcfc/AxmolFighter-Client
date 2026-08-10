#pragma once

#include "mugen/core/bt/BTAction.h"

NS_MG_BEGIN

class PatrolAction : public BTAction
{
public:
    PatrolAction() { debugName = "PatrolAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionTick(BTContext& ctx, int32_t dtMs) override;
};

class AlertAction : public BTAction
{
public:
    AlertAction() { debugName = "AlertAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionTick(BTContext& ctx, int32_t dtMs) override;
};

class ChaseAction : public BTAction
{
public:
    ChaseAction() { debugName = "ChaseAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionTick(BTContext& ctx, int32_t dtMs) override;
};

class JostledAction : public BTAction
{
public:
    JostledAction() { debugName = "JostledAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionTick(BTContext& ctx, int32_t dtMs) override;
};

NS_MG_END
