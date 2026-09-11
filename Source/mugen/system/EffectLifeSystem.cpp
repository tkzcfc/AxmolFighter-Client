#include "EffectLifeSystem.h"

#include "mugen/buff/BuffManager.h"
#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/bt/EffectTreeBuilder.h"
#include "mugen/avatar/AvatarLayerUtils.h"
#include "mugen/component/AttributeComponent.h"
#include "mugen/component/AvatarComponent.h"
#include "mugen/component/AvatarRenderComponent.h"
#include "mugen/component/BehaviorComponent.h"
#include "mugen/component/BehaviorTreeComponent.h"
#include "mugen/component/EffectComponent.h"
#include "mugen/component/IdentityComponent.h"
#include "mugen/component/SoundComponent.h"
#include "mugen/component/TransformComponent.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/effect/Effect.h"
#include "mugen/skill/SkillManager.h"

#include <vector>

NS_MG_BEGIN

namespace
{
std::string replaceExtension(const std::string& path, const std::string& newExt)
{
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + newExt;
    return path.substr(0, dot) + newExt;
}

void copyTransform(TransformComponent& dst, const TransformComponent& src)
{
    dst.position        = src.position;
    dst.scale           = src.scale;
    dst.facingDirection = src.facingDirection;
}

void attachSpineAvatar(Entity* effect, const ResSpineConfig* spine)
{
    if (!effect || !spine)
        return;
    auto* avatar = MG_ADD_COMPONENT(effect, AvatarComponent);
    MG_ADD_COMPONENT(effect, AvatarRenderComponent);
    if (!avatar)
        return;
    avatar->resSpine = spine;
    if (spine->spine.empty())
        return;
    avatar->spineSkeleton = spine->spine;
    avatar->spineAtlas    = replaceExtension(spine->spine, ".atlas");
    avatar->defaultSkin.clear();
    avatar->spineScale = spine->scale > 0.0f ? spine->scale : 1.0f;
    avatar->motionFile = AvatarLayerUtils::spinePathToMotionFile(spine->spine);
}

void seedEffectIdentity(Entity* effect, EntityId ownerId)
{
    auto* attr = MG_ADD_COMPONENT(effect, AttributeComponent);
    if (attr)
    {
        attr->hp = 1.0f;
    }
    MG_ADD_COMPONENT(effect, BehaviorComponent);
    MG_ADD_COMPONENT(effect, SoundComponent);
    if (auto* id = MG_ADD_COMPONENT(effect, IdentityComponent))
    {
        id->category       = EntityCategory::kSkillEffect;
        id->belongEntityId = ownerId;
    }
}

void attachEffectTree(Entity* effect)
{
    auto* fxBt = MG_ADD_COMPONENT(effect, BehaviorTreeComponent);
    if (!fxBt)
        return;
    fxBt->treeKind = 2;
    fxBt->ensureTree();
    EffectTreeBuilder::attachToEntity(effect);
}

void stampOwnerSkill(Effect* fx, ECSManager* ecs, EntityId ownerId)
{
    if (!fx || !ecs || ownerId == INVALID_ENTITY_ID)
        return;
    Entity* owner = ecs->getEntity(ownerId);
    if (!owner)
        return;
    auto* mgr = SkillManager::of(owner);
    if (!mgr)
        return;
    fx->baseSkillId = mgr->activeSkillAttackId;
    fx->slotIndex   = mgr->activeInputSlot;
}

void prepareEffect(Entity* entity)
{
    auto* comp = MG_GET_COMPONENT(entity, EffectComponent);
    if (!comp)
        return;
    comp->ensureEffect();
    comp->restoreRuntimeData();
}

void placeAtOrigin(TransformComponent* tf, const TransformComponent* originTf, const Vector3f& rel)
{
    if (!tf || !originTf)
        return;
    const float facing  = originTf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
    tf->position.x      = originTf->position.x + static_cast<int32_t>(rel.x * facing);
    tf->position.y      = originTf->position.y + static_cast<int32_t>(rel.y);
    tf->position.z      = originTf->position.z + static_cast<int32_t>(rel.z);
    tf->facingDirection = originTf->facingDirection;
}
}  // namespace

EffectLifeSystem::EffectLifeSystem() {}
EffectLifeSystem::~EffectLifeSystem() {}

void EffectLifeSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, EffectComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, TransformComponent);
}

void EffectLifeSystem::onEntityAdded(Entity* entity)
{
    prepareEffect(entity);
}

