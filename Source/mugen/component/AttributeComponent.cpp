#include "AttributeComponent.h"

NS_MG_BEGIN

AttributeComponent::AttributeComponent()
{
    setFacingDirection(FacingDirection::kFacingRight);
}

AttributeComponent::~AttributeComponent() {}

void AttributeComponent::bindVariable()
{
    exprEval.setVars({{"hpMax", static_cast<double>(currentAttribute.hpMax)},
                      {"mpMax", static_cast<double>(currentAttribute.mpMax)},
                      {"physicalAttack", static_cast<double>(currentAttribute.physicalAttack)},
                      {"physicalDefense", static_cast<double>(currentAttribute.physicalDefense)},
                      {"magicAttack", static_cast<double>(currentAttribute.magicAttack)},
                      {"magicDefense", static_cast<double>(currentAttribute.magicDefense)},
                      {"darkResistance", static_cast<double>(currentAttribute.darkResistance)},
                      {"lightResistance", static_cast<double>(currentAttribute.lightResistance)},
                      {"mpRegenSpeed", static_cast<double>(currentAttribute.mpRegenSpeed)},
                      {"moveSpeed", static_cast<double>(currentAttribute.moveSpeed)},
                      {"attackSpeed", static_cast<double>(currentAttribute.attackSpeed)},
                      {"castSpeed", static_cast<double>(currentAttribute.castSpeed)},
                      {"hitRecovery", static_cast<double>(currentAttribute.hitRecovery)},
                      {"jumpSpeed", static_cast<double>(currentAttribute.jumpSpeed)}});
}

void AttributeComponent::setFacingDirection(FacingDirection facingDirection)
{
    exprEval.setVar("facingDirection", facingDirection == FacingDirection::kFacingRight ? 1.0 : -1.0);
}

NS_MG_END
