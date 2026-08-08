#include "ActionRunner.h"

#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"

#include <algorithm>

NS_MG_BEGIN

namespace
{
void spawnEffectEntity(Entity* owner, int32_t effectId, int32_t fallbackSkillHitId)
{
    if (!owner || effectId <= 0)
        return;
    auto* cfg = Config::getInstance()->getEffectConfigById(effectId);
    if (!cfg)
        return;

    auto* ecs = owner->getECSManager();
    if (!ecs)
        return;

    auto* ownerTf = MG_GET_COMPONENT(owner, TransformComponent);
    auto* effect  = ecs->newEntity();
    auto* tf      = MG_ADD_COMPONENT(effect, TransformComponent);
    auto* fx      = MG_ADD_COMPONENT(effect, EffectLifetimeComponent);

    fx->effectId         = effectId;
    fx->skillHitId       = cfg->skillHitId > 0 ? cfg->skillHitId : fallbackSkillHitId;
    fx->ownerId          = owner->getId();
    fx->lifetimeMs       = cfg->autoRelease > 0 ? cfg->autoRelease : 450;
    fx->follow           = cfg->follow != 0;
    fx->radius           = cfg->radius > 0 ? cfg->radius : 40.0f;
    fx->relativePosition = cfg->relativePosition;

    if (ownerTf)
    {
        const float facing  = ownerTf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
        tf->position.x      = ownerTf->position.x + static_cast<int32_t>(cfg->relativePosition.x * facing);
        tf->position.y      = ownerTf->position.y + static_cast<int32_t>(cfg->relativePosition.y);
        tf->position.z      = ownerTf->position.z + static_cast<int32_t>(cfg->relativePosition.z);
        tf->facingDirection = ownerTf->facingDirection;
    }

    effect->notifyEntityReady();
    MG_LOG_D("ActionRunner: spawned effect {} skillHit={} owner={}", effectId, fx->skillHitId, fx->ownerId);
}
}  // namespace

bool ActionRunner::start(int32_t skillAttackId,
                         Entity* owner,
                         AvatarComponent* avatar,
                         DisplacementComponent* displacement,
                         BuffComponent* buffs)
{
    stop(owner, avatar, displacement, buffs);
    if (!m_timeline.start(skillAttackId, avatar))
        return false;

    m_owner              = owner;
    m_skillAttackId      = skillAttackId;
    m_status             = Status::Playing;
    m_interruptExtraOpen = false;
    m_effectSpawned.clear();
    m_elapsedMs = 0;

    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(m_timeline.getCurrentActionId());
    if (actionCfg)
    {
        m_effectSpawned.assign(actionCfg->effectIds.size(), false);
        if (displacement && actionCfg->displacementId > 0)
        {
            if (auto* d = Config::getInstance()->getDisplacementConfigById(actionCfg->displacementId))
                displacement->start(d);
        }
        applyBuffs(*actionCfg, buffs, true);
    }
    return true;
}

void ActionRunner::stop(Entity* /*owner*/,
                        AvatarComponent* avatar,
                        DisplacementComponent* displacement,
                        BuffComponent* buffs)
{
    if (auto* actionCfg = Config::getInstance()->getActionAttackConfigById(m_timeline.getCurrentActionId()))
        applyBuffs(*actionCfg, buffs, false);

    m_timeline.stop(avatar);
    if (displacement)
        displacement->reset();
    m_status             = Status::Idle;
    m_skillAttackId      = 0;
    m_owner              = nullptr;
    m_interruptExtraOpen = false;
    m_elapsedMs          = 0;
    m_effectSpawned.clear();
}

bool ActionRunner::tick(int32_t dtMs,
                        Entity* owner,
                        AvatarComponent* avatar,
                        DisplacementComponent* displacement,
                        BuffComponent* buffs)
{
    if (m_status != Status::Playing)
        return m_status == Status::Finished;

    m_owner = owner;
    m_elapsedMs += dtMs;

    const int32_t prevActionId = m_timeline.getCurrentActionId();
    const bool finished        = m_timeline.tick(dtMs, avatar);

    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(m_timeline.getCurrentActionId());
    if (actionCfg)
    {
        if (m_timeline.getCurrentActionId() != prevActionId)
        {
            if (auto* prev = Config::getInstance()->getActionAttackConfigById(prevActionId))
                applyBuffs(*prev, buffs, false);
            m_effectSpawned.assign(actionCfg->effectIds.size(), false);
            m_interruptExtraOpen = false;
            m_elapsedMs          = 0;
            if (displacement)
            {
                displacement->reset();
                if (actionCfg->displacementId > 0)
                {
                    if (auto* d = Config::getInstance()->getDisplacementConfigById(actionCfg->displacementId))
                        displacement->start(d);
                }
            }
            applyBuffs(*actionCfg, buffs, true);
        }

        if (actionCfg->interruptExtraFrame >= 0 && m_timeline.isInterruptOpen())
            m_interruptExtraOpen = true;

        const float scale         = actionCfg->actionScaleTime > 0.0f ? actionCfg->actionScaleTime : 1.0f;
        const float frameInterval = (1000.0f / 30.0f) / scale;
        const float curFrame      = frameInterval > 0.0f ? static_cast<float>(m_elapsedMs) / frameInterval : 0.0f;

        for (size_t i = 0; i < actionCfg->effectIds.size() && i < m_effectSpawned.size(); ++i)
        {
            if (m_effectSpawned[i])
                continue;
            const int32_t ef = i < actionCfg->effectFrames.size() ? actionCfg->effectFrames[i] : 0;
            if (ef <= 0 || curFrame >= static_cast<float>(ef) || m_timeline.isInterruptOpen())
            {
                m_effectSpawned[i] = true;
                if (actionCfg->effectIds[i] > 0)
                    spawnEffectEntity(owner, actionCfg->effectIds[i], m_skillAttackId);
            }
        }
    }

    if (finished)
    {
        m_status = Status::Finished;
        if (displacement)
            displacement->reset();
    }
    return finished;
}

void ActionRunner::applyBuffs(const ActionAttackConfig& cfg, BuffComponent* buffs, bool add)
{
    if (!buffs)
        return;
    for (int32_t bid : cfg.buffIds)
    {
        if (bid <= 0)
            continue;
        if (add)
        {
            BuffInstance inst;
            inst.buffId        = bid;
            inst.remainingMs   = 3000;
            inst.sourceSkillId = m_skillAttackId;
            buffs->buffs.push_back(inst);
        }
        else
        {
            buffs->buffs.erase(std::remove_if(buffs->buffs.begin(), buffs->buffs.end(),
                                              [bid](const BuffInstance& b) { return b.buffId == bid; }),
                               buffs->buffs.end());
        }
    }
}

NS_MG_END
