#pragma once

#include "mugen/avatar/AvatarLayerDef.h"
#include "mugen/avatar/data/CombatTimeline.h"
#include "mugen/avatar/data/MotionMap.h"

#include <memory>
#include <string>
#include <vector>

NS_MG_BEGIN

// 逻辑层：motionName → .box（时长 / 事件 / 碰撞盒）
class MotionLayer
{
public:
    MotionLayer() = default;

    // 加载 MotionMap；空路径表示直通（无 .motion）
    bool init(const AvatarLayerDef& def);

    // 切换动作并加载 .box；找不到 entry 时按 motionName 自动找 .box
    bool setMotion(const std::string& motionName, const std::string& entryId);

    // 清空当前 .box
    void clearMotion();

    // 当前 .box 时长（毫秒），无盒为 0
    int durationMs() const;

    // 采样攻/受盒（追加到出参）
    void boxesAt(int timeMs, std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;

    // 收集 [t0, t1) 事件（追加到出参）
    void eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const;

    // 层来源 tag
    int32_t getTag() const { return m_def.tag; }

    // 层静态描述
    MG_SYNTHESIZE_READONLY_BY_REF(AvatarLayerDef, m_def, Def)

private:
    // 本层 MotionMap
    std::shared_ptr<const MotionMap> m_motionMap;
    // 当前动作 .box（可空）
    std::shared_ptr<const CombatTimeline> m_box;
};

NS_MG_END
