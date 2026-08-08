#pragma once

#include "mugen/combat/ActionTimelinePlayer.h"
#include "mugen/conf/TableConfig.h"

NS_MG_BEGIN

class AvatarComponent;
class DisplacementComponent;
class BuffComponent;
class Entity;

/**
 * 技能动作执行器：在 ActionTimelinePlayer 基础上驱动
 * 位移、特效帧、取消窗口、镜头、buff。
 */
class ActionRunner
{
public:
    enum class Status : int32_t
    {
        Idle = 0,
        Playing,
        Finished,
    };

    bool start(int32_t skillAttackId,
               Entity* owner,
               AvatarComponent* avatar,
               DisplacementComponent* displacement,
               BuffComponent* buffs);
    void stop(Entity* owner, AvatarComponent* avatar, DisplacementComponent* displacement, BuffComponent* buffs);
    /** @return true 当本技能结束 */
    bool tick(int32_t dtMs,
              Entity* owner,
              AvatarComponent* avatar,
              DisplacementComponent* displacement,
              BuffComponent* buffs);

    Status getStatus() const { return m_status; }
    int32_t getSkillAttackId() const { return m_skillAttackId; }
    bool isInterruptOpen() const { return m_timeline.isInterruptOpen(); }
    bool isInterruptExtraOpen() const { return m_interruptExtraOpen; }
    int32_t getCurrentActionId() const { return m_timeline.getCurrentActionId(); }

private:
    void applyBuffs(const ActionAttackConfig& cfg, BuffComponent* buffs, bool add);

    ActionTimelinePlayer m_timeline;
    Status m_status           = Status::Idle;
    int32_t m_skillAttackId   = 0;
    Entity* m_owner           = nullptr;
    bool m_interruptExtraOpen = false;
    int32_t m_elapsedMs       = 0;
    std::vector<bool> m_effectSpawned;
};

NS_MG_END
