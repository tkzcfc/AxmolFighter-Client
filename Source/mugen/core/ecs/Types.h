#pragma once

#include "mugen/core/Object.h"
#include <bitset>

NS_MG_BEGIN

using EntityId                   = uint32_t;
const uint32_t INVALID_ENTITY_ID = 0;

using ComponentTypeId                           = uint8_t;
const ComponentTypeId INVALID_COMPONENT_TYPE_ID = 0;

const uint32_t MAX_SIGNATURES = 255;
using Signature               = std::bitset<MAX_SIGNATURES>;

NS_MG_END

#define MG_ECS_ENABLE_LOG 1

#if MG_ECS_ENABLE_LOG
#    define MG_ECS_LOG(...) printf(__VA_ARGS__)
#else
#    define MG_ECS_LOG(...)
#endif

#define MG_ECS_OBJECT_GC_LOG_ENABLED 0

#if MG_ECS_OBJECT_GC_LOG_ENABLED
#    define MG_ECS_OBJECT_GC_LOG(...) printf(__VA_ARGS__)
#else
#    define MG_ECS_OBJECT_GC_LOG(...)
#endif
