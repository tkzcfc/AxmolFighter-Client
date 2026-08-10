#pragma once

#include "component/ActorDataComponent.h"
#include "component/AIComponent.h"
#include "component/AttributeComponent.h"
#include "component/AvatarComponent.h"
#include "component/AvatarRenderComponent.h"
#include "component/BehaviorComponent.h"
#include "component/BehaviorTreeComponent.h"
#include "component/BuffComponent.h"
#include "component/DirectorComponent.h"
#include "component/DisplacementComponent.h"
#include "component/EffectLifetimeComponent.h"
#include "component/GameMapComponent.h"
#include "component/GameMapRenderComponent.h"
#include "component/HitReactComponent.h"
#include "component/IdentityComponent.h"
#include "component/InputComponent.h"
#include "component/PhysicsComponent.h"
#include "component/SkillBarComponent.h"
#include "component/SkillCastComponent.h"
#include "component/SkillDeckComponent.h"
#include "component/SoundComponent.h"
#include "component/TransformComponent.h"

// clang-format off
#define COMPONENT_LIST X(ActorDataComponent) X(AIComponent) X(AttributeComponent) X(AvatarComponent) X(AvatarRenderComponent) X(BehaviorComponent) X(BehaviorTreeComponent) X(BuffComponent) X(DirectorComponent) X(DisplacementComponent) X(EffectLifetimeComponent) X(GameMapComponent) X(GameMapRenderComponent) X(HitReactComponent) X(IdentityComponent) X(InputComponent) X(PhysicsComponent) X(SkillBarComponent) X(SkillCastComponent) X(SkillDeckComponent) X(SoundComponent) X(TransformComponent)
// clang-format on