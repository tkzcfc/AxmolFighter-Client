#pragma once

#include "mugen/core/StdC.h"

#include <cstdint>
#include <vector>

NS_MG_BEGIN

class AvatarComponent;
class ActionAttackConfig;
class SkillAttackConfig;

// 动作时间轴：按 skill.primaryActionIds 推进 action_attack。
// effect/位移/连段取消窗消费留给后续阶段；本类负责播 Spine 动画、打断帧标记与链推进。
class ActionTimelinePlayer
{
public:
    enum class Status : int8_t
    {
        Idle = 0,
        Playing,
        Finished,
    };

    ActionTimelinePlayer() = default;

    // 开始播放技能主链；失败返回 false（无配置或无动作）
    bool start(int32_t skillAttackId, AvatarComponent* avatar);

    // 每帧推进；返回 true 表示整条技能链已结束
    bool tick(int32_t dtMs, AvatarComponent* avatar);

    void stop(AvatarComponent* avatar);

    Status getStatus() const { return m_status; }
    int32_t getSkillAttackId() const { return m_skillAttackId; }
    int32_t getCurrentActionId() const { return m_currentActionId; }
    int32_t getActionIndex() const { return m_actionIndex; }
    bool isInterruptOpen() const { return m_interruptOpen; }
    bool isInterruptExtraOpen() const { return m_interruptExtraOpen; }
    int32_t getElapsedMs() const { return m_elapsedMs; }

    // 动画名解析：action 数字 → Spine 动画名（目前为十进制字符串）
    static std::string resolveAnimationName(int32_t actionAnimId);

private:
    bool enterAction(int32_t actionId, AvatarComponent* avatar);
    void advanceOrFinish(AvatarComponent* avatar);
    int32_t estimateActionDurationMs(const ActionAttackConfig& cfg) const;

    Status m_status               = Status::Idle;
    int32_t m_skillAttackId       = 0;
    int32_t m_currentActionId     = 0;
    int32_t m_actionIndex         = -1;
    int32_t m_elapsedMs           = 0;
    int32_t m_estimatedDurationMs = 0;
    int32_t m_actionDelayMs       = 0;
    float m_frameIntervalMs       = 16.666f;
    bool m_interruptOpen          = false;
    bool m_interruptExtraOpen     = false;

    std::vector<int32_t> m_actionIds;
    const SkillAttackConfig* m_skillCfg = nullptr;
};

NS_MG_END
