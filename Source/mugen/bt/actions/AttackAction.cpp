#include "mugen/bt/actions/AttackAction.h"

#include "mugen/Components.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/bt/SkillCastRules.h"
#include "mugen/buff/BuffApi.h"
#include "mugen/conf/Config.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"
#include "mugen/render/VirtualCamera.h"
#include "mugen/system/EffectLifeSystem.h"
#include "mugen/system/SoundSystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

NS_MG_BEGIN

namespace
{
constexpr int32_t kSafetyActionDurationMs = 5000;
constexpr float kLogicFrameMs             = 1000.0f / 30.0f;
constexpr uint32_t kPresShake             = 1u << 0;
constexpr uint32_t kPresDisplaySpine      = 1u << 1;
constexpr uint32_t kPresTransform         = 1u << 2;
constexpr uint32_t kPresStatic            = 1u << 3;
constexpr int32_t kGhostIntervalMs        = 80;

std::string replaceExtension(const std::string& path, const std::string& newExt)
{
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + newExt;
    return path.substr(0, dot) + newExt;
}

VirtualCamera* findMapCamera(ECSManager* ecs)
{
#ifdef RUNTIME_IN_AXMOL
    if (!ecs)
        return nullptr;
    Signature sig;
    sig.set(ecs->getComponentTypeId("GameMapRenderComponent"));
    for (Entity* e : ecs->getEntitiesBySignature(sig))
    {
        if (auto* mapRender = MG_GET_COMPONENT(e, GameMapRenderComponent))
        {
            if (mapRender->camera)
                return mapRender->camera.get();
        }
    }
#else
    (void)ecs;
#endif
    return nullptr;
}

}  // namespace

void AttackAction::resetDisplacement(BTContext& ctx)
{
    auto* displacement = ctx.displacement;
    if (!displacement)
        return;
    if (ctx.entity)
    {
        if (auto* physics = MG_GET_COMPONENT(ctx.entity, PhysicsComponent))
        {
            if (displacement->hasSavedGravity)
                physics->gravityScale = displacement->savedGravityScale;
        }
    }
    displacement->reset();
}

void AttackAction::applyBuffs(BTContext& ctx, bool add)
{
    if (!ctx.entity)
        return;
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg)
        return;

    for (int32_t bid : actionCfg->buffIds)
    {
        if (bid <= 0)
            continue;
        if (add)
            BuffApi::addBuff(ctx.entity, bid, skillAttackId, 1);
        else
            BuffApi::removeBuff(ctx.entity, bid);
    }
}

void AttackAction::spawnEffect(BTContext& ctx, int32_t effectId)
{
    if (!ctx.entity || effectId <= 0)
        return;
    auto* cfg = Config::getInstance()->getEffectConfigById(effectId);
    if (!cfg)
        return;
    auto* ecs = ctx.ecs ? ctx.ecs : ctx.entity->getECSManager();
    if (!ecs)
        return;

    auto* ownerTf = ctx.transform;
    auto* effect  = ecs->newEntity();
    auto* tf      = MG_ADD_COMPONENT(effect, TransformComponent);
    auto* fx      = MG_ADD_COMPONENT(effect, EffectLifetimeComponent);
    auto* fxAttr  = MG_ADD_COMPONENT(effect, AttributeComponent);  // 顿帧挂点
    (void)fxAttr;

    fx->effectId         = effectId;
    fx->skillHitId       = cfg->skillHitId > 0 ? cfg->skillHitId : skillAttackId;
    fx->ownerId          = ctx.entity->getId();
    // autoRelease==0：由动作 exit 销毁，寿命给一个大值避免提前超时
    fx->lifetimeMs       = cfg->autoRelease > 0 ? cfg->autoRelease : 60000;
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

    if (ctx.skillCast)
        ctx.skillCast->spawnedEffectIds.push_back(effect->getId());
}

