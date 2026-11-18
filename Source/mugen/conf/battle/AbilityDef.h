#pragma once

#include "mugen/conf/battle/BuffDef.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace mugen::conf::battle
{
// 特殊能力的表现透传。
struct AbilityPresentation
{
    // Spine 资源。
    int32_t spineId = 0;
    // Spine 间隔。
    int32_t spineInterval = 0;
    // Spine 偏移。
    std::vector<int32_t> spineOffsets;
    // Spine 步骤。
    std::vector<int32_t> spineStep;
    // 音效 id。
    int32_t resSoundId = 0;
    // 是否弹提示。
    int32_t showTips = 0;
    // 图标：整数或资源路径。
    std::variant<int32_t, std::string> icon;
    // 名称文本 id。
    int32_t nameId = 0;
    // 描述文本 id。
    int32_t descId = 0;

    MG_REFLECT(AbilityPresentation,
               spineId,
               spineInterval,
               spineOffsets,
               spineStep,
               resSoundId,
               showTips,
               icon,
               nameId,
               descId)
};

// 一行特殊能力。与 Buff 共用触发点、谓词与 op。
struct AbilityDef
{
    // 配置行 id。查找键为 KeyById。
    int32_t id = 0;
    // 降级状态。整行 op 都是空作用时为 Noop。
    LowerStatus status = LowerStatus::Lowered;
    // 触发点（源 event 已映射到 TriggerPoint）。
    std::vector<TriggerPoint> events;
    // 准入谓词。空表示无条件。
    std::vector<Predicate> conditions;
    // 触发概率原值。
    Fixed probability;
    // 冷却（秒）。
    Fixed cd;
    // PVP 冷却（秒）。
    Fixed cdPvp;
    // 周期（秒）；负值表示没有周期。
    Fixed intervalSec;
    // 结束方式：0 不结束，1 倒计时。
    int32_t endedType = 0;
    // 倒计时时长（秒）。
    Fixed endedValue;
    // PVP 设置：0 在 PVP 中禁用，大于 1 时替换为该 id。
    int32_t pvpSetting = 0;
    // 优先级。
    int32_t priority = 0;
    // 子类型；−1 表示始终新建。
    int32_t subType = 0;
    // 源规则 id。
    int32_t ruleId = 0;
    // 绑定技能类型；非 0 时要求事件技能匹配。
    int32_t bindSkillType = 0;
    // 源数值参数。
    int32_t paramValue = 0;
    // 激活时执行的 op（生成特效、施加 Buff）。
    std::vector<BuffOp> ops;
    // 表现透传。
    AbilityPresentation presentation;

    MG_REFLECT(AbilityDef,
               id,
               status,
               events,
               conditions,
               probability,
               cd,
               cdPvp,
               intervalSec,
               endedType,
               endedValue,
               pvpSetting,
               priority,
               subType,
               ruleId,
               bindSkillType,
               paramValue,
               ops,
               presentation)
};
}  // namespace mugen::conf::battle
