#include "mugen/bt/actions/AttackAction.h"

#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/conf/Config.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"
#include "mugen/core/StdC.h"
#include "mugen/render/VirtualCamera.h"
#include "mugen/effect/Effect.h"
#include "mugen/skill/SkillManager.h"
#include "mugen/system/EffectLifeSystem.h"
#include "mugen/system/SoundSystem.h"

#ifdef RUNTIME_IN_AXMOL
#    include "mugen/avatar/AvatarLayerUtils.h"
#    include "mugen/avatar/render/Avatar.h"
#    include "mugen/avatar/render/AvatarBuilder.h"
#    include "mugen/render/spine/SpineSkeletonCache.h"
#endif

#include <algorithm>
#include <vector>

NS_MG_BEGIN

AttackAction::AttackAction() {}

AttackAction::AttackAction(int32_t actionId, int32_t actionIndex, int32_t skillAttackId, bool effectTable)
    : actionId(actionId), actionIndex(actionIndex), skillAttackId(skillAttackId), effectTable(effectTable)
{}

AttackAction::~AttackAction() {}

namespace
{
constexpr int32_t kSafetyActionDurationMs = 5000;
constexpr float kLogicFrameMs             = 1000.0f / 30.0f;
constexpr uint32_t kPresShake             = 1u << 0;
constexpr uint32_t kPresDisplaySpine      = 1u << 1;
constexpr uint32_t kPresTransform         = 1u << 2;
constexpr uint32_t kPresStatic            = 1u << 3;

std::string replaceExtension(const std::string& path, const std::string& newExt)
{
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + newExt;
    return path.substr(0, dot) + newExt;
}

#ifdef RUNTIME_IN_AXMOL
GameMapRenderComponent* findMapRender(ECSManager* ecs);
#endif

VirtualCamera* findMapCamera(ECSManager* ecs)
{
#ifdef RUNTIME_IN_AXMOL
    if (auto* mapRender = findMapRender(ecs))
    {
        if (mapRender->camera)
            return mapRender->camera.get();
    }
#else
    (void)ecs;
#endif
    return nullptr;
}

#ifdef RUNTIME_IN_AXMOL
GameMapRenderComponent* findMapRender(ECSManager* ecs)
{
    if (!ecs)
        return nullptr;
    auto* word = reinterpret_cast<GameWord*>(ecs->getUserdata());
    if (!word)
        return nullptr;
    auto* directorComp = MG_GET_COMPONENT(word->getDirector(), DirectorComponent);
    auto* mapEntity    = ecs->getEntity(directorComp->mapEntityId);
    if (!mapEntity)
        return nullptr;
    return MG_GET_COMPONENT(mapEntity, GameMapRenderComponent);
}

std::string pickDisplaySpineAnim(const ResSpineConfig* spine)
{
    if (!spine || spine->spine.empty())
        return "animation";
    const std::string atlas = replaceExtension(spine->spine, ".atlas");
    const float scale       = spine->scale > 0.0f ? spine->scale : 1.0f;
    auto* data              = SpineSkeletonCache::getInstance()->getOrCreate(spine->spine, atlas, scale);
    if (!data)
        return "animation";
    if (data->findAnimation("animation"))
        return "animation";
    if (data->animationCount() > 0)
    {
        MgAnimation first = data->animationAt(0);
        if (first)
            return first.name();
    }
    return "animation";
}

void spawnDisplaySpineOverlay(GameMapRenderComponent* mapRender, const ResSpineConfig* spine)
{
    if (!mapRender || !mapRender->overlayNode || !spine)
        return;

    FashionSpineDesc desc;
    desc.skeleton   = spine->spine;
    desc.atlases    = {replaceExtension(spine->spine, ".atlas")};
    desc.scale      = spine->scale > 0.0f ? spine->scale : 1.0f;
    desc.motionFile = AvatarLayerUtils::spinePathToMotionFile(spine->spine);
    Avatar* avatar  = AvatarBuilder::createAvatar(desc);
    if (!avatar)
        return;

    const std::string anim = pickDisplaySpineAnim(spine);
    avatar->setMotion(anim, "", false);
    avatar->setAutoPlay(true);

    const ax::Size vis = ax::Director::getInstance()->getVisibleSize();
    avatar->setPosition(vis.width * 0.5f, vis.height * 0.5f);
    mapRender->overlayNode->addChild(avatar);

    const int durMs    = avatar->durationMs();
    const float durSec = durMs > 0 ? (static_cast<float>(durMs) / 1000.0f) : 3.0f;
    avatar->runAction(ax::Sequence::create(ax::DelayTime::create(durSec), ax::RemoveSelf::create(), nullptr));
}
#endif

}  // namespace

