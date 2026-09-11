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

    // 加载 MotionMap；路径为空或加载失败则失败
    bool init(const AvatarLayerDef& def);

    // 切换动作并加载 .box；无 map / 无 entry 则失败
    bool setMotion(const std::string& motionName, const std::string& entryId);

    // 清空当前 .box
    void clearBox();

    // 当前 .box 时长（毫秒），无盒为 0
    int durationMs() const;

    // 采样攻/受盒（追加到出参）
    void boxesAt(int timeMs, std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;

    // 收集 [t0, t1) 事件（追加到出参）
    void eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const;

    // 层来源 tag
    AvatarLayerTag getTag() const { return m_def.tag; }

    const MotionMap* motionMap() const { return m_motionMap.get(); }

    // 层静态描述
    MG_SYNTHESIZE_READONLY_BY_REF(AvatarLayerDef, m_def, Def)

private:
    // 本层 MotionMap
    std::shared_ptr<const MotionMap> m_motionMap;
    // 当前动作 .box（可空）
    std::shared_ptr<const CombatTimeline> m_box;
};

NS_MG_END