void AttackAction::playSounds(BTContext& ctx)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg || !ctx.entity)
        return;
    if (soundsPlayed.size() != actionCfg->soundId.size())
        soundsPlayed.assign(actionCfg->soundId.size(), false);

    auto* ecs = ctx.ecs ? ctx.ecs : ctx.entity->getECSManager();
    if (!ecs)
        return;
    auto* soundSys = MG_GET_SYSTEM(ecs, SoundSystem);
    if (!soundSys)
        return;

    for (size_t i = 0; i < actionCfg->soundId.size(); ++i)
        soundsPlayed[i] = true;

    // 随机播放一条音效
    std::vector<int32_t> valid;
    valid.reserve(actionCfg->soundId.size());
    for (int32_t sid : actionCfg->soundId)
    {
        if (sid > 0)
            valid.push_back(sid);
    }
    if (valid.empty())
        return;

    Random rng(static_cast<uint64_t>(actionId) ^ static_cast<uint64_t>(ctx.bt ? ctx.bt->actionElapsedMs : 0));
    const int32_t pick = valid[static_cast<size_t>(rng.nextInt(0, static_cast<int32_t>(valid.size()) - 1))];
    soundSys->play(pick, ctx.entity);
}

void AttackAction::onActionEnter(BTContext& ctx)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg)
    {
        MG_LOG_E("AttackAction: actionAttack {} not found", actionId);
        return;
    }

    // 快照恢复后树重建会重入 enter：用组件态续跑，避免重复扣副作用/特效
    const bool resume = ctx.bt && ctx.bt->currentActionId == actionId && ctx.bt->actionElapsedMs > 0;

    if (ctx.skillCast && !resume)
    {
        ctx.skillCast->interruptOpen      = false;
        ctx.skillCast->interruptExtraOpen = false;
        ctx.skillCast->spawnedEffectIds.clear();
    }
    if (ctx.behavior)
    {
        ctx.behavior->currentKind = static_cast<int32_t>(BehaviorKind::kAttack);
        ctx.behavior->statusTags &= ~StateTag::kTagMovable;
    }

    if (ctx.bt)
    {
        ctx.bt->currentActionId  = actionId;
        ctx.bt->activeBranchKind = static_cast<int32_t>(BehaviorKind::kAttack);
        if (!resume)
        {
            ctx.bt->actionElapsedMs = 0;
            ctx.bt->effectSpawnMask = 0;
            ctx.bt->animationEnd    = false;
        }
    }

    animFinishedAtMs = -1;
    const float scale = actionCfg->actionScaleTime > 0.0f ? actionCfg->actionScaleTime : 1.0f;
    frameIntervalMs   = kLogicFrameMs / scale;
    actionDelayMs     = std::max(0, actionCfg->actionDelayTime);
    elapsedMs         = resume && ctx.bt ? ctx.bt->actionElapsedMs : 0;
    if (resume && ctx.bt && ctx.bt->animationEnd)
        animFinishedAtMs = (std::max)(0, elapsedMs - actionDelayMs);

    // control==1：按输入改朝向（简化：左右键）
    if (!resume && actionCfg->control == 1 && ctx.input && ctx.transform)
    {
        if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)))
            ctx.transform->facingDirection = FacingDirection::kFacingLeft;
        else if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)))
            ctx.transform->facingDirection = FacingDirection::kFacingRight;
    }

    if (ctx.avatar)
    {
        const std::string animName = std::to_string(actionCfg->action);
        const bool loop            = (actionCfg->loop == 0 || actionCfg->loop > 1);
        ctx.avatar->animationSpeed = scale;
        if (!resume)
            ctx.avatar->play(animName, loop ? -1 : 1, true);
        estimatedDurMs = ctx.avatar->playback.getDurationMs();
        if (estimatedDurMs <= 0)
            estimatedDurMs = kSafetyActionDurationMs;
    }
    else
    {
        estimatedDurMs = kSafetyActionDurationMs;
    }

    effectSpawned.assign(actionCfg->effectIds.size(), false);
    if (resume && ctx.bt)
    {
        for (size_t i = 0; i < effectSpawned.size() && i < 32; ++i)
        {
            if (ctx.bt->effectSpawnMask & (1u << i))
                effectSpawned[i] = true;
        }
    }

    if (!resume)
    {
        soundsPlayed.clear();
        playSounds(ctx);
        resetDisplacement(ctx);
        if (ctx.displacement && actionCfg->displacementId > 0)
        {
            if (auto* d = Config::getInstance()->getDisplacementConfigById(actionCfg->displacementId))
                ctx.displacement->start(d);
        }
        applyBuffs(ctx, true);
        if (actionCfg->ghost >= 0 && ctx.avatar)
            ++ctx.avatar->ghostRefCount;
        if (actionCfg->shadow > 0.0f && ctx.avatar)
            ctx.avatar->shadowScale = actionCfg->shadow;
    }

    MG_LOG_D("AttackAction enter skill={} action[{}]={} delay={} resume={}", skillAttackId, actionIndex, actionId,
             actionDelayMs, resume ? 1 : 0);
}