const ActionAttackConfig* AttackAction::lookupActionCfg() const
{
    auto* config = Config::getInstance();
    if (effectTable)
        return config->getActionAttackEffectConfigById(actionId);
    return config->getActionAttackConfigById(actionId);
}

bool AttackAction::roleCastGone(BTContext& ctx) const
{
    if (effectTable)
        return false;
    auto* mgr = SkillManager::of(ctx.entity);
    return !mgr || mgr->activeSkillAttackId != skillAttackId;
}

void AttackAction::syncDerivedFromConfig(BTContext& ctx)
{
    auto* actionCfg = lookupActionCfg();
    if (!actionCfg)
        return;

    const float scale = actionCfg->actionScaleTime > 0.0f ? actionCfg->actionScaleTime : 1.0f;
    frameIntervalMs   = kLogicFrameMs / scale;
    actionDelayMs     = std::max(0, actionCfg->actionDelayTime);

    if (auto* avatar = MG_GET_COMPONENT(ctx.entity, AvatarComponent))
        avatar->animationSpeed = scale;

    if (effectSpawned.size() != actionCfg->effectIds.size())
    {
        effectSpawned.assign(actionCfg->effectIds.size(), false);
        for (size_t i = 0; i < effectSpawned.size() && i < 32; ++i)
        {
            if (effectSpawnMask & (1u << i))
                effectSpawned[i] = true;
        }
    }
}

void AttackAction::resetDisplacement(BTContext& ctx)
{
    auto* physics      = MG_GET_COMPONENT(ctx.entity, PhysicsComponent);
    auto* displacement = MG_GET_COMPONENT(ctx.entity, DisplacementComponent);
    if (displacement)
    {
        displacement->restoreGravity(physics);
        displacement->reset();
    }
    if (physics)
    {
        physics->velocity.x = 0;
        physics->velocity.y = 0;
        physics->velocity.z = 0;
    }
}

void AttackAction::applyBuffs(BTContext& ctx, bool add)
{
    auto* actionCfg = lookupActionCfg();
    if (!actionCfg)
        return;

    for (int32_t bid : actionCfg->buffIds)
    {
        if (bid <= 0)
            continue;
        if (add)
        {
            if (auto* mgr = BuffManager::of(ctx.entity))
                mgr->addBuff(ctx.entity, bid, skillAttackId, 1);
        }
        else
        {
            if (auto* mgr = BuffManager::of(ctx.entity))
                mgr->removeBuff(ctx.entity, bid);
        }
    }
}

void AttackAction::spawnEffect(BTContext& ctx, int32_t effectId)
{
    if (effectId <= 0)
        return;
    auto* ecs         = ctx.entity->getECSManager();
    EntityId ownerId  = ctx.entity->getId();
    int32_t hitLookup = skillAttackId;
    if (auto* selfFx = Effect::of(ctx.entity))
    {
        if (selfFx->ownerId != INVALID_ENTITY_ID)
            ownerId = selfFx->ownerId;
        if (hitLookup <= 0)
            hitLookup = selfFx->skillHitId;
    }
    Entity* effect = EffectLifeSystem::spawnEffect(ecs, effectId, ownerId,
                                                   MG_GET_COMPONENT(ctx.entity, TransformComponent), hitLookup, false);
    auto* skillMgr = SkillManager::of(ctx.entity);
    if (effect && skillMgr)
        skillMgr->spawnedEffectIds.push_back(effect->getId());
}

void AttackAction::playSounds(BTContext& ctx)
{
    auto* actionCfg = lookupActionCfg();
    if (!actionCfg)
        return;
    if (soundsPlayed.size() != actionCfg->soundId.size())
        soundsPlayed.assign(actionCfg->soundId.size(), false);

    auto* ecs      = ctx.entity->getECSManager();
    auto* soundSys = MG_GET_SYSTEM(ecs, SoundSystem);
    if (!soundSys)
        return;

    for (size_t i = 0; i < actionCfg->soundId.size(); ++i)
        soundsPlayed[i] = true;

    std::vector<int32_t> valid;
    valid.reserve(actionCfg->soundId.size());
    for (int32_t sid : actionCfg->soundId)
    {
        if (sid > 0)
            valid.push_back(sid);
    }
    if (valid.empty())
        return;

    Random rng(static_cast<uint64_t>(actionId) ^ static_cast<uint64_t>(elapsedMs));
    const int32_t pick = valid[static_cast<size_t>(rng.nextInt(0, static_cast<int32_t>(valid.size()) - 1))];
    soundSys->play(pick, ctx.entity);
}

