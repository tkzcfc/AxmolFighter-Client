#include "mugen/sim/view/WorldView.h"

#include "mugen/sim/World.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/AbilityDef.h"
#include "mugen/conf/battle/AiDef.h"
#include "mugen/conf/battle/BuffDef.h"
#include "mugen/conf/battle/ClipDef.h"
#include "mugen/conf/battle/EffectDef.h"
#include "mugen/conf/battle/RoleDef.h"
#include "mugen/conf/battle/SkillDef.h"

#include <array>
#include <set>

namespace mugen::sim
{
namespace
{
// 待回收或休眠的实体对表现层不可见。
bool hidden(const WorldECS& ecs, Entity entity)
{
    return ecs.has<DespawningComponent>(entity) || ecs.has<InactiveComponent>(entity);
}

// 身份种类映射到可视种类；召唤物与角色共用 Role。
VisualEntityKind visualKind(SpawnKind kind)
{
    switch (kind)
    {
    case SpawnKind::Effect:
        return VisualEntityKind::Effect;
    case SpawnKind::Obstacle:
        return VisualEntityKind::Obstacle;
    case SpawnKind::Goods:
        return VisualEntityKind::Goods;
    case SpawnKind::Portal:
        return VisualEntityKind::Portal;
    case SpawnKind::Role:
        return VisualEntityKind::Role;
    }
    return VisualEntityKind::Role;
}

// 立即装配一个城镇角色。
Entity materializeTownRole(World& world, int32_t roleId, int32_t level, const FixedVec3& position, int32_t towardX,
                           std::optional<uint32_t> playerSlot)
{
    SpawnRequest request;
    request.kind          = SpawnKind::Role;
    request.configId      = roleId;
    request.level         = level;
    request.position      = position;
    request.towardX       = towardX == -1 ? -1 : 1;
    request.hasPlayerSlot = playerSlot.has_value();
    request.playerSlot    = playerSlot.value_or(0);
    if (const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(roleId))
        request.camp = row->monsterCamps;
    return life::rules::materialize(world, request, SpawnStage::RoleTimeline);
}

// 配置里 0 和负数都表示未配置。
bool validId(int32_t id)
{
    return id > 0;
}

// 按角色技能与特效配置递归收集特效 Spine，访问过的 id 记入集合以避免循环。
class SpineCollector
{
public:
    // 按出生时的技能装配展开该角色会用到的特效。
    void addRole(int32_t roleId, int32_t aiDifficulty)
    {
        if (!validId(roleId) || !m_roles.insert(roleId).second)
            return;
        const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(roleId);
        if (!role)
            return;

        addEffect(role->deathEffectId);
        addEffect(role->koEffectId);
        for (const int32_t buffId : role->buffIds)
            addBuff(buffId);

        const auto occupation = type_conversions::toCharacterClass(roleId);
        if (occupation != CharacterClass::kUnknown)
        {
            for (const auto& [slot, skillId] : skill::rules::offlineKit(occupation))
                addSkill(skillId);
        }
        else if (!role->aiId.empty())
        {
            const size_t difficulty =
                aiDifficulty > 0 && static_cast<size_t>(aiDifficulty) <= role->aiId.size()
                    ? static_cast<size_t>(aiDifficulty - 1)
                    : role->aiId.size() - 1;
            addAi(role->aiId[difficulty]);
        }
    }

    // 返回已去重且按 id 升序的 Spine 资源列表。
    std::vector<int32_t> spines() const { return {m_spines.begin(), m_spines.end()}; }

private:
    // 仅收录 res_spine 表中真实存在的资源。
    void addSpine(int32_t resSpineId)
    {
        if (!validId(resSpineId))
            return;
        if (!conf::Config::instance().find<conf::ResSpineConfig>(resSpineId))
            return;
        m_spines.insert(resSpineId);
    }

    // 收录 Buff 自身 Spine 及其绑定的特殊能力与生成特效。
    void addBuff(int32_t buffId)
    {
        if (!validId(buffId) || !m_buffs.insert(buffId).second)
            return;
        const auto* buff = conf::Config::instance().find<conf::battle::BuffDef>(buffId);
        if (!buff)
            return;
        addSpine(buff->presentation.spineId);
        addOps(buff->persistent);
        addOps(buff->onEnter);
        addOps(buff->onTick);
        addOps(buff->onBegin);
        addOps(buff->onEnd);
        addOps(buff->onExit);
        addOps(buff->watch);
        addOps(buff->choicePool);
    }