void AttackAction::onActionExit(BTContext& ctx)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (actionCfg && actionCfg->ghost >= 0 && ctx.avatar)
        ctx.avatar->ghostRefCount = (std::max)(0, ctx.avatar->ghostRefCount - 1);
    if (ctx.avatar)
        ctx.avatar->shadowScale = 0.0f;

    applyBuffs(ctx, false);
    resetDisplacement(ctx);

    // autoRelease==0 的特效在动作结束时销毁
    if (ctx.skillCast && ctx.ecs)
    {
        for (uint32_t eid : ctx.skillCast->spawnedEffectIds)
        {
            Entity* fxEnt = ctx.ecs->getEntity(static_cast<EntityId>(eid));
            if (!fxEnt)
                continue;
            auto* life = MG_GET_COMPONENT(fxEnt, EffectLifetimeComponent);
            if (!life)
                continue;
            const auto* cfg = Config::getInstance()->getEffectConfigById(life->effectId);
            if (cfg && cfg->autoRelease == 0)
                ctx.ecs->destroyEntity(fxEnt);
        }
        ctx.skillCast->spawnedEffectIds.clear();
    }

    if (ctx.avatar)
        ctx.avatar->animationSpeed = 1.0f;
    if (ctx.bt)
    {
        ctx.bt->currentActionId  = 0;
        ctx.bt->actionElapsedMs  = 0;
        ctx.bt->effectSpawnMask  = 0;
        ctx.bt->presentationMask = 0;
        ctx.bt->animationEnd     = false;
    }
    effectSpawned.clear();
    soundsPlayed.clear();
    animFinishedAtMs = -1;
    elapsedMs        = 0;
}

BTStatus AttackAction::onActionTick(BTContext& ctx, int32_t dtMs)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg)
        return BTStatus::Failure;

    if (dtMs < 0)
        dtMs = 0;

    elapsedMs += dtMs;
    if (ctx.bt)
        ctx.bt->actionElapsedMs = elapsedMs;

    const float frame =
        frameIntervalMs > 0.0f ? static_cast<float>(elapsedMs) / frameIntervalMs : 0.0f;

    // —— obstruct / floor（对齐 ActionAttack onDisplacementEvent）——
    // obstruct==1：碰边界结束动作；floor==0：落地结束动作
    if (ctx.physics)
    {
        if (actionCfg->obstruct == 1 && ctx.physics->boundaryHitFlags != 0)
            return BTStatus::Success;
        if (actionCfg->floor == 0 && ctx.physics->justLanded)
            return BTStatus::Success;
    }

    // —— dealWithInterrupt ——
    if (actionCfg->interruptFrame >= 0 && frame >= static_cast<float>(actionCfg->interruptFrame))
    {
        if (ctx.skillCast)
            ctx.skillCast->interruptOpen = true;

        if (SkillCastRules::canConsumePendingOnInterrupt(ctx.entity))
        {
            if (SkillCastRules::dealWithNextSkillBase(ctx.entity))
                return BTStatus::Failure;
        }
        else if (SkillCastRules::dealWithRun(ctx.entity))
        {
            return BTStatus::Failure;
        }
    }

    // —— dealWithExtraInterrupt ——
    if (actionCfg->interruptExtraFrame >= 0 && frame >= static_cast<float>(actionCfg->interruptExtraFrame))
    {
        if (ctx.skillCast)
            ctx.skillCast->interruptExtraOpen = true;

        if (SkillCastRules::canConsumePendingOnExtraInterrupt(ctx.entity))
        {
            if (SkillCastRules::dealWithNextSkillBase(ctx.entity))
                return BTStatus::Failure;
        }
        else if (SkillCastRules::dealWithRun(ctx.entity))
        {
            return BTStatus::Failure;
        }
    }

    // —— dealWithControl：攻击中移动 ——
    // 0=全向移动+朝向；2=仅朝向；3=移动不改朝向
    if (ctx.physics && ctx.input)
    {
        const bool dispActive =
            ctx.displacement && !ctx.displacement->finished && ctx.displacement->activeConfig;
        if (!dispActive)
        {
            const int32_t ctrl = actionCfg->control;
            const bool allowMove = (ctrl == 0 || ctrl == 3) && actionCfg->controlVelocity > 0.0f;
            const bool allowFace = (ctrl == 0 || ctrl == 2);

            if (allowMove || allowFace)
            {
                float vx = 0.0f, vy = 0.0f;
                if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)))
                    vx -= 1.0f;
                if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)))
                    vx += 1.0f;
                if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)))
                    vy += 1.0f;
                if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN)))
                    vy -= 1.0f;

                if (allowMove)
                {
                    if (vx != 0.0f || vy != 0.0f)
                    {
                        const float speed = actionCfg->controlVelocity;
                        const float len   = std::sqrt(vx * vx + vy * vy);
                        ctx.physics->velocity.x = vx / len * speed;
                        ctx.physics->velocity.y = vy / len * speed;
                    }
                    else
                    {
                        ctx.physics->velocity.x = 0;
                        ctx.physics->velocity.y = 0;
                    }
                }
                else
                {
                    ctx.physics->velocity.x = 0;
                    ctx.physics->velocity.y = 0;
                }

                if (allowFace && ctx.transform && vx != 0.0f)
                {
                    ctx.transform->facingDirection =
                        vx > 0 ? FacingDirection::kFacingRight : FacingDirection::kFacingLeft;
                }
            }
            else
            {
                ctx.physics->velocity.x = 0;
                ctx.physics->velocity.y = 0;
            }
        }
    }

    // —— dealWithEffect ——
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
                if (ctx.bt && i < 32)
                    ctx.bt->effectSpawnMask |= (1u << i);
                if (actionCfg->effectIds[i] > 0)
                    spawnEffect(ctx, actionCfg->effectIds[i]);
            }
        }
    }

    dealWithPresentation(ctx, frame);

    bool animDone = false;
    if (ctx.avatar && ctx.avatar->animationFinished)
        animDone = true;
    else if (ctx.avatar)
    {
        const int32_t animDur = ctx.avatar->playback.getDurationMs();
        if (animDur > 0 && elapsedMs >= animDur)
            animDone = true;
        else if (animDur <= 0 && elapsedMs >= estimatedDurMs)
            animDone = true;
    }
    else if (elapsedMs >= estimatedDurMs)
    {
        animDone = true;
    }

    if (animDone)
    {
        if (ctx.bt)
            ctx.bt->animationEnd = true;
        if (animFinishedAtMs < 0)
            animFinishedAtMs = elapsedMs;
        if (elapsedMs - animFinishedAtMs >= actionDelayMs)
            return BTStatus::Success;
    }

    return BTStatus::Running;
}

