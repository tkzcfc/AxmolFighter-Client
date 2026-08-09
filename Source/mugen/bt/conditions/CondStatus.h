#pragma once

#include "mugen/core/bt/BTCondition.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

/** 按 BehaviorKind / 状态位判断是否进入该枝 */
class CondStatus : public BTCondition
{
public:
    explicit CondStatus(BehaviorKind kind) : kind(kind) { debugName = "CondStatus"; }

    bool check(BTContext& ctx) override;

    BehaviorKind kind;
};

NS_MG_END
