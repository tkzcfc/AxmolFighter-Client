#include "mugen/bt/actions/AttackAction.h"

#include "mugen/Components.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/bt/SkillCastRules.h"
#include "mugen/conf/Config.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/system/SoundSystem.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace
{
constexpr int32_t kSafetyActionDurationMs = 5000;
constexpr float kLogicFrameMs             = 1000.0f / 30.0f;

std::string replaceExtension(const std::string& path, const std::string& newExt)
{
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + newExt;
    return path.substr(0, dot) + newExt;
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
    auto* buffs = ctx.buff;
    if (!buffs)
        return;
    auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId);
    if (!actionCfg)
        return;

    for (int32_t bid : actionCfg->buffIds)
    {
        if (bid <= 0)
            continue;
        if (add)
        {
            BuffInstance inst;
            inst.buffId        = bid;
            inst.sourceSkillId = skillAttackId;
            const auto* buffCfg = Config::getInstance()->getBuffConfigById(bid);
            // BuffConfig.times：毫秒；缺省兜底 3000
            inst.remainingMs = (buffCfg && buffCfg->times > 0) ? buffCfg->times : 3000;
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
    {
        if (soundsPlayed[i])
            continue;
        soundsPlayed[i] = true;
        if (actionCfg->soundId[i] > 0)
            soundSys->play(actionCfg->soundId[i], ctx.entity);
    }
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
    }

    MG_LOG_D("AttackAction enter skill={} action[{}]={} delay={} resume={}", skillAttackId, actionIndex, actionId,
             actionDelayMs, resume ? 1 : 0);
}

void AttackAction::onActionExit(BTContext& ctx)
{
    applyBuffs(ctx, false);
    resetDisplacement(ctx);
    if (ctx.avatar)
        ctx.avatar->animationSpeed = 1.0f;
    if (ctx.bt)
    {
        ctx.bt->currentActionId = 0;
        ctx.bt->actionElapsedMs = 0;
        ctx.bt->effectSpawnMask = 0;
        ctx.bt->animationEnd    = false;
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

    // —— dealWithInterrupt ——
    if (actionCfg->interruptFrame >= 0 && frame >= static_cast<float>(actionCfg->interruptFrame))
    {
        if (ctx.skillCast)
            ctx.skillCast->interruptOpen = true;

        if (SkillCastRules::canConsumePendingOnInterrupt(ctx.entity))
        {
            // Failure：中止当前 Toward 序列，由父 Selector 按新施法上下文重选（勿 Success 推进旧动作链）
            if (SkillCastRules::dealWithNextSkillBase(ctx.entity))
                return BTStatus::Failure;
        }
        else if (ctx.skillCast && ctx.skillCast->wantRunCancel)
        {
            // B7：跑取消仅至尊窗；普通窗不消费 wantRunCancel
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
        else if (ctx.skillCast && ctx.skillCast->pendingSkillAttackId == 0)
        {
            // 无预输入：Dash 输入 → 跑取消（仅至尊窗）
            const bool wantRun = ctx.behavior &&
                                 (ctx.behavior->statusTags & StateTag::kTagDashState) != 0 &&
                                 bt_util::anyMoveKeyDown(ctx.input);
            if (wantRun || (ctx.skillCast && ctx.skillCast->wantRunCancel))
            {
                ctx.skillCast->wantRunCancel        = false;
                ctx.skillCast->activeSkillAttackId  = 0;
                ctx.skillCast->pendingSkillAttackId = 0;
                ctx.skillCast->interruptOpen        = false;
                ctx.skillCast->interruptExtraOpen   = false;
                SkillCastRules::syncBehaviorMirror(ctx.entity);
                if (ctx.behavior)
                {
                    ctx.behavior->statusTags |=
                        StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed |
                        StateTag::kTagDashState;
                    ctx.behavior->currentKind = static_cast<int32_t>(BehaviorKind::kDash);
                }
                return BTStatus::Failure;
            }
        }
    }

    // —— dealWithControl：攻击中移动 ——
    if (ctx.physics && ctx.input)
    {
        const bool dispActive =
            ctx.displacement && !ctx.displacement->finished && ctx.displacement->activeConfig;
        if (!dispActive)
        {
            if (actionCfg->control != 0 && actionCfg->controlVelocity > 0.0f)
            {
                const float speed = actionCfg->controlVelocity;
                float vx = 0.0f, vy = 0.0f;
                if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)))
                    vx -= 1.0f;
                if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)))
                    vx += 1.0f;
                if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)))
                    vy += 1.0f;
                if (ctx.input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN)))
                    vy -= 1.0f;
                if (vx != 0.0f || vy != 0.0f)
                {
                    const float len         = std::sqrt(vx * vx + vy * vy);
                    ctx.physics->velocity.x = vx / len * speed;
                    ctx.physics->velocity.y = vy / len * speed;
                    if (ctx.transform && vx != 0.0f)
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

NS_MG_END
