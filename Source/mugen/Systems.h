#pragma once

#include "system/AttributeSystem.h"
#include "system/AvatarRenderSystem.h"
#include "system/AvatarSystem.h"
#include "system/BehaviorSystem.h"
#include "system/BuffSystem.h"
#include "system/CombatSystem.h"
#include "system/DisplacementSystem.h"
#include "system/EffectLifetimeSystem.h"
#include "system/GameMapRenderSystem.h"
#include "system/GameMapSystem.h"
#include "system/InputSystem.h"
#include "system/PhysicsSystem.h"
#include "system/AISystem.h"
#include "system/SoundSystem.h"

// clang-format off
#define SYSTEM_LIST X(AttributeSystem) X(AvatarRenderSystem) X(AvatarSystem) X(AISystem) X(BuffSystem) X(EffectLifetimeSystem) X(CombatSystem) X(BehaviorSystem) X(DisplacementSystem) X(GameMapRenderSystem) X(GameMapSystem) X(InputSystem) X(PhysicsSystem) X(SoundSystem)
// clang-format on
