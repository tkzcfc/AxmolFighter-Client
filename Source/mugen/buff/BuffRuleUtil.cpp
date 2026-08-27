#include "mugen/buff/BuffRuleUtil.h"

#include "mugen/buff/BFEvent.h"
#include "mugen/buff/Buff.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"
#include "mugen/skill/SkillManager.h"
#include "mugen/system/EffectLifeSystem.h"

#include <algorithm>

NS_MG_BEGIN

namespace BuffRuleUtil
{

float param(const BuffConfig* cfg, size_t index, float fallback)
{
    if (!cfg || index >= cfg->paramValue.size())
        return fallback;
    return cfg->paramValue[index];
}

int32_t toMsIfSeconds(int32_t value)
{
    if (value <= 0)
        return 0;
    return value < 100 ? value * 1000 : value;
}

int32_t intervalMs(const BuffConfig* cfg)
{
    if (!cfg)
        return 0;
    return toMsIfSeconds(cfg->interval);
}

int32_t durationMs(const BuffConfig* cfg)
{
    if (!cfg)
        return 0;
    const int32_t interval = intervalMs(cfg);
    if (cfg->times < 0)
        return 0;
    if (cfg->times == 0)
        return interval;
    if (interval > 0)
        return cfg->times * interval;
    return 0;
}

bool passTriggerGates(Entity* entity, Buff& inst, const BuffConfig* cfg, int32_t skillId)
{
    (void)entity;
    if (!cfg)
        return true;

    if (cfg->binding != 0)
    {
        const int32_t bindId = inst.sourceSkillId;
        if (bindId > 0 && skillId > 0 && bindId != skillId)
            return false;
    }

    if (inst.innerCdMs > 0)
        return false;

    if (cfg->probability < 100)
    {
        Random rng(static_cast<uint64_t>(inst.buffId) ^ static_cast<uint64_t>(inst.remainingMs) ^
                   static_cast<uint64_t>(skillId));
        if (rng.nextInt(1, 100) > cfg->probability)
            return false;
    }

    if (cfg->innerCd > 0)
        inst.innerCdMs = toMsIfSeconds(cfg->innerCd);

    return true;
}

void modifyExtend(Entity* entity, ExtendAttributeType type, float delta)
{
    if (!entity || delta == 0.0f)
        return;
    if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
        attr->extendAttribute.modify(type, delta);
}

Skill* findSkill(Entity* entity, int32_t skillAttackId)
{
    auto* mgr = SkillManager::of(entity);
    return mgr ? mgr->findSkill(skillAttackId) : nullptr;
}

void modifyMp(Entity* entity, float delta)
{
    auto* attr = entity ? MG_GET_COMPONENT(entity, AttributeComponent) : nullptr;
    if (!attr || delta == 0.0f)
        return;
    float mp          = attr->mp + delta;
    const float maxMp = (std::max)(0.0f, attr->mpMax);
    if (mp < 0.0f)
        mp = 0.0f;
    if (mp > maxMp)
        mp = maxMp;
    attr->mp = mp;
}

void applyHpDelta(Entity* entity, float delta)
{
    auto* attr = entity ? MG_GET_COMPONENT(entity, AttributeComponent) : nullptr;
    if (!attr || delta == 0.0f)
        return;
    if (attr->hp <= 0.0f)
        return;
    const float oldHp = attr->hp;
    float hp          = oldHp + delta;
    const float maxHp = (std::max)(1.0f, attr->basic.hpMax);
    if (hp < 0.0f)
        hp = 0.0f;
    if (hp > maxHp)
        hp = maxHp;
    attr->hp = hp;
    if (hp > 0.0f && hp != oldHp)
    {
        if (auto* mgr = BuffManager::of(entity))
            mgr->trigger(entity, BFEvent::HpChange, nullptr, 0, hp - oldHp);
    }
}

int32_t mappedBehaviorState(int32_t behaviorKind)
{
    switch (static_cast<BehaviorKind>(behaviorKind))
    {
    case BehaviorKind::kGetUp:
        return 1;
    case BehaviorKind::kStun:
    case BehaviorKind::kHitSwitch:
        return 2;
    case BehaviorKind::kHitUp:
    case BehaviorKind::kHitDown:
        return 3;
    default:
        return 0;
    }
}

void notifyBehaviorKindChange(Entity* entity, int32_t oldKind, int32_t newKind)
{
    const int32_t oldType = mappedBehaviorState(oldKind);
    const int32_t newType = mappedBehaviorState(newKind);
    if (oldType == newType)
        return;
    auto* mgr = BuffManager::of(entity);
    if (!mgr)
        return;
    if (oldType > 0)
        mgr->trigger(entity, BFEvent::BehaviorStateEnd, nullptr, oldType);
    if (newType > 0)
        mgr->trigger(entity, BFEvent::BehaviorStateStart, nullptr, newType);
}

void addBuffToTarget(Entity* holder, Entity* other, int32_t buffId, int32_t sourceSkillId)
{
    if (buffId <= 0)
        return;
    const auto* cfg = Config::getInstance()->getBuffConfigById(buffId);
    Entity* dest    = holder;
    if (cfg && cfg->target == 2 && other)
        dest = other;
    else if (cfg && cfg->target == 8 && other)
        dest = other;
    if (!dest)
        return;
    if (auto* mgr = BuffManager::of(dest))
        mgr->addBuff(dest, buffId, sourceSkillId);
}

void addBuffIds(Entity* target, const std::vector<int32_t>& ids, int32_t sourceSkillId)
{
    if (!target)
        return;
    auto* mgr = BuffManager::of(target);
    if (!mgr)
        return;
    for (int32_t id : ids)
    {
        if (id > 0)
            mgr->addBuff(target, id, sourceSkillId);
    }
}

void attachSpine(Entity* entity, Buff& inst)
{
    if (!entity || inst.vfxEntityId != 0)
        return;
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!cfg || cfg->spineId <= 0)
        return;
    const auto* spine = Config::getInstance()->getResSpineConfigById(cfg->spineId);
    if (!spine)
        return;
    auto* ecs = entity->getECSManager();
    if (!ecs)
        return;

    auto* ownerTf = MG_GET_COMPONENT(entity, TransformComponent);
    const Vector3f rel(cfg->spineOffsets.size() > 0 ? cfg->spineOffsets[0] : 0.0f,
                       cfg->spineOffsets.size() > 1 ? cfg->spineOffsets[1] : 0.0f, 0.0f);
    Entity* vfx = EffectLifeSystem::spawnVisual(ecs, cfg->spineId, entity->getId(), ownerTf, true, 0, rel);
    if (vfx)
        inst.vfxEntityId = static_cast<int32_t>(vfx->getId());
}

void detachSpine(Entity* entity, Buff& inst)
{
    if (inst.vfxEntityId == 0)
        return;
    if (entity)
    {
        if (auto* ecs = entity->getECSManager())
            ecs->destroyEntityById(static_cast<EntityId>(inst.vfxEntityId));
    }
    inst.vfxEntityId = 0;
}

}  // namespace BuffRuleUtil

NS_MG_END
