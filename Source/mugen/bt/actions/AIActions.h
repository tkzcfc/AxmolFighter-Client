#pragma once

#include "mugen/core/bt/BTAction.h"

NS_MG_BEGIN

// 巡逻：在出生点范围内随机走动，到达后短暂停步。
class PatrolAction : public BTAction
{
public:
    PatrolAction();
    virtual ~PatrolAction();

    const char* typeName() const override { return "PatrolAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;
};

// 警觉：面向玩家并停步，倒计时结束 Success。
class AlertAction : public BTAction
{
public:
    AlertAction();
    virtual ~AlertAction();

    const char* typeName() const override { return "AlertAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;
};

// 追击：朝最近玩家移动。
class ChaseAction : public BTAction
{
public:
    ChaseAction();
    virtual ~ChaseAction();

    const char* typeName() const override { return "ChaseAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;
};

// 挤开：与重叠的玩家/怪物互相施加冲量。
class JostledAction : public BTAction
{
public:
    JostledAction();
    virtual ~JostledAction();

    const char* typeName() const override { return "JostledAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;
};

// 寻路回出生点：离巡逻范围过远时往 spawn 走。
class PathFindingAction : public BTAction
{
public:
    PathFindingAction();
    virtual ~PathFindingAction();

    const char* typeName() const override { return "PathFindingAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;
};

NS_MG_END