void AttackAction::onActionEnter(BTContext& ctx)
{
    sessionOpen = false;
    if (roleCastGone(ctx))
        return;

    auto* actionCfg = lookupActionCfg();
    if (!actionCfg)
    {
        MG_LOG_E("AttackAction: {} {} not found", effectTable ? "actionAttackEffect" : "actionAttack", actionId);
        return;
    }

    auto* skillMgr  = SkillManager::of(ctx.entity);
    auto* behavior  = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    auto* physics   = MG_GET_COMPONENT(ctx.entity, PhysicsComponent);
    auto* input     = MG_GET_COMPONENT(ctx.entity, InputComponent);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    auto* avatar    = MG_GET_COMPONENT(ctx.entity, AvatarComponent);

    if (skillMgr)
    {
        skillMgr->interruptOpen      = false;
        skillMgr->interruptExtraOpen = false;
        skillMgr->spawnedEffectIds.clear();
    }
    if (!effectTable && behavior)
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kAttack);
        behavior->statusTags &= ~StateTag::kTagMovable;
    }

    animFinishedAtMs = -1;
    airborneOnce     = false;
    elapsedMs        = 0;
    effectSpawnMask  = 0;
    presentationMask = 0;
    animationEnd     = false;
    effectSpawned.clear();
    soundsPlayed.clear();

    const float scale = actionCfg->actionScaleTime > 0.0f ? actionCfg->actionScaleTime : 1.0f;
    frameIntervalMs   = kLogicFrameMs / scale;
    actionDelayMs     = std::max(0, actionCfg->actionDelayTime);

    if (actionCfg->control == 1 && input && transform)
    {
        if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)))
            transform->facingDirection = FacingDirection::kFacingLeft;
        else if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)))
            transform->facingDirection = FacingDirection::kFacingRight;
    }

    if (avatar)
    {
        std::string animName;
        if (actionCfg->action >= 0)
            animName = avatar->playback.motionNameAt(static_cast<size_t>(actionCfg->action));
        const bool looping     = actionCfg->loop < 0 || actionCfg->loop > 1;
        avatar->animationSpeed = scale;
        if (!animName.empty())
            avatar->play(animName, looping ? -1 : 1, true);
        estimatedDurMs = avatar->playback.getDurationMs();
        // 时长来自 .box；未知时保持 0，完成事件不会触发
    }
    else
    {
        estimatedDurMs = kSafetyActionDurationMs;
    }

    effectSpawned.assign(actionCfg->effectIds.size(), false);

    playSounds(ctx);
    resetDisplacement(ctx);
    if (auto* displacement = MG_GET_COMPONENT(ctx.entity, DisplacementComponent))
    {
        if (actionCfg->displacementId > 0)
        {
            if (auto* d = Config::getInstance()->getDisplacementConfigById(actionCfg->displacementId))
            {
                float facing = 1.0f;
                if (transform)
                    facing = transform->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
                displacement->start(d, facing);
                if (physics)
                    displacement->writePhysicsVelocity(physics, facing);
            }
        }
    }
    applyBuffs(ctx, true);
    if (actionCfg->ghost >= 0 && avatar)
        ++avatar->ghostRefCount;
    if (actionCfg->shadow > 0.0f && avatar)
        avatar->shadowScale = actionCfg->shadow;

    sessionOpen = true;
    MG_LOG_D("AttackAction enter skill={} action[{}]={} delay={}", skillAttackId, actionIndex, actionId, actionDelayMs);
}

