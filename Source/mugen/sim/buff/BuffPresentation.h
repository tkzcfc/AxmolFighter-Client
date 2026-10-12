#pragma once
#include "mugen/core/ecs/Entity.h"

namespace mugen::sim { class World; }

namespace mugen::sim::buff
{
// 创建或重建实例的出现与持续表现。
void beginPresentation(World& world, Entity entity, uint64_t serial);
// 出现定时器到期后请求持续表现，退出中的实例不会恢复显示。
void continuePresentation(World& world, Entity entity, uint64_t serial);
// 取消实例的出现阶段定时器。
void cancelAppearance(World& world, Entity entity, uint64_t serial);
// 返回已绑定逻辑动作的毫秒时长；缺失时返回空值而不是猜测。
std::optional<Fixed> disappearDuration(int32_t spineId);
// 请求实例退出，已解析的消失动作由逻辑定时器完成退出钩子。
void exitInstance(World& world, Entity entity, uint64_t serial);
// 定时器到期完成退出，序号避免误操作同配置的新实例。
void finishExit(World& world, Entity entity, uint64_t serial);
}
