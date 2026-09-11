#pragma once

#include "mugen/avatar/AvatarLayerDef.h"
#include "mugen/avatar/data/MotionMap.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 逻辑层：motionName → 串接的 .box（时长 / 事件 / 碰撞盒）
class MotionLayer
{
public:
    MotionLayer() = default;

    // 加载 MotionMap；路径为空或加载失败则失败
    bool init(const AvatarLayerDef& def);

    // 绑定 motion；无 map / 无 entry / 缺盒则失败
    bool setMotion(const std::string& motionName, const std::string& entryId);

    // 清空当前 motion
    void clearBox();

    // 全部 clip 时长之和（毫秒）
    int durationMs() const;

    // entryId 对应 clip 的全局起点（空 entryId 为 0）
    int startTimeMs() const { return m_startMs; }

    // 采样攻/受盒（追加到出参）
    void boxesAt(int timeMs, std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;

    // 收集 [t0, t1) 事件（追加到出参）
    void eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const;

    // 层来源 tag
    AvatarLayerTag getTag() const { return m_def.tag; }

    const MotionMap* motionMap() const { return m_motionMap; }

    // 层静态描述
    MG_SYNTHESIZE_READONLY_BY_REF(AvatarLayerDef, m_def, Def)

private:
    const MotionMap* m_motionMap = nullptr;
    const Motion* m_motion       = nullptr;
    int m_startMs                = 0;
};

NS_MG_END