void AttackAction::onActionExit(BTContext& ctx)
{
    if (!sessionOpen)
        return;
    sessionOpen = false;

    if (effectTable)
    {
        if (auto* fx = Effect::of(ctx.entity))
        {
            const auto* cfg = fx->effectId > 0 ? Config::getInstance()->getEffectConfigById(fx->effectId) : nullptr;
            if (cfg)
            {
                int32_t n = 0;
                for (int32_t aid : cfg->actionIds)
                {
                    if (aid > 0)
                        ++n;
                }
                if (n > 0 && actionIndex == n - 1)
                    fx->destroyRequested = true;
            }
        }
    }

    auto* actionCfg = lookupActionCfg();
    auto* avatar    = MG_GET_COMPONENT(ctx.entity, AvatarComponent);
    if (actionCfg && actionCfg->ghost >= 0 && avatar)
        avatar->ghostRefCount = (std::max)(0, avatar->ghostRefCount - 1);
    if (avatar)
        avatar->shadowScale = 0.0f;

    applyBuffs(ctx, false);
    resetDisplacement(ctx);

    auto* skillMgr = SkillManager::of(ctx.entity);
    if (skillMgr)
    {
        auto* ecs = ctx.entity->getECSManager();
        for (uint32_t eid : skillMgr->spawnedEffectIds)
        {
            Entity* fxEnt = ecs->getEntity(static_cast<EntityId>(eid));
            if (!fxEnt)
                continue;
            auto* life = Effect::of(fxEnt);
            if (!life || life->effectId <= 0)
                continue;
            const auto* cfg = Config::getInstance()->getEffectConfigById(life->effectId);
            if (cfg && cfg->autoRelease == 0)
                ecs->destroyEntity(fxEnt);
        }
        skillMgr->spawnedEffectIds.clear();
    }

    if (avatar)
        avatar->animationSpeed = 1.0f;

    effectSpawned.clear();
    soundsPlayed.clear();
    animFinishedAtMs = -1;
    elapsedMs        = 0;
    airborneOnce     = false;
    effectSpawnMask  = 0;
    presentationMask = 0;
    animationEnd     = false;
}

BTStatus AttackAction::onActionUpdate(BTContext& ctx, int32_t dtMs)
{
    if (!sessionOpen || roleCastGone(ctx))
    {
        return BTStatus::Success;
    }

    auto* actionCfg = lookupActionCfg();
    if (!actionCfg)
    {
        return BTStatus::Failure;
    }

    syncDerivedFromConfig(ctx);

    if (dtMs < 0)
        dtMs = 0;

    elapsedMs += dtMs;

    auto* physics      = MG_GET_COMPONENT(ctx.entity, PhysicsComponent);
    auto* skillMgr     = SkillManager::of(ctx.entity);
    auto* avatar       = MG_GET_COMPONENT(ctx.entity, AvatarComponent);
    auto* displacement = MG_GET_COMPONENT(ctx.entity, DisplacementComponent);

    if (avatar && estimatedDurMs <= 0 && avatar->playback.getDurationMs() > 0)
        estimatedDurMs = avatar->playback.getDurationMs();

    if (displacement && displacement->braked && actionCfg->loop == -1 && actionCfg->displacementId > 0)
    {
        return BTStatus::Success;
    }
    if (physics)
    {
        if (actionCfg->obstruct == 1 && physics->boundaryHitFlags != 0)
        {
            return BTStatus::Success;
        }
        if (!physics->onGround || physics->position.z > physics->groundLevel + 0.01f)
            airborneOnce = true;
        if (actionCfg->floor == 0)
        {
            if (physics->justLanded || (airborneOnce && physics->onGround))
            {
                return BTStatus::Success;
            }
        }
    }

    const bool looping = actionCfg->loop < 0 || actionCfg->loop > 1;
    if (!animationEnd && !looping && estimatedDurMs > 0)
    {
        const int32_t playedMs = avatar ? avatar->playback.getCurrentTimeMs() : elapsedMs;
        if (playedMs >= estimatedDurMs)
        {
            animationEnd     = true;
            animFinishedAtMs = elapsedMs;
            elapsedMs        = 0;
        }
    }

    const float frame = frameIntervalMs > 0.0f ? static_cast<float>(elapsedMs) / frameIntervalMs : 0.0f;

    if (animationEnd && elapsedMs >= actionDelayMs)
    {
        return BTStatus::Success;
    }

    if (actionCfg->interruptFrame >= 0 && frame >= static_cast<float>(actionCfg->interruptFrame))
    {
        if (skillMgr)
            skillMgr->interruptOpen = true;

        if (skillMgr && skillMgr->canConsumePendingOnInterrupt(ctx.entity) &&
            skillMgr->dealWithNextSkillBase(ctx.entity))
        {
            return BTStatus::Success;
        }
    }

    if (actionCfg->interruptExtraFrame >= 0 && frame >= static_cast<float>(actionCfg->interruptExtraFrame))
    {
        if (skillMgr)
            skillMgr->interruptExtraOpen = true;

        if (skillMgr && skillMgr->canConsumePendingOnExtraInterrupt(ctx.entity) &&
            skillMgr->dealWithNextSkillBase(ctx.entity))
        {
            return BTStatus::Success;
        }
        if (skillMgr && skillMgr->dealWithRun(ctx.entity))
        {
            return BTStatus::Success;
        }
    }

    dealWithControl(ctx);

    if (frameIntervalMs > 0.0f)
    {
        for (size_t i = 0; i < actionCfg->effectIds.size() && i < effectSpawned.size(); ++i)
        {
            if (effectSpawned[i])
                continue;
            const int32_t ef = i < actionCfg->effectFrames.size() ? actionCfg->effectFrames[i] : 0;
            if (ef <= 0 || frame >= static_cast<float>(ef))
            {
                effectSpawned[i] = true;
                if (i < 32)
                    effectSpawnMask |= (1u << i);
                if (actionCfg->effectIds[i] > 0)
                    spawnEffect(ctx, actionCfg->effectIds[i]);
            }
        }
    }

    dealWithPresentation(ctx, frame);
    return BTStatus::Running;
}

