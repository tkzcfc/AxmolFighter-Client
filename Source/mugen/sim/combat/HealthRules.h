#pragma once
#include "mugen/sim/Events.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"

namespace mugen::sim
{
class World;
struct Health;
namespace health::rules
{
// 扣血或回血：夹到 [0, maxHp]，发出 HpEvent。跨过死亡线时立即 m_dead 并追加 DeathRequest（R6）。
// S2 的护盾、锁血、免死挂在写入之前。
void applyDelta(World& world, Entity entity, Fixed delta, HpCause cause, Entity source, Entity effect = {});
// 是否已判定死亡（m_dead 或生命耗尽且上限有效）。
bool isDead(const Health& health);
}  // namespace health::rules
}  // namespace mugen::sim
