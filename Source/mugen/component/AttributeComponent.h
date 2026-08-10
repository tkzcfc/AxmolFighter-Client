#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/buff/ExtendAttribute.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/TableConfig.h"
#include "mugen/core/expr/ExprEval.h"

NS_MG_BEGIN

class AttributeComponent : public Component
{
public:
    typedef Component Super;

public:
    AttributeComponent();

    virtual ~AttributeComponent();

    void bindVariable();

    void setFacingDirection(FacingDirection facingDirection);

public:
    RoleAttributeConfig baseAttribute;
    RoleAttributeConfig currentAttribute;

    // 运行时怒气/EP（表里 SkillAttack.ep；RoleAttribute 无对应字段）
    float ep    = 0.0f;
    float epMax = 100.0f;

    // 技能资源 / 消耗缩放（castBegan 用）
    int32_t crystal           = 0;
    float mpConsumeScale      = 1.0f;
    float epConsumeScale      = 1.0f;
    float epPlus              = 0.0f;

    // Buff 扩展属性通道（ADD_HURT / AVOID_HURT / ADD_CRIT …）
    ExtendAttribute extendAttribute;

    // Phase 1.4：顿帧剩余（ms，可序列化）
    int32_t freezeRemainingMs = 0;
    int32_t freezeDelayMs     = 0;  // >0 时先延迟再进入 freezeRemainingMs

    MG_DEFINE_SERIALIZABLE(baseAttribute,
                           currentAttribute,
                           ep,
                           epMax,
                           crystal,
                           mpConsumeScale,
                           epConsumeScale,
                           epPlus,
                           extendAttribute,
                           freezeRemainingMs,
                           freezeDelayMs)

    ExprEval exprEval;
};

NS_MG_END
