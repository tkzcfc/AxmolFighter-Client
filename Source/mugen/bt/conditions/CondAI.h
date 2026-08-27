#pragma once

#include "mugen/core/bt/BTCondition.h"

NS_MG_BEGIN

// 怪物巡逻：在出生范围内、玩家未进入追击/目标范围、且未在施法。
class CondPatrol : public BTCondition
{
public:
    CondPatrol();
    virtual ~CondPatrol();
    const char* typeName() const override { return "CondPatrol"; }
    bool check(BTContext& ctx) override;
};

// 怪物警觉：玩家进入目标范围且尚未 alertDone。
class CondAlert : public BTCondition
{
public:
    CondAlert();
    virtual ~CondAlert();
    const char* typeName() const override { return "CondAlert"; }
    bool check(BTContext& ctx) override;
};

// 怪物追击：玩家在 chase 范围（或警觉完成后的目标范围），且尚未进入攻击距离。
class CondChase : public BTCondition
{
public:
    CondChase();
    virtual ~CondChase();
    const char* typeName() const override { return "CondChase"; }
    bool check(BTContext& ctx) override;
};

// AABB 与其他玩家/怪物重叠时挤开。
class CondJostled : public BTCondition
{
public:
    CondJostled();
    virtual ~CondJostled();
    const char* typeName() const override { return "CondJostled"; }
    bool check(BTContext& ctx) override;
};

// 离出生点超过巡逻范围两倍，走回 spawn。
class CondPathFinding : public BTCondition
{
public:
    CondPathFinding();
    virtual ~CondPathFinding();
    const char* typeName() const override { return "CondPathFinding"; }
    bool check(BTContext& ctx) override;
};

NS_MG_END