void AttackAction::triggerShake(BTContext& ctx)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg || actionCfg->cameraId < 0)
        return;
    const auto* camCfg = Config::getInstance()->getCameraConfigById(actionCfg->cameraId);
    if (!camCfg)
        return;
#ifdef RUNTIME_IN_AXMOL
    if (auto* cam = findMapCamera(ctx.ecs))
    {
        float amp = camCfg->amplitude;
        if (amp <= 0.0f)
            amp = (std::max)(camCfg->amplitudeX, camCfg->amplitudeY);
        cam->shake(amp, camCfg->duration, camCfg->freezeTime);
    }
#endif
    if (camCfg->freezeTime > 0 && ctx.attribute)
    {
        if (ctx.attribute->freezeRemainingMs < camCfg->freezeTime)
            ctx.attribute->freezeRemainingMs = camCfg->freezeTime;
    }
}

void AttackAction::triggerDisplaySpine(BTContext& ctx)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg || actionCfg->displaySpineIds.empty())
        return;
    for (int32_t spineId : actionCfg->displaySpineIds)
    {
        if (spineId <= 0)
            continue;
        // 最小可用：按 ResSpine 生成短寿命跟随特效实体（居中由渲染层定位）
        if (!ctx.ecs || !ctx.entity)
            continue;
        const auto* spine = Config::getInstance()->getResSpineConfigById(spineId);
        if (!spine)
            continue;
        auto* vfx = ctx.ecs->newEntity();
        auto* tf  = MG_ADD_COMPONENT(vfx, TransformComponent);
        auto* fx  = MG_ADD_COMPONENT(vfx, EffectLifetimeComponent);
        fx->ownerId    = ctx.entity->getId();
        fx->follow     = false;
        fx->lifetimeMs = 3000;
        if (ctx.transform)
            tf->position = ctx.transform->position;
        auto* avatar = MG_ADD_COMPONENT(vfx, AvatarComponent);
        MG_ADD_COMPONENT(vfx, AvatarRenderComponent);
        avatar->resSpine = spine;
        if (!spine->spine.empty())
        {
            avatar->spineSkeleton = spine->spine;
            avatar->spineAtlas =
                !spine->atlas.empty() ? spine->atlas : replaceExtension(spine->spine, ".atlas");
            avatar->defaultSkin = spine->defaultSkin;
            avatar->spineScale  = spine->scale > 0.0f ? spine->scale : 1.0f;
        }
        vfx->notifyEntityReady();
        if (ctx.skillCast)
            ctx.skillCast->spawnedEffectIds.push_back(vfx->getId());
    }
}

