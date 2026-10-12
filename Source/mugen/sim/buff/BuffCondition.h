#pragma once
#include "BuffComponents.h"

namespace mugen::sim
{
class World;
namespace buff
{
// 按配置顺序短路计算条件，返回真表示全部通过。
bool checkConditions(World& world, Entity entity, const BuffInstance& instance);
// 使用已解析配置计算条件，供重置和配置验证复用同一入口。
bool checkConditions(World& world, Entity entity, const BuffInstance& instance, const conf::BuffConfig& config);
}
}
