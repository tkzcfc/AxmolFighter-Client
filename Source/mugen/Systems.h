#pragma once

#include "system/AISystem.h"
#include "system/AttributeSystem.h"
#include "system/AvatarRenderSystem.h"
#include "system/AvatarSystem.h"
#include "system/BehaviorTreeSystem.h"
#include "system/BuffSystem.h"
#include "system/CombatSystem.h"
#include "system/DisplacementSystem.h"
#include "system/EffectLifeSystem.h"
#include "system/EffectLifetimeSystem.h"
#include "system/GameMapRenderSystem.h"
#include "system/GameMapSystem.h"
#include "system/InputSystem.h"
#include "system/PhysicsSystem.h"
#include "system/SoundSystem.h"

// clang-format off
#define SYSTEM_LIST X(AISystem) X(AttributeSystem) X(AvatarRenderSystem) X(AvatarSystem) X(BehaviorTreeSystem) X(BuffSystem) X(CombatSystem) X(DisplacementSystem) X(EffectLifeSystem) X(EffectLifetimeSystem) X(GameMapRenderSystem) X(GameMapSystem) X(InputSystem) X(PhysicsSystem) X(SoundSystem)
// clang-format on