void AttackAction::dealWithControl(BTContext& ctx)
{
    auto* actionCfg = lookupActionCfg();
    if (!actionCfg)
        return;

    auto* physics   = MG_GET_COMPONENT(ctx.entity, PhysicsComponent);
    auto* input     = MG_GET_COMPONENT(ctx.entity, InputComponent);
    auto* transform = MG_GET_COMPONENT(ctx.entity, TransformComponent);
    if (!physics || !input)
        return;

    const int32_t ctrl = actionCfg->control;
    const bool hasDisp = actionCfg->displacementId > 0;

    float vx = 0.0f, vy = 0.0f;
    if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)))
        vx -= 1.0f;
    if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)))
        vx += 1.0f;
    if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)))
        vy += 1.0f;
    if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN)))
        vy -= 1.0f;

    auto applyMove = [&]() {
        if (vx == 0.0f && vy == 0.0f)
        {
            physics->velocity.x = 0;
            physics->velocity.y = 0;
            return;
        }
        const float speed   = actionCfg->controlVelocity;
        const float len     = std::sqrt(vx * vx + vy * vy);
        physics->velocity.x = vx / len * speed;
        physics->velocity.y = vy / len * speed;
    };
    auto applyFace = [&]() {
        if (transform && vx != 0.0f)
        {
            transform->facingDirection = vx > 0 ? FacingDirection::kFacingRight : FacingDirection::kFacingLeft;
        }
    };

    if (ctrl == 0 && !hasDisp)
    {
        applyMove();
        applyFace();
    }
    else if (ctrl == 2 && hasDisp)
    {
        applyFace();
    }
    else if (ctrl == 3 && !hasDisp)
    {
        applyMove();
    }
}

void AttackAction::triggerShake(BTContext& ctx)
{
    auto* actionCfg = lookupActionCfg();
    if (!actionCfg || actionCfg->cameraId < 0)
        return;
    const auto* camCfg = Config::getInstance()->getCameraConfigById(actionCfg->cameraId);
    if (!camCfg)
        return;
#ifdef RUNTIME_IN_AXMOL
    if (auto* cam = findMapCamera(ctx.entity->getECSManager()))
    {
        float amp = (std::max)(camCfg->amplitudeX, camCfg->amplitudeY);
        cam->shake(amp, camCfg->duration);
    }
#endif
}

void AttackAction::triggerDisplaySpine(BTContext& ctx)
{
#ifdef RUNTIME_IN_AXMOL
    auto* actionCfg = lookupActionCfg();
    if (!actionCfg || actionCfg->displaySpineIds.empty())
        return;
    auto* mapRender = findMapRender(ctx.entity->getECSManager());
    if (!mapRender || !mapRender->overlayNode)
        return;
    for (int32_t spineId : actionCfg->displaySpineIds)
    {
        if (spineId <= 0)
            continue;
        const auto* spine = Config::getInstance()->getResSpineConfigById(spineId);
        if (!spine)
            continue;
        spawnDisplaySpineOverlay(mapRender, spine);
    }
#else
    (void)ctx;
#endif
}