    // 遍历技能 op 中的生成特效、施加 Buff 与特殊能力。
    void addOps(const std::vector<conf::battle::BuffOp>& ops)
    {
        for (const auto& op : ops)
        {
            if (op.kind == conf::battle::BuffOpKind::SpawnEffect)
            {
                addEffect(op.i0);
                for (const int32_t id : op.ids)
                    addEffect(id);
            }
            else if (op.kind == conf::battle::BuffOpKind::Apply)
            {
                addBuff(op.i0);
                for (const int32_t id : op.ids)
                    addBuff(id);
            }
            else if (op.kind == conf::battle::BuffOpKind::Ability)
            {
                addAbility(op.i0);
                for (const int32_t id : op.ids)
                    addAbility(id);
            }
        }
    }

    // 沿 next 展开一条技能链上的全部片段。
    void addSkill(int32_t skillId)
    {
        while (validId(skillId) && m_skills.insert(skillId).second)
        {
            const auto* skill = conf::Config::instance().find<conf::battle::SkillDef>(skillId);
            if (!skill)
                return;
            addClipGroups(skill->sequences);
            addClipGroups(skill->releaseSequences);
            skillId = skill->next;
        }
    }

    // 展开技能动作二维数组。
    void addClipGroups(const std::vector<std::vector<int32_t>>& groups)
    {
        for (const auto& group : groups)
            for (const int32_t clipId : group)
                addClip(clipId);
    }

    // 收录角色动作生成的特效、附加 Buff 与召唤。
    void addClip(int32_t clipId)
    {
        if (!validId(clipId))
            return;
        const auto* clip = conf::Config::instance().find<conf::battle::ClipDef>(clipId);
        if (!clip)
            return;
        for (const int32_t buffId : clip->clipBuffs)
            addBuff(buffId);
        addCues(clip->cues, false);
    }

    // 收录特效动作的子特效，并展开召唤角色。
    void addEffectClip(int32_t clipId)
    {
        if (!validId(clipId))
            return;
        const auto* clip = conf::Config::instance().find<conf::battle::EffectClipDef>(clipId);
        if (!clip)
            return;
        addCues(clip->cues, true);
    }

    // 遍历片段 cue：生成特效、演出 Spine、变身与召唤。
    void addCues(const std::vector<conf::battle::ClipCue>& cues, bool effectClip)
    {
        for (const auto& cue : cues)
        {
            if (cue.kind == conf::battle::ClipCueKind::SpawnEffect)
                addEffect(cue.effectId);
            else if (cue.kind == conf::battle::ClipCueKind::DisplaySpine)
            {
                for (const auto& group : cue.displaySpineGroups)
                    for (const int32_t spineId : group)
                        addSpine(spineId);
            }
            else if (cue.kind == conf::battle::ClipCueKind::Transform)
                addRole(cue.transformId, 0);
            else if (cue.kind == conf::battle::ClipCueKind::Summon)
                addRole(cue.summonId, 0);
            else if (effectClip && cue.kind == conf::battle::ClipCueKind::None && validId(cue.effectId))
                addEffect(cue.effectId);
        }
    }

    // 递归收录特效本体、命中特效、后继特效和子动作。
    void addEffect(int32_t effectId)
    {
        if (!validId(effectId) || !m_effects.insert(effectId).second)
            return;
        const auto* effect = conf::Config::instance().find<conf::battle::EffectDef>(effectId);
        if (!effect)
            return;

        addSpine(effect->resSpineId);
        addEffect(effect->nextEffectId);
        for (const int32_t hitId : effect->hitEffectIds)
            addEffect(hitId);
        for (const int32_t clipId : effect->clips)
            addEffectClip(clipId);
        for (const int32_t buffId : effect->debuffId)
            addBuff(buffId);
        if (const auto* all = std::get_if<int32_t>(&effect->buffAllId))
            addBuff(*all);
        else if (const auto* many = std::get_if<std::vector<int32_t>>(&effect->buffAllId))
            for (const int32_t buffId : *many)
                addBuff(buffId);
        for (const int32_t buffId : effect->buffId)
            addBuff(buffId);
        addAbility(effect->specialabilityId);
        addAbility(effect->specialabilityAllId);
    }

    // 收录特殊能力的表现 Spine 及其触发特效。
    void addAbility(int32_t abilityId)
    {
        if (!validId(abilityId) || !m_abilities.insert(abilityId).second)
            return;
        const auto* ability = conf::Config::instance().find<conf::battle::AbilityDef>(abilityId);
        if (!ability)
            return;
        addSpine(ability->presentation.spineId);
        addOps(ability->ops);
    }

