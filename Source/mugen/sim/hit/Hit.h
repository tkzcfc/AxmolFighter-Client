#pragma once
#include "mugen/sim/role/RoleComponents.h"
#include "Damage.h"

namespace mugen::sim
{
// 命中特效所处的世界实例。
class World;
namespace hit
{
// 伤害许可在配置三元组中的位置。
enum class HurtPermit : uint8_t { Attack, Hp, Number };
// 解析召唤物最终归属；不存在归属时返回空句柄。
Entity finalOwner(World&, Entity);
// 按角色类型和等级读取标准伤害。
Fixed standardHurt(World&, Entity);
// 从特效和双方实体解析公式输入。
bool damageInput(World&, Entity effect, Entity target, DamageInput&);
// 按生命或机关剩余命中次数判断死亡。
bool isDead(World&, Entity);
// 查询配置中的伤害许可。
bool checkHurtPermit(World&, Entity target, Entity effect, HurtPermit);
// 护盾吸收后返回仍需作用于生命的负数变化。
Fixed calcShieldAndHp(World&, Entity, Fixed delta);
// 修改生命、同步派发免死事件，并处理死亡入口。
void modifyHp(World&, Entity, Fixed delta, Entity effect = {}, bool noShield = false);
// 处理特效碰撞状态变化。
void onEffectCollision(World& world, Entity effect, Entity other, CollisionEvent event, FixedVec2 contact);
}
}
