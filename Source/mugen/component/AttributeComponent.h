#pragma once

#include "mugen/core/ecs/Component.h"
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

    MG_DEFINE_SERIALIZABLE(baseAttribute, currentAttribute, ep, epMax)

    ExprEval exprEval;
};

NS_MG_END