Entity* EffectLifeSystem::spawnEffect(ECSManager* ecs,
                                      int32_t effectId,
                                      EntityId ownerId,
                                      const TransformComponent* originTf,
                                      int32_t skillHitLookupId,
                                      bool chainFromParent)
{
    if (!ecs || effectId <= 0)
        return nullptr;
    const auto* cfg = Config::getInstance()->getEffectConfigById(effectId);
    if (!cfg)
        return nullptr;

    auto* effect = ecs->newEntity();
    auto* tf     = MG_ADD_COMPONENT(effect, TransformComponent);
    MG_ADD_COMPONENT(effect, EffectComponent);

    auto* fx = Effect::of(effect);
    if (fx)
    {
        fx->bindFromConfig(cfg, ownerId, skillHitLookupId, chainFromParent, originTf);
        stampOwnerSkill(fx, ecs, ownerId);
    }

    placeAtOrigin(tf, originTf, cfg->relativePosition);

    if (cfg->resSpineId > 0)
    {
        if (const auto* spine = Config::getInstance()->getResSpineConfigById(cfg->resSpineId))
            attachSpineAvatar(effect, spine);
    }

    seedEffectIdentity(effect, ownerId);
    attachEffectTree(effect);
    effect->notifyEntityReady();

    if (Entity* owner = ecs->getEntity(ownerId))
        BuffRuleUtil::addBuffIds(owner, cfg->buffId, skillHitLookupId);

    return effect;
}

Entity* EffectLifeSystem::spawnVisual(ECSManager* ecs,
                                      int32_t resSpineId,
                                      EntityId ownerId,
                                      const TransformComponent* originTf,
                                      bool follow,
                                      int32_t lifetimeMs,
                                      const Vector3f& relativePosition)
{
    if (!ecs)
        return nullptr;

    auto* effect = ecs->newEntity();
    auto* tf     = MG_ADD_COMPONENT(effect, TransformComponent);
    MG_ADD_COMPONENT(effect, EffectComponent);

    if (auto* fx = Effect::of(effect))
        fx->bindVisual(ownerId, follow, lifetimeMs, relativePosition);

    placeAtOrigin(tf, originTf, relativePosition);
    if (originTf && relativePosition.x == 0.0f && relativePosition.y == 0.0f && relativePosition.z == 0.0f)
    {
        tf->position        = originTf->position;
        tf->facingDirection = originTf->facingDirection;
    }

    if (resSpineId > 0)
    {
        if (const auto* spine = Config::getInstance()->getResSpineConfigById(resSpineId))
            attachSpineAvatar(effect, spine);
    }

    seedEffectIdentity(effect, ownerId);
    attachEffectTree(effect);
    effect->notifyEntityReady();
    return effect;
}

void EffectLifeSystem::spawnHitEffects(Entity* effectEntity, Entity* hitTarget)
{
    if (!effectEntity || !hitTarget)
        return;
    auto* fx  = Effect::of(effectEntity);
    auto* ecs = effectEntity->getECSManager();
    if (!fx || !ecs || fx->effectId <= 0)
        return;
    const auto* cfg = Config::getInstance()->getEffectConfigById(fx->effectId);
    if (!cfg)
        return;

    auto* targetTf = MG_GET_COMPONENT(hitTarget, TransformComponent);
    auto* effectTf = MG_GET_COMPONENT(effectEntity, TransformComponent);
    TransformComponent origin;
    if (targetTf)
        copyTransform(origin, *targetTf);
    else if (effectTf)
        copyTransform(origin, *effectTf);

    for (int32_t hitFxId : cfg->hitEffectIds)
    {
        if (hitFxId <= 0)
            continue;
        spawnEffect(ecs, hitFxId, fx->ownerId, &origin, fx->skillHitId, true);
    }
}

void EffectLifeSystem::update()
{
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    auto* ecs          = getECSManager();
    // nextEffect / 命中特效会 spawn 并 notifyEntityReady，不能边遍历边改 entities
    const std::vector<Entity*> ticking = entities;
    std::vector<Entity*> toDestroy;
    for (Entity* entity : ticking)
    {
        if (!entity || entity->isPendingRemoval())
            continue;
        prepareEffect(entity);
        auto* fx = Effect::of(entity);
        if (!fx)
            continue;

        if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
        {
            if (attr->freezeRemainingMs > 0 && attr->freezeDelayMs <= 0)
                continue;
        }

        if (fx->update(entity, dtMs))
            toDestroy.push_back(entity);
    }
    for (Entity* e : toDestroy)
        ecs->destroyEntity(e);
}

NS_MG_END