    // 展开怪物 AI 技能组。
    void addAi(int32_t aiId)
    {
        const auto* ai = conf::Config::instance().find<conf::battle::AiDef>(aiId);
        if (!ai)
            return;
        const std::array<const std::vector<std::vector<int32_t>>*, 4> groups = {
            &ai->skillIds, &ai->crazySkillIds, &ai->joystickSkillIds, &ai->otherSkillIds};
        for (const auto* group : groups)
            for (const auto& slot : *group)
                for (const int32_t skillId : slot)
                    addSkill(skillId);
    }

    // 已收集的特效 Spine 资源 id。
    std::set<int32_t> m_spines;
    // 已展开的角色，避免召唤环。
    std::set<int32_t> m_roles;
    // 已展开的技能链节点。
    std::set<int32_t> m_skills;
    // 已展开的特效配置。
    std::set<int32_t> m_effects;
    // 已展开的 Buff。
    std::set<int32_t> m_buffs;
    // 已展开的特殊能力。
    std::set<int32_t> m_abilities;
};
}  // namespace

namespace view
{
// 当前已推进的逻辑帧号。
uint64_t worldFrame(const World& world)
{
    return world.frame();
}

// 当前逻辑帧的毫秒时长。
Fixed worldDtMs(const World& world)
{
    return world.ecs().singleton<WorldClock>().dtMs;
}

// 固定逻辑帧的毫秒数。
Fixed logicDtMs()
{
    return kLogicDtMs;
}

// 上一帧产生的表现事件。
const std::vector<PresentationEvent>& worldEvents(const World& world)
{
    return world.ecs().singleton<PresentationEvents>().events;
}

// 收集当前可视实体，跳过待回收与休眠实体。
void collectVisualEntities(const World& world, std::vector<VisualEntityView>& out)
{
    out.clear();
    const auto& ecs = world.ecs();
    ecs.each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (hidden(ecs, entity))
            return;
        VisualEntityView view;
        view.entity   = entity;
        view.kind     = visualKind(identity.kind);
        view.configId = identity.configId;
        if (const auto* transform = ecs.tryGet<TransformComponent>(entity))
        {
            view.position = transform->position;
            view.towardX  = transform->towardX;
            view.vectorZ  = transform->vectorZ;
        }
        if (const auto* motion = ecs.tryGet<MotionStateComponent>(entity))
        {
            view.motionFile = motion->motionFile;
            view.motionName = motion->motionName;
            view.entryId    = motion->entryId;
            view.loop       = motion->loop;
            view.timeMs     = motion->timeMs;
            view.actionScale = motion->rate;
            if (ecs.has<StatsComponent>(entity))
                view.actionScale *= ecs.get<StatsComponent>(entity).finalStat(202);
        }
        if (const auto* pause = ecs.tryGet<PauseComponent>(entity); pause && pause->paused)
            view.actionScale = {};
        if (const auto* rage = ecs.tryGet<RageStateComponent>(entity))
            view.partnerRageTransformed = rage->transformed;
        out.push_back(std::move(view));
    });
}

// 返回槽位对应的玩家实体；越界或不存在时返回空实体。
Entity playerEntity(const World& world, uint32_t slot)
{
    const auto& slots = world.ecs().singleton<PlayerSlots>().entities;
    const auto it     = slots.find(slot);
    if (it == slots.end())
        return {};
    return world.ecs().alive(it->second) ? it->second : Entity{};
}

// 收集当前可见角色与召唤物的世界坐标，供回滚纠偏采样。
void collectRolePositions(const World& world, std::vector<std::pair<Entity, FixedVec3>>& out)
{
    out.clear();
    const auto& ecs = world.ecs();
    ecs.each<IdentityComponent, TransformComponent>([&](Entity entity, const IdentityComponent& identity,
                                                        const TransformComponent& transform) {
        if (hidden(ecs, entity))
            return;
        const bool summon = ecs.has<SummonComponent>(entity) ||
                            identity.roleType == static_cast<int32_t>(EntityRoleType::kSummon);
        if (identity.kind != SpawnKind::Role && !summon)
            return;
        out.push_back({entity, transform.position});
    });
}

// 读取实体世界坐标；不可见或缺失变换时返回假。
bool readEntityPosition(const World& world, Entity entity, FixedVec3& outPos)
{
    const auto& ecs = world.ecs();
    if (!ecs.alive(entity) || hidden(ecs, entity))
        return false;
    const auto* transform = ecs.tryGet<TransformComponent>(entity);
    if (!transform)
        return false;
    outPos = transform->position;
    return true;
}

