#pragma once

#include "../reflect/Reflect.h"

namespace mugen
{
// 低 20 位为索引、高 12 位为代数的可序列化实体句柄。
struct Entity
{
    // 打包后的索引与代数；零表示无效句柄。
    uint32_t value = 0;

    // 句柄中索引字段的位数。
    static constexpr uint32_t kIndexBits = 20;
    // 索引字段掩码。
    static constexpr uint32_t kIndexMask = (1U << kIndexBits) - 1;
    // 代数达到此值后槽位永久退役。
    static constexpr uint32_t kMaxGeneration = (1U << 12) - 1;

    constexpr uint32_t index() const { return value & kIndexMask; }
    constexpr uint32_t generation() const { return value >> kIndexBits; }
    constexpr bool operator==(const Entity&) const = default;

    MG_REFLECT(Entity, value)
};
static_assert(sizeof(Entity) == sizeof(uint32_t));
}  // namespace mugen