void AttackAction::triggerTransform(BTContext& ctx)
{
    auto* actionCfg = lookupActionCfg();
    auto* avatar    = MG_GET_COMPONENT(ctx.entity, AvatarComponent);
    if (!actionCfg || actionCfg->transformId <= 0 || !avatar)
        return;
    if (const auto* spine = Config::getInstance()->getResSpineConfigById(actionCfg->transformId))
    {
        avatar->resSpine      = spine;
        avatar->spineSkeleton = spine->spine;
        avatar->spineAtlas    = replaceExtension(spine->spine, ".atlas");
        avatar->defaultSkin.clear();
        avatar->spineScale = spine->scale > 0.0f ? spine->scale : avatar->spineScale;
        if (auto* render = MG_GET_COMPONENT(ctx.entity, AvatarRenderComponent))
        {
            render->syncedMotion.clear();
            render->syncedEntry.clear();
        }
    }
}

void AttackAction::triggerStatic(BTContext& ctx)
{
    auto* actionCfg = lookupActionCfg();
    if (!actionCfg || actionCfg->staticTarget < 0)
        return;
    auto* skillMgr = SkillManager::of(ctx.entity);
    if (skillMgr && skillMgr->staticResetRemainMs > 0)
        return;

    const int32_t staticMs = actionCfg->staticTime;
    if (staticMs <= 0)
        return;

    auto applyStatic = [staticMs](Entity* e) {
        if (auto* b = MG_GET_COMPONENT(e, BehaviorComponent))
        {
            if (b->staticRemainMs < staticMs)
                b->staticRemainMs = staticMs;
            b->statusTags &= ~(StateTag::kTagMovable | StateTag::kTagAttackAllowed);
        }
    };

    if (actionCfg->staticTarget == 1)
        applyStatic(ctx.entity);

    auto* ecs = ctx.entity->getECSManager();
    Signature sig;
    sig.set(ecs->getComponentTypeId("BehaviorComponent"));
    sig.set(ecs->getComponentTypeId("IdentityComponent"));
    for (Entity* e : ecs->getEntitiesBySignature(sig))
    {
        if (e == ctx.entity)
            continue;
        auto* selfId  = MG_GET_COMPONENT(ctx.entity, IdentityComponent);
        auto* otherId = MG_GET_COMPONENT(e, IdentityComponent);
        if (!selfId || !otherId)
            continue;
        if (selfId->category == otherId->category && selfId->monsterCamps != 0 && otherId->monsterCamps != 0 &&
            (selfId->monsterCamps & otherId->monsterCamps) != 0)
            continue;
        applyStatic(e);
    }

    if (skillMgr)
        skillMgr->staticResetRemainMs = (std::max)(0, actionCfg->staticResetTime);
}

void AttackAction::dealWithPresentation(BTContext& ctx, float frame)
{
    auto* actionCfg = lookupActionCfg();
    if (!actionCfg)
        return;

    if (actionCfg->cameraId >= 0 && actionCfg->cameraFrame >= 0 && !(presentationMask & kPresShake) &&
        frame >= static_cast<float>(actionCfg->cameraFrame))
    {
        presentationMask |= kPresShake;
        triggerShake(ctx);
    }

    if (!actionCfg->displaySpineIds.empty() && actionCfg->displaySpineFrame >= 0 &&
        !(presentationMask & kPresDisplaySpine) && frame >= static_cast<float>(actionCfg->displaySpineFrame))
    {
        presentationMask |= kPresDisplaySpine;
        triggerDisplaySpine(ctx);
    }

    if (actionCfg->transformId > 0 && actionCfg->transformFrame >= 0 && !(presentationMask & kPresTransform) &&
        frame >= static_cast<float>(actionCfg->transformFrame))
    {
        presentationMask |= kPresTransform;
        triggerTransform(ctx);
    }

    if (actionCfg->staticTarget >= 0 && actionCfg->staticStartFrame >= 0 && !(presentationMask & kPresStatic) &&
        frame >= static_cast<float>(actionCfg->staticStartFrame))
    {
        presentationMask |= kPresStatic;
        triggerStatic(ctx);
    }
}

void AttackAction::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(actionId);
    byteBuffer.writeInt32(actionIndex);
    byteBuffer.writeInt32(skillAttackId);
    byteBuffer.writeBool(effectTable);
}

bool AttackAction::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return byteBuffer.getInt32(actionId) && byteBuffer.getInt32(actionIndex) && byteBuffer.getInt32(skillAttackId) &&
           byteBuffer.getBool(effectTable);
}

NS_MG_END
