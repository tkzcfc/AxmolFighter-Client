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

    MG_DEFINE_SERIALIZABLE(baseAttribute, currentAttribute)

    ExprEval exprEval;
};

NS_MG_END
