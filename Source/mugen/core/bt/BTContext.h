#pragma once

#include "mugen/core/bt/BTNode.h"

NS_MG_BEGIN

class Entity;
class ECSManager;
class BehaviorTreeComponent;
class BehaviorComponent;
class SkillCastComponent;
class HitReactComponent;
class AvatarComponent;
class TransformComponent;
class AttributeComponent;
class DisplacementComponent;
class BuffComponent;
class InputComponent;
class PhysicsComponent;
class SkillDeckComponent;

struct BTContext
{
    Entity* entity     = nullptr;
    ECSManager* ecs    = nullptr;
    int32_t dtMs       = 0;
    int64_t runningTimeMs = 0;

    BehaviorTreeComponent* bt = nullptr;
    BehaviorComponent* behavior = nullptr;
    SkillCastComponent* skillCast = nullptr;
    HitReactComponent* hitReact = nullptr;
    AvatarComponent* avatar = nullptr;
    TransformComponent* transform = nullptr;
    AttributeComponent* attribute = nullptr;
    DisplacementComponent* displacement = nullptr;
    BuffComponent* buff = nullptr;
    InputComponent* input = nullptr;
    PhysicsComponent* physics = nullptr;
    SkillDeckComponent* skillDeck = nullptr;
};

NS_MG_END