void AttackAction::triggerTransform(BTContext& ctx)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg || actionCfg->transformId <= 0 || !ctx.avatar)
        return;
    // 最小可用：transformId 视为 ResSpineId，切换当前 Avatar 资源
    if (const auto* spine = Config::getInstance()->getResSpineConfigById(actionCfg->transformId))
    {
        ctx.avatar->resSpine      = spine;
        ctx.avatar->spineSkeleton = spine->spine;
        ctx.avatar->spineAtlas =
            !spine->atlas.empty() ? spine->atlas : replaceExtension(spine->spine, ".atlas");
        ctx.avatar->defaultSkin = spine->defaultSkin;
        ctx.avatar->spineScale  = spine->scale > 0.0f ? spine->scale : ctx.avatar->spineScale;
        if (auto* render = MG_GET_COMPONENT(ctx.entity, AvatarRenderComponent))
        {
            render->syncedMotion.clear();
            render->syncedEntry.clear();
        }
    }
}

void AttackAction::triggerStatic(BTContext& ctx)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg || actionCfg->staticTarget < 0 || !ctx.ecs)
        return;
    if (ctx.bt && ctx.bt->staticResetRemainMs > 0)
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

    // 0=仅敌方；1=自己+敌方。简化：非自身一律视为敌方目标池中有 Behavior 的实体
    if (actionCfg->staticTarget == 1 && ctx.entity)
        applyStatic(ctx.entity);

    Signature sig;
    sig.set(ctx.ecs->getComponentTypeId("BehaviorComponent"));
    sig.set(ctx.ecs->getComponentTypeId("IdentityComponent"));
    for (Entity* e : ctx.ecs->getEntitiesBySignature(sig))
    {
        if (e == ctx.entity)
            continue;
        auto* selfId = MG_GET_COMPONENT(ctx.entity, IdentityComponent);
        auto* otherId = MG_GET_COMPONENT(e, IdentityComponent);
        if (!selfId || !otherId)
            continue;
        // 友军：同 category 且 monsterCamps 有交集
        if (selfId->category == otherId->category && selfId->monsterCamps != 0 && otherId->monsterCamps != 0 &&
            (selfId->monsterCamps & otherId->monsterCamps) != 0)
            continue;
        applyStatic(e);
    }

    if (ctx.bt)
        ctx.bt->staticResetRemainMs = (std::max)(0, actionCfg->staticResetTime);
}

void AttackAction::dealWithPresentation(BTContext& ctx, float frame)
{
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg || !ctx.bt)
        return;

    if (actionCfg->cameraId >= 0 && actionCfg->cameraFrame >= 0 &&
        !(ctx.bt->presentationMask & kPresShake) && frame >= static_cast<float>(actionCfg->cameraFrame))
    {
        ctx.bt->presentationMask |= kPresShake;
        triggerShake(ctx);
    }

    if (!actionCfg->displaySpineIds.empty() && actionCfg->displaySpineFrame >= 0 &&
        !(ctx.bt->presentationMask & kPresDisplaySpine) &&
        frame >= static_cast<float>(actionCfg->displaySpineFrame))
    {
        ctx.bt->presentationMask |= kPresDisplaySpine;
        triggerDisplaySpine(ctx);
    }

    if (actionCfg->transformId > 0 && actionCfg->transformFrame >= 0 &&
        !(ctx.bt->presentationMask & kPresTransform) &&
        frame >= static_cast<float>(actionCfg->transformFrame))
    {
        ctx.bt->presentationMask |= kPresTransform;
        triggerTransform(ctx);
    }

    if (actionCfg->staticTarget >= 0 && actionCfg->staticStartFrame >= 0 &&
        !(ctx.bt->presentationMask & kPresStatic) &&
        frame >= static_cast<float>(actionCfg->staticStartFrame))
    {
        ctx.bt->presentationMask |= kPresStatic;
        triggerStatic(ctx);
    }
}

NS_MG_END
