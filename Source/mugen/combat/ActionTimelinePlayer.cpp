#include "ActionTimelinePlayer.h"

#include "mugen/component/AvatarComponent.h"
#include "mugen/conf/Config.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace
{
constexpr int32_t kDefaultActionDurationMs = 500;
constexpr float kLogicFrameMs              = 1000.0f / 30.0f;
}  // namespace

std::string ActionTimelinePlayer::resolveAnimationName(int32_t actionAnimId)
{
    return std::to_string(actionAnimId);
}

bool ActionTimelinePlayer::start(int32_t skillAttackId, AvatarComponent* avatar)
{
    stop(avatar);

    auto* cfg = Config::getInstance()->getSkillAttackConfigById(skillAttackId);
    if (!cfg || cfg->primaryActionIds.empty())
    {
        MG_LOG_E("ActionTimelinePlayer: skillAttack {} missing or empty primaryActionIds", skillAttackId);
        return false;
    }

    m_skillCfg      = cfg;
    m_skillAttackId = skillAttackId;
    m_actionIds     = cfg->primaryActionIds;
    m_actionIndex   = -1;
    m_status        = Status::Playing;
    m_interruptOpen = false;

    return enterAction(m_actionIds.front(), avatar);
}

void ActionTimelinePlayer::stop(AvatarComponent* avatar)
{
    if (avatar)
        avatar->animationSpeed = 1.0f;

    m_status              = Status::Idle;
    m_skillAttackId       = 0;
    m_currentActionId     = 0;
    m_actionIndex         = -1;
    m_elapsedMs           = 0;
    m_estimatedDurationMs = 0;
    m_actionDelayMs       = 0;
    m_frameIntervalMs     = kLogicFrameMs;
    m_interruptOpen       = false;
    m_actionIds.clear();
    m_skillCfg = nullptr;
}

bool ActionTimelinePlayer::enterAction(int32_t actionId, AvatarComponent* avatar)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg)
    {
        MG_LOG_E("ActionTimelinePlayer: actionAttack {} not found", actionId);
        m_status = Status::Finished;
        return false;
    }

    ++m_actionIndex;
    m_currentActionId = actionId;
    m_elapsedMs       = 0;
    m_interruptOpen   = false;
    m_actionDelayMs   = std::max(0, actionCfg->actionDelayTime);

    const float scale     = actionCfg->actionScaleTime > 0.0f ? actionCfg->actionScaleTime : 1.0f;
    m_frameIntervalMs     = kLogicFrameMs / scale;
    m_estimatedDurationMs = estimateActionDurationMs(*actionCfg);

    if (avatar)
    {
        const std::string animName = resolveAnimationName(actionCfg->action);
        const bool loop            = (actionCfg->loop == 0 || actionCfg->loop > 1);
        avatar->animationSpeed     = scale;
        avatar->play(animName, loop ? -1 : 1, true);

        // 逻辑层若无 .box，用估算时长驱动 animationFinished
        if (avatar->playback.getDurationMs() <= 0 && m_estimatedDurationMs > 0)
            avatar->playback.setDurationMs(m_estimatedDurationMs);

        MG_LOG_W("ActionTimelinePlayer: skill={} action[{}]={} anim={} scale={:.2f} dur≈{}ms", m_skillAttackId,
                 m_actionIndex, actionId, animName, scale, m_estimatedDurationMs);
    }

    return true;
}

int32_t ActionTimelinePlayer::estimateActionDurationMs(const ActionAttackConfig& cfg) const
{
    const float scale = cfg.actionScaleTime > 0.0f ? cfg.actionScaleTime : 1.0f;
    int32_t ms        = cfg.actionDelayTime;

    if (cfg.interruptFrame > 0)
    {
        const int32_t byFrame = static_cast<int32_t>(std::lround(cfg.interruptFrame * (kLogicFrameMs / scale)));
        ms                    = std::max(ms, byFrame);
    }

    if (ms <= 0)
        ms = kDefaultActionDurationMs;

    return ms;
}

void ActionTimelinePlayer::advanceOrFinish(AvatarComponent* avatar)
{
    const int32_t next = m_actionIndex + 1;
    if (next >= 0 && next < static_cast<int32_t>(m_actionIds.size()))
    {
        enterAction(m_actionIds[static_cast<size_t>(next)], avatar);
        return;
    }

    m_status = Status::Finished;
    if (avatar)
        avatar->animationSpeed = 1.0f;
}

bool ActionTimelinePlayer::tick(int32_t dtMs, AvatarComponent* avatar)
{
    if (m_status != Status::Playing)
        return m_status == Status::Finished;

    if (dtMs < 0)
        dtMs = 0;
    m_elapsedMs += dtMs;

    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(m_currentActionId);
    if (actionCfg && actionCfg->interruptFrame >= 0 && m_frameIntervalMs > 0.0f)
    {
        const float frame = static_cast<float>(m_elapsedMs) / m_frameIntervalMs;
        if (frame >= static_cast<float>(actionCfg->interruptFrame))
            m_interruptOpen = true;
    }

    // effectFrames 触发放后置（阶段 D）

    const bool animFinished = avatar && avatar->animationFinished;
    const bool delayMet     = m_elapsedMs >= m_actionDelayMs;
    const bool fallbackDone = m_elapsedMs >= m_estimatedDurationMs;

    if ((animFinished && delayMet) || fallbackDone)
        advanceOrFinish(avatar);

    return m_status == Status::Finished;
}

NS_MG_END
