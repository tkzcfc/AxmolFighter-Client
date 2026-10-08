#pragma once

#include "../reflect/Reflect.h"

namespace mugen
{
struct Entity
{
    uint32_t value = 0;

    static constexpr uint32_t kIndexBits     = 20;
    static constexpr uint32_t kIndexMask     = (1U << kIndexBits) - 1;
    static constexpr uint32_t kMaxGeneration = (1U << 12) - 1;

    constexpr uint32_t index() const { return value & kIndexMask; }
    constexpr uint32_t generation() const { return value >> kIndexBits; }
    constexpr bool operator==(const Entity&) const = default;

    MG_REFLECT(Entity, value)
};
static_assert(sizeof(Entity) == sizeof(uint32_t));
}  // namespace mugen
