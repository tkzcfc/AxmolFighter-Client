#include "EffectLifeSystem.h"

#include "mugen/component/EffectLifetimeComponent.h"
#include "mugen/component/AttributeComponent.h"
#include "mugen/component/AvatarComponent.h"
#include "mugen/component/AvatarRenderComponent.h"
#include "mugen/component/TransformComponent.h"
#include "mugen/conf/Config.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"

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
}  // namespace

EffectLifeSystem::EffectLifeSystem() {}
EffectLifeSystem::~EffectLifeSystem() {}

void EffectLifeSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, EffectLifetimeComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, TransformComponent);
}

Entity* EffectLifeSystem::spawnEffect(ECSManager* ecs,
                                      int32_t effectId,
                                      EntityId ownerId,
                                      const TransformComponent* originTf,
                                      int32_t skillHitId,
                                      bool chainFromParent)
{
    if (!ecs || effectId <= 0)
        return nullptr;
    const auto* cfg = Config::getInstance()->getEffectConfigById(effectId);
    if (!cfg)
        return nullptr;

    auto* effect = ecs->newEntity();
    auto* tf     = MG_ADD_COMPONENT(effect, TransformComponent);
    auto* fx     = MG_ADD_COMPONENT(effect, EffectLifetimeComponent);
    MG_ADD_COMPONENT(effect, AttributeComponent);

    fx->effectId   = effectId;
    fx->skillHitId = skillHitId > 0 ? skillHitId : (cfg->skillHitId > 0 ? cfg->skillHitId : 0);
    fx->ownerId    = ownerId;
    fx->chainSpawned = false;
    fx->lifetimeMs =
        cfg->autoRelease > 0 ? cfg->autoRelease : (chainFromParent ? 3000 : 60000);
    fx->follow           = cfg->follow != 0;
    fx->radius           = cfg->radius > 0 ? cfg->radius : 40.0f;
    fx->relativePosition = cfg->relativePosition;
    fx->moveVx           = 0.0f;
    fx->moveVy           = 0.0f;

    if (originTf)
    {
        const float facing = originTf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
        tf->position.x =
            originTf->position.x + static_cast<int32_t>(cfg->relativePosition.x * facing);
        tf->position.y = originTf->position.y + static_cast<int32_t>(cfg->relativePosition.y);
        tf->position.z = originTf->position.z + static_cast<int32_t>(cfg->relativePosition.z);
        tf->facingDirection = originTf->facingDirection;
        if (!fx->follow && cfg->velocity != 0.0f)
        {
            fx->moveVx = cfg->velocity * facing;
            fx->moveVy = 0.0f;
        }
    }

    if (cfg->resSpineId > 0)
    {
        if (const auto* spine = Config::getInstance()->getResSpineConfigById(cfg->resSpineId))
        {
            auto* avatar = MG_ADD_COMPONENT(effect, AvatarComponent);
            MG_ADD_COMPONENT(effect, AvatarRenderComponent);
            avatar->resSpine = spine;
            if (!spine->spine.empty())
            {
                avatar->spineSkeleton = spine->spine;
                avatar->spineAtlas =
                    !spine->atlas.empty() ? spine->atlas : replaceExtension(spine->spine, ".atlas");
                avatar->defaultSkin = spine->defaultSkin;
                avatar->spineScale  = spine->scale > 0.0f ? spine->scale : 1.0f;
                const auto slash    = spine->spine.find_last_of("/\\");
                avatar->defaultAnimationPath =
                    slash == std::string::npos ? std::string{} : spine->spine.substr(0, slash);
            }
        }
    }

    effect->notifyEntityReady();
    return effect;
}

void EffectLifeSystem::spawnHitEffects(Entity* effectEntity, Entity* hitTarget)
{
    if (!effectEntity || !hitTarget)
        return;
    auto* fx  = MG_GET_COMPONENT(effectEntity, EffectLifetimeComponent);
    auto* ecs = effectEntity->getECSManager();
    if (!fx || !ecs)
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
    for (Entity* entity : entities)
    {
        auto* fx = MG_GET_COMPONENT(entity, EffectLifetimeComponent);
        auto* tf = MG_GET_COMPONENT(entity, TransformComponent);
        if (!fx || !tf)
            continue;

        if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
        {
            if (attr->freezeRemainingMs > 0 && attr->freezeDelayMs <= 0)
                continue;
        }

        if (fx->follow && fx->ownerId != INVALID_ENTITY_ID)
        {
            if (auto* owner = ecs->getEntity(fx->ownerId))
            {
                if (auto* ownerTf = MG_GET_COMPONENT(owner, TransformComponent))
                {
                    const float facing =
                        ownerTf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
                    tf->position.x =
                        ownerTf->position.x + static_cast<int32_t>(fx->relativePosition.x * facing);
                    tf->position.y =
                        ownerTf->position.y + static_cast<int32_t>(fx->relativePosition.y);
                    tf->position.z =
                        ownerTf->position.z + static_cast<int32_t>(fx->relativePosition.z);
                    tf->facingDirection = ownerTf->facingDirection;
                }
            }
        }
        else if (!fx->follow)
        {
            // Derive ballistic from table if move not initialized (e.g. AttackAction spawn)
            if (fx->moveVx == 0.0f && fx->moveVy == 0.0f)
            {
                if (const auto* cfg = Config::getInstance()->getEffectConfigById(fx->effectId))
                {
                    if (cfg->velocity != 0.0f)
                    {
                        const float facing =
                            tf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
                        fx->moveVx = cfg->velocity * facing;
                    }
                }
            }
            if (fx->moveVx != 0.0f || fx->moveVy != 0.0f)
            {
                const float sec = dtMs / 1000.0f;
                tf->position.x += static_cast<int32_t>(fx->moveVx * sec);
                tf->position.y += static_cast<int32_t>(fx->moveVy * sec);
            }
        }

        fx->elapsedMs += dtMs;
        if (fx->lifetimeMs > 0 && fx->elapsedMs >= fx->lifetimeMs)
        {
            const auto* cfg = Config::getInstance()->getEffectConfigById(fx->effectId);
            if (!fx->chainSpawned && cfg && cfg->nextEffectId > 0)
            {
                fx->chainSpawned = true;
                spawnEffect(ecs, cfg->nextEffectId, fx->ownerId, tf, fx->skillHitId, true);
            }
            ecs->destroyEntity(entity);
        }
    }
}

NS_MG_END
