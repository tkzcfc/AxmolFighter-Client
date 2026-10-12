#pragma once
#include "BuffComponents.h"

namespace mugen::sim
{
class World;
namespace buff
{
// 计算带符号的周期生命变化，不经过防御、暴击或随机浮动。
Fixed calculateBuffDamage(Fixed standardHurt, Fixed parameter, Fixed addition);
// 从施加者或受益者解析标准伤害并在取整后乘层数。
Fixed getAttackValue(World& world, Entity entity, const BuffInstance& instance);
// 通过统一生命入口结算周期变化，并发送伤害数字。
void dealBuffDamage(World& world, Entity entity, Entity attacker, Fixed attackValue);
}
}
