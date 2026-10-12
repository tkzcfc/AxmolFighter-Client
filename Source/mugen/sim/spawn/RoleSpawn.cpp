#include "RoleSpawn.h"
#include "mugen/avatar/AvatarPaths.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/sim/buff/BuffPool.h"

namespace mugen::sim
{
namespace
{
// 将配置中的基础属性按运行时属性槽位顺序转换为 Fixed 数组。
std::array<Fixed, 16> values(const conf::AttributeConfig& a)
{
    return {Fixed::fromInt(a.sourceForce),
            Fixed::fromInt(a.agility),
            Fixed::fromInt(a.habitus),
            Fixed::fromInt(a.spirit),
            Fixed::fromInt(a.hp),
            Fixed::fromInt(a.atk),
            Fixed::fromInt(a.def),
            Fixed::fromInt(a.matk),
            Fixed::fromInt(a.mdef),
            Fixed::fromInt(a.crit),
            Fixed::fromInt(a.critResist),
            Fixed::fromInt(a.critDamage),
            Fixed::fromInt(a.critDamageResist),
            Fixed::fromInt(a.dodge),
            Fixed::fromInt(a.hit),
            Fixed::fromInt(a.baseDamage)};
}
// 将阵营位掩码转换为可碰撞的敌对阵营掩码。
int32_t hostile(int32_t camp)
{
    switch (camp)
    {
    case 1:
        return 30;
    case 2:
    case 4:
        return 25;
    case 8:
    case 16:
        return 7;
    default:
        MG_LOG_E("Invalid role camp");
        MG_ASSERT(false && "Invalid role camp");
        return 0;
    }
}
}  // namespace
// 按角色类型、等级和属性倍率计算初始战斗属性。
Attributes loadAttributes(const conf::RoleConfig& role, int32_t level)
{
    const bool validAttributes = level > 0 && level < 10000 && role.attributeRate.size() == 16;
    if (!validAttributes)
    {
        MG_LOG_E("Invalid role level or attribute rates");
        MG_ASSERT(validAttributes && "Invalid role level or attribute rates");
        return {};
    }
    const auto& config = conf::Config::instance();
    Attributes result;
    if (role.roleType == 1)
    {
        // Offline players have no account attribute payload; use the occupation's base curve.
        const auto* row = config.find<conf::AttributeConfig>(10000 + level);
        if (!row)
            row = &config.get<conf::AttributeConfig>(10000);
        result.basic = values(*row);
    }
    else
    {
        const int32_t base   = role.roleType == 3   ? 150000
                               : role.roleType == 4 ? 160000
                               : role.roleType == 5 ? 70000
                                                    : 140000;
        const auto initial   = values(config.get<conf::AttributeConfig>(140000));
        const auto& current  = config.get<conf::AttributeConfig>(140000 + level / 100);
        const auto cur       = values(current);
        const int32_t units  = level % 100;
        const auto& endpoint = units != 0 ? config.get<conf::AttributeConfig>(140001 + level / 100) : current;
        const auto next      = values(endpoint);
        for (size_t i = 0; i < result.basic.size(); ++i)
        {
            const size_t source = i == 4 ? 15 : i == 15 ? 4 : i;
            Fixed value         = initial[source];
            if (level / 100 == 0)
                value += next[source] * Fixed::fromInt(units) / Fixed::fromInt(100);
            else
                value += cur[source] + (next[source] - cur[source]) * Fixed::fromInt(units) / Fixed::fromInt(100);
            if (i == 4)
            {
                value += Fixed::fromInt(config.get<conf::SkillHurtConfig>((level + 9) / 10).hurt);
                value *= endpoint.monsterHitNumber;
                if (base + level > 160000)
                    value *= endpoint.bossHpRate;
                else if (base + level > 150000)
                    value *= endpoint.eliteHpRate;
                value = value.floor();
            }
            else if (i == 15)
            {
                value /= Fixed::fromInt(endpoint.playerHitNumber);
                if (base + level > 160000)
                    value *= endpoint.bossHurtRate;
                else if (base + level > 150000)
                    value *= endpoint.eliteHurtRate;
            }
            result.basic[i] = value;
        }
    }
    for (size_t i = 0; i < result.basic.size(); ++i)
        result.basic[i] *= role.attributeRate[i];
    return result;
}
// 依据配置创建战斗角色、技能池和行为树实例。
Entity spawnRole(World& world, const RoleSpawnParams& params)
{
    const auto& config = conf::Config::instance();
    const auto* row    = config.find<conf::RoleConfig>(params.roleId);
    if (!row)
    {
        MG_LOG_E("Invalid roleId in spawnRole");
        MG_ASSERT(row != nullptr && "Invalid roleId in spawnRole");
        return {};
    }
    const bool validFacing = params.toward == -1 || params.toward == 1;
    if (!validFacing)
    {
        MG_LOG_E("Invalid role facing");
        MG_ASSERT(validFacing && "Invalid role facing");
        return {};
    }
    const bool hasAi = !row->aiId.empty();
    if (!hasAi)
    {
        MG_LOG_E("Role has no AI configuration ids");
        MG_ASSERT(hasAi && "Role has no AI configuration ids");
        return {};
    }
    const size_t difficulty = params.aiDifficulty > 0 && static_cast<size_t>(params.aiDifficulty) <= row->aiId.size()
                                  ? static_cast<size_t>(params.aiDifficulty - 1)
                                  : row->aiId.size() - 1;
    const int32_t aiId      = row->aiId[difficulty];
    const auto* ai          = config.find<conf::AiConfig>(aiId);
    if (!ai)
        MG_LOG_W("Role {} references missing AI {}", params.roleId, aiId);
    const int32_t camp = params.camp.value_or(row->monsterCamps);
    RoleIdentity identity{params.roleId, row->roleType, params.level,     aiId, params.aiDifficulty, camp,
                          hostile(camp), params.isAi,   params.playerSlot, params.m_owner};
    RoleTimers timers;
    if (row->timeRage.size() > 2 && !row->timeRage[1].empty())
    {
        timers.timeRage  = Fixed::fromInt(row->timeRage[1][0]);
        timers.rageBuffs = row->timeRage[2];
    }
    auto attributes = loadAttributes(*row, params.level);
    if (row->roleType == static_cast<int32_t>(EntityRoleType::kSummon))
        if (const auto* owner = world.ecs().tryGet<Attributes>(params.m_owner))
            for (size_t i = 0; i < attributes.basic.size(); ++i)
                attributes.basic[i] = owner->value(i) * (i < row->attributeRate.size() ? row->attributeRate[i] : "1"_fx);
    Transform transform{params.position, params.toward, Fixed::fromInt(params.toward), {}, Fixed::fromInt(1)};
    MotionState motion;
    if (const auto* spine = config.find<conf::ResSpineConfig>(row->resSpineId))
    {
        motion.motionFile = AvatarPaths::motionFileFromSpine(spine->fileName);
        transform.scale   = spine->scale;
    }
    auto& ecs = world.ecs();
    const bool playerSlotFree =
        !params.playerSlot || !ecs.singleton<PlayerSlots>().entities.contains(*params.playerSlot);
    if (!playerSlotFree)
    {
        MG_LOG_E("Player slot is already occupied");
        MG_ASSERT(playerSlotFree && "Player slot is already occupied");
        return {};
    }
    const Entity entity = world.create(GroupKind::Role);
    ecs.add(entity, identity);
    ecs.add(entity, RoleStatus{});
    ecs.add(entity, timers);
    ecs.add(entity, RoleHitState{Fixed::fromInt(row->rigidity),
                                 row->weight,
                                 Fixed::fromInt(row->fatigue),
                                 row->hitCount,
                                 Fixed::fromInt(row->hitStiffTime),
                                 {}});
    ecs.add(entity, transform);
    ecs.add(entity, motion);
    ecs.add(entity, ColliderState{{}, {}, ecs.singleton<WorldClock>().frame, Fixed::fromInt(row->radius)});
    ecs.add(entity, attributes);
    Vitals vitals;
    vitals.hp = vitals.maxHp = attributes.maxHp();
    ecs.add(entity, vitals);
    ecs.add(entity, ExtraState{});
    ecs.add(entity, ControlEventQueue{});
    ecs.add(entity, Displacement{});
    RoleTree tree;
    ecs.add(entity, std::move(tree));
    if (world.setup().mode != WorldMode::Town)
    {
        const auto occupation = type_conversions::toCharacterClass(params.roleId);
        if (occupation != CharacterClass::kUnknown)
            skill::loadSkill(world, entity, skill::playerLoadout(skill::offlineKit(occupation)));
        else
            skill::loadSkill(world, entity);
    }
    if (params.isAi && world.setup().mode != WorldMode::Town && row->roleType != 12)
        ecs.add(entity, AiAgent{});
    auto& behaviorTree     = ecs.get<RoleTree>(entity);
    behaviorTree.defKey    = behavior::definitionKey(world, entity);
    const auto& definition = behavior::definition(behaviorTree.defKey);
    behaviorTree.instance  = BTInstance<behavior::LeafState>(definition);
    behavior::BTCtx context{world, entity};
    BTRunner<behavior::BTCtx, behavior::Param, behavior::LeafState>(definition, behavior::registry(),
                                                                    behaviorTree.instance)
        .enter(context);
    if (params.playerSlot.has_value())
        ecs.singleton<PlayerSlots>().entities.emplace(*params.playerSlot, entity);
    if (world.setup().mode != WorldMode::Town)
        buff::loadBuffs(world, entity, params.m_buffs);
    return entity;
}
// 将职业映射为角色配置后复用标准角色创建流程。
Entity spawnPlayer(World& world, CharacterClass occupation, RoleSpawnParams params)
{
    params.roleId = type_conversions::toRoleConfigId(occupation);
    return spawnRole(world, params);
}
// 创建由外部帧同步驱动、没有行为树的城镇木偶。
Entity spawnPuppet(World& world, int64_t playerId, int32_t roleId, FixedVec3 position, int32_t toward)
{
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(roleId);
    if (!row)
    {
        MG_LOG_E("Invalid roleId in spawnPuppet");
        MG_ASSERT(row != nullptr && "Invalid roleId in spawnPuppet");
        return {};
    }
    const bool validFacing = toward == -1 || toward == 1;
    if (!validFacing)
    {
        MG_LOG_E("Invalid puppet facing");
        MG_ASSERT(validFacing && "Invalid puppet facing");
        return {};
    }
    Transform transform{position, toward, Fixed::fromInt(toward), {}, Fixed::fromInt(1)};
    MotionState motion;
    if (const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(row->resSpineId))
    {
        motion.motionFile = AvatarPaths::motionFileFromSpine(spine->fileName);
        transform.scale   = spine->scale;
    }
    auto& ecs           = world.ecs();
    const Entity entity = world.create(GroupKind::Role);
    RoleIdentity identity;
    identity.roleId   = roleId;
    identity.roleType = row->roleType;
    ecs.add(entity, identity);
    ecs.add(entity, RoleStatus{});
    ecs.add(entity, transform);
    ecs.add(entity, std::move(motion));
    ecs.add(entity, Puppet{playerId});
    return entity;
}
}  // namespace mugen::sim
