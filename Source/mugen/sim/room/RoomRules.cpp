#include "mugen/sim/room/RoomRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 读取配置数组中可省略的尾项。
int32_t at(const std::vector<int32_t>& values, size_t index, int32_t fallback)
{
    return index < values.size() ? values[index] : fallback;
}
}  // namespace

namespace room::rules
{
// 进入战斗房间：定位玩家、生成怪物并初始化房间状态。
bool enter(World& world, const conf::RoomConfig& config, int32_t stageId, int32_t copyId)
{
    auto& room = world.ecs().singleton<RoomState>();
    if (room.m_enabled)
        return false;
    room.m_enabled    = true;
    room.m_roomId     = config.id;
    room.m_levelBonus = 0;
    room.m_roomType       = config.roomType;
    room.m_battleRuleType = config.battleRuleType;
    room.m_ko             = false;
    room.m_haveBoss       = false;
    if (const auto* copy = conf::Config::instance().find<conf::CopyConfig>(copyId))
        room.m_levelBonus = copy->addLevel;

    // 定位已出生的玩家（非 PVP 按 actorPosX/Z，朝向 actorVectorX）。
    size_t index = 0;
    for (const auto& [slot, entity] : world.ecs().singleton<PlayerSlots>().m_entities)
    {
        auto& transform    = world.ecs().get<Transform>(entity);
        transform.m_position = {Fixed::fromInt(at(config.actorPosX, index, 0)), {},
                                Fixed::fromInt(at(config.actorPosZ, index, 0))};
        transform.m_towardX  = at(config.actorVectorX, index, 1) < 0 ? -1 : 1;
        transform.m_vectorX  = Fixed::fromInt(transform.m_towardX);
        ++index;
    }

    // 生成怪物（移植旧实现 RoomDirector::_spawn）。
    for (size_t i = 0; i < config.monsterIds.size(); ++i)
    {
        const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(config.monsterIds[i]);
        if (!row || config.monsterIds[i] <= 0)
            continue;
        RoleSpawnParams params;
        params.m_roleId       = row->id;
        params.m_isAi         = true;
        params.m_aiDifficulty = config.monsterAiDifficulty;
        const int32_t type    = row->roleType;
        const size_t levelIndex = type == 6 || type == 10 || type == 12 ? 0 : static_cast<size_t>(type - 2);
        params.m_level        = at(config.monsterLevels, levelIndex, 0) + room.m_levelBonus;
        params.m_position     = {Fixed::fromInt(at(config.monsterPosX, i, 0)), {},
                                 Fixed::fromInt(at(config.monsterPosZ, i, 0))};
        params.m_toward       = at(config.monsterVectorX, i, 1);
        role::rules::spawnRole(world, params);
        room.m_haveBoss |= type == 4;
    }
    static_cast<void>(stageId);
    return true;
}
}  // namespace room::rules
}  // namespace mugen::sim
