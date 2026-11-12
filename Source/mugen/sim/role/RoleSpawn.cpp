#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/MacroDefinition.h"

namespace mugen::sim
{
namespace
{
// 将配置中的基础属性按运行时属性槽位顺序转换为 Fixed 数组。
std::array<Fixed, kBaseStatCount> values(const conf::AttributeConfig& a)
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
}  // namespace

namespace role::rules
{
// 按角色类型、等级和属性倍率计算初始基础属性。
std::array<Fixed, kBaseStatCount> loadAttributes(const conf::battle::RoleDef& role, int32_t level)
{
    const bool valid = level > 0 && level < 10000 && role.attributeRate.size() == static_cast<size_t>(kBaseStatCount);
    if (!valid)
    {
        MG_LOG_E("Invalid role level or attribute rates");
        MG_ASSERT(valid && "Invalid role level or attribute rates");
        return {};
    }
    const auto& config = conf::Config::instance();
    std::array<Fixed, kBaseStatCount> result{};
    if (role.roleType == 1)
    {
        // 离线玩家无账号属性载荷，使用职业基础曲线。
        const auto* row = config.find<conf::AttributeConfig>(10000 + level);
        if (!row)
            row = &config.get<conf::AttributeConfig>(10000);
        result = values(*row);
    }
    else
    {
        const int32_t base  = role.roleType == 3   ? 150000
                              : role.roleType == 4 ? 160000
                              : role.roleType == 5 ? 70000
                                                   : 140000;
        const auto initial  = values(config.get<conf::AttributeConfig>(140000));
        const auto& current = config.get<conf::AttributeConfig>(140000 + level / 100);
        const auto cur      = values(current);
        const int32_t units = level % 100;
        const auto& endpoint = units != 0 ? config.get<conf::AttributeConfig>(140001 + level / 100) : current;
        const auto next      = values(endpoint);
        for (size_t i = 0; i < result.size(); ++i)
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
            result[i] = value;
        }
    }
    for (size_t i = 0; i < result.size(); ++i)
        result[i] *= role.attributeRate[i];
    return result;
}

// 将阵营位掩码转换为可碰撞的敌对阵营掩码。
int32_t hostileMask(int32_t camp)
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

// 按配置生成角色：追加 SpawnRequest 到队列。
void spawnRole(World& world, const RoleSpawnParams& params)
{
    const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(params.m_roleId);
    if (!row)
    {
        MG_LOG_E("Invalid roleId in spawnRole");
        MG_ASSERT(row != nullptr && "Invalid roleId in spawnRole");
        return;
    }
    SpawnRequest request;
    request.m_kind         = SpawnKind::Role;
    request.m_configId     = params.m_roleId;
    request.m_level        = params.m_level;
    request.m_camp         = params.m_camp.value_or(row->monsterCamps);
    request.m_aiDifficulty = params.m_aiDifficulty;
    request.m_playerSlot   = params.m_playerSlot.value_or(0);
    request.m_hasPlayerSlot = params.m_playerSlot.has_value();
    request.m_isAi         = params.m_isAi;
    request.m_owner        = params.m_owner;
    request.m_position     = params.m_position;
    request.m_towardX      = params.m_toward;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}
}  // namespace role::rules
}  // namespace mugen::sim