// 读取实体角色身份。
EntityRoleInfoView readEntityRoleInfo(const World& world, Entity entity)
{
    EntityRoleInfoView view;
    const auto& ecs = world.ecs();
    if (!ecs.alive(entity) || hidden(ecs, entity))
        return view;
    const auto* identity = ecs.tryGet<IdentityComponent>(entity);
    if (!identity)
        return view;
    const bool summon = ecs.has<SummonComponent>(entity) ||
                        identity->roleType == static_cast<int32_t>(EntityRoleType::kSummon);
    if (identity->kind != SpawnKind::Role && !summon)
        return view;
    view.valid    = true;
    view.roleId   = identity->configId;
    view.roleType = identity->roleType;
    view.camp     = identity->camp;
    if (const auto* rage = ecs.tryGet<RageStateComponent>(entity))
        view.partnerRageTransformed = rage->transformed;
    return view;
}

// 立即生成城镇可控角色并返回实体。
Entity spawnTownRole(World& world, int32_t roleId, int32_t level, const FixedVec3& position, int32_t towardX,
                     std::optional<uint32_t> playerSlot)
{
    return materializeTownRole(world, roleId, level, position, towardX, playerSlot);
}

// 立即生成城镇远端木偶并返回实体。
Entity spawnTownPuppet(World& world, int64_t playerId, int32_t roleId, const FixedVec3& position, int32_t towardX)
{
    static_cast<void>(playerId);
    return materializeTownRole(world, roleId, 1, position, towardX, std::nullopt);
}

// 立即生成城镇常开传送门并返回实体。
Entity spawnTownPortal(World& world, int32_t portalId, const FixedVec3& position)
{
    const auto* row = conf::Config::instance().find<conf::PortalConfig>(portalId);
    if (!row || portalId <= 0)
        return {};
    TransformComponent transform;
    transform.position = position;
    const Entity entity = room::rules::spawnPortal(world, portalId, transform, 0, 1, 0, {});
    if (entity.value == 0)
        return {};
    world.ecs().get<PortalStateComponent>(entity).phase = PortalPhase::Active;
    auto& motion      = world.ecs().get<MotionStateComponent>(entity);
    motion.motionName = room::rules::portalMotion(*row, PortalPhase::Active);
    motion.loop       = room::rules::portalLoop(*row, PortalPhase::Active, true);
    motion.playing    = true;
    return entity;
}

// 从玩家槽位移除并销毁城镇实体。
void destroyTownEntity(World& world, Entity entity)
{
    auto& slots = world.ecs().singleton<PlayerSlots>().entities;
    for (auto it = slots.begin(); it != slots.end();)
    {
        if (it->second == entity)
            it = slots.erase(it);
        else
            ++it;
    }
    world.ecs().destroy(entity);
    world.ecs().flushDestroyed();
}

// 读取城镇实体移动状态。
TownMovementView readTownMovement(const World& world, Entity entity)
{
    TownMovementView view;
    const auto& ecs = world.ecs();
    if (!ecs.alive(entity) || hidden(ecs, entity))
        return view;
    const auto* transform = ecs.tryGet<TransformComponent>(entity);
    if (!transform)
        return view;
    view.valid    = true;
    view.position = transform->position;
    view.towardX  = transform->towardX;
    if (const auto* displacement = ecs.tryGet<DisplacementComponent>(entity))
        view.velocity = displacement->velocity;
    if (const auto* motion = ecs.tryGet<MotionStateComponent>(entity))
        view.motionName = motion->motionName;
    if (const auto* state = ecs.tryGet<RoleStateComponent>(entity))
        view.moving = state->state == RoleStateId::Walk || state->state == RoleStateId::Run;
    return view;
}

// 写入城镇实体世界坐标。
void setTownEntityPosition(World& world, Entity entity, const FixedVec3& position)
{
    if (!world.ecs().alive(entity))
        return;
    if (auto* transform = world.ecs().tryGet<TransformComponent>(entity))
        transform->position = position;
}

// 收集角色战斗中会引用的特效 Spine 资源 id，升序去重。
std::vector<int32_t> collectRoleEffectSpineIds(int32_t roleId, int32_t aiDifficulty)
{
    SpineCollector collector;
    collector.addRole(roleId, aiDifficulty);
    return collector.spines();
}
}  // namespace view
}  // namespace mugen::sim
