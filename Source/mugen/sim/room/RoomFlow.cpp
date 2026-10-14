#include "RoomFlow.h"
#include "mugen/sim/World.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/skill/Skill.h"
#include <algorithm>

namespace mugen::sim::room
{
using State = mugen::RoomState;
namespace
{
// 暂停角色组，不暂停场景状态机。
void suspend(World& world, bool value)
{
    world.ecs().each<EntityGroup>([&](Entity, EntityGroup& group) {
        if (group.kind == GroupKind::Role) group.suspended = value;
    });
}
// 取得主控玩家实体。
Entity hero(World& world)
{
    const auto& slots = world.ecs().singleton<PlayerSlots>().entities;
    return slots.empty() ? Entity{} : slots.begin()->second;
}
// 按主控阵营的敌对掩码判断目标。
bool enemy(World& world, const RoleIdentity& identity)
{
    const auto* own = world.ecs().tryGet<RoleIdentity>(hero(world));
    return own && (own->hostileCamp & identity.camp) != 0;
}
// 读取配置数组中可省略的尾项。
int32_t value(const std::vector<int32_t>& values, size_t index, int32_t fallback)
{
    return index < values.size() ? values[index] : fallback;
}
// 使用主房间难度及等级生成一组坐标配置。
void spawn(World& world, const conf::RoomConfig& positions)
{
    auto& room = world.ecs().singleton<sim::RoomState>();
    for (size_t i = 0; i < positions.monsterIds.size(); ++i)
    {
        const auto* row = conf::Config::instance().find<conf::RoleConfig>(positions.monsterIds[i]);
        if (!row || positions.monsterIds[i] <= 0) continue;
        RoleSpawnParams params;
        params.roleId = row->id;
        params.isAi = true;
        params.aiDifficulty = room.m_config.monsterAiDifficulty;
        const int32_t type = row->roleType;
        const size_t levelIndex = type == 6 || type == 10 || type == 12 ? 0 : static_cast<size_t>(type - 2);
        params.level = value(room.m_config.monsterLevels, levelIndex, 0) + room.m_levelBonus;
        params.position = {Fixed::fromInt(value(positions.monsterPosX, i, 0)), {},
                           Fixed::fromInt(value(positions.monsterPosZ, i, 0))};
        params.toward = value(positions.monsterVectorX, i, 1);
        const auto entity = spawnRole(world, params);
        if (entity.value != 0)
        {
            DropGoods drops;
            const size_t index = type >= 2 ? static_cast<size_t>(type - 2) : 0;
            if (!room.m_config.monsterDropGoods.empty())
                drops.m_hpRate = Fixed::fromInt(value(room.m_config.monsterDropGoods[0], index, 0));
            if (room.m_config.monsterDropGoods.size() > 1)
                drops.m_epRate = Fixed::fromInt(value(room.m_config.monsterDropGoods[1], index, 0));
            world.ecs().add(entity, drops);
        }
        room.m_haveBoss |= type == 4;
    }
}
// 进入单个状态，结算仅产生本地事件。
void enterState(World& world, State state)
{
    auto& room = world.ecs().singleton<sim::RoomState>();
    room.m_state = state;
    world.emit({}, RoomStateChanged{static_cast<int32_t>(state)});
    switch (state)
    {
    case State::Init: setExpectRoomState(world, State::Ready); break;
    case State::Ready: suspend(world, false); setExpectRoomState(world, State::Enter); break;
    case State::Enter: setExpectRoomState(world, State::ReadySpine); break;
    case State::ReadySpine:
        if (value(room.m_config.isWarning, 0, 0) == 1) world.emit({}, RoomWarning{1, false});
        if (value(room.m_config.isWarning, 1, 0) != 0)
        {
            suspend(world, true);
            world.emit({}, RoomWarning{value(room.m_config.isWarning, 1, 0), true});
        }
        else setExpectRoomState(world, State::Tip);
        break;
    case State::Tip:
        if (room.m_config.openTips != -1) world.emit({}, Tips{std::to_string(room.m_config.openTips)});
        if (room.m_config.sceneName > 0) world.emit({}, Tips{std::to_string(room.m_config.sceneName)});
        setExpectRoomState(world, State::Process);
        break;
    case State::FightFailure:
        room.m_failure = true;
        suspend(world, true);
        world.emit({}, Failure{room.roomId});
        setExpectRoomState(world, State::FightBalance);
        break;
    case State::FightVictory:
        room.m_victory = true;
        world.emit({}, Victory{room.roomId});
        setExpectRoomState(world, room.m_config.roomType == 0 ? State::FightEnd : State::FightBalance);
        break;
    case State::FightBalance:
        if (room.m_victory) setExpectRoomState(world, State::FightFinish);
        break;
    case State::FightFinish: setExpectRoomState(world, State::FightEnd); break;
    case State::FightEnd:
        if (room.m_victory)
        {
            if (std::find(room.m_clearedRooms.begin(), room.m_clearedRooms.end(), room.roomId) == room.m_clearedRooms.end())
                room.m_clearedRooms.push_back(room.roomId);
            room_entities::openPortals(world);
        }
        setExpectRoomState(world, State::End);
        break;
    default: break;
    }
}
}
// 请求过的状态只排队一次，每次切换等待三十毫秒。
void setExpectRoomState(World& world, State state)
{
    auto& room = world.ecs().singleton<sim::RoomState>();
    if (std::find(room.m_recorded.begin(), room.m_recorded.end(), state) != room.m_recorded.end()) return;
    room.m_recorded.push_back(state);
    room.m_pending.push_back(state);
    if (room.m_pending.size() == 1)
    {
        room.m_waitMs = "30"_fx;
        room.m_state = State::Check;
    }
}
// 按副本预算与关卡限时初始化房间，并加载怪物和房间效果。
bool enter(World& world, const conf::RoomConfig& config, int32_t stageId, int32_t copyId)
{
    auto& room = world.ecs().singleton<sim::RoomState>();
    if (room.m_enabled || !world.ecs().alive(hero(world))) return false;
    room.m_enabled = true;
    room.m_stageId = stageId;
    room.m_copyId = copyId;
    room.m_config = config;
    room.roomId = config.id;
    if (const auto* copy = conf::Config::instance().find<conf::CopyConfig>(copyId))
    {
        room.m_maxRevives = copy->maxRevive;
        room.m_levelBonus = copy->addLevel;
    }
    if (const auto* stage = conf::Config::instance().find<conf::StageConfig>(stageId))
        if (config.battleRuleType == 1) room.m_limitSeconds = Fixed::fromInt(stage->openTime);
    if (config.battleRuleType == 3) room.m_limitSeconds = Fixed::fromInt(value(config.battleRuleParam, 0, 0));
    const auto players = world.ecs().singleton<PlayerSlots>().entities;
    size_t index = 0;
    for (const auto& [slot, entity] : players)
    {
        auto& transform = world.ecs().get<Transform>(entity);
        transform.position = {Fixed::fromInt(value(config.actorPosX, index, 0)), {}, Fixed::fromInt(value(config.actorPosZ, index, 0))};
        transform.towardX = value(config.actorVectorX, index++, 1) < 0 ? -1 : 1;
        for (int32_t id : config.actorBuffIds) if (id != -1) buff::addEntityBuff(world, entity, id);
    }
    if (std::find(room.m_clearedRooms.begin(), room.m_clearedRooms.end(), config.id) == room.m_clearedRooms.end())
        spawn(world, config);
    room_entities::load(world, config);
    suspend(world, true);
    setExpectRoomState(world, State::Init);
    return true;
}
// 清场等待死亡实体真正从角色组移除。
bool checkWipeOutEnemys(World& world)
{
    bool clear = true;
    world.ecs().each<RoleIdentity, EntityGroup>([&](Entity, const RoleIdentity& role, const EntityGroup&) {
        if (enemy(world, role) && ((role.roleType >= 2 && role.roleType <= 4) || (role.roleType >= 7 && role.roleType <= 9))) clear = false;
    });
    return clear;
}
// 复活只能由有效槽位输入触发，并交给已有角色动作执行。
void commands(World& world, const FrameInput& input)
{
    auto& room = world.ecs().singleton<sim::RoomState>();
    if (!room.m_enabled) return;
    for (const auto& command : input.systemCommands)
    {
        if (const auto* revive = std::get_if<ReviveCommand>(&command))
        {
            const auto& slots = world.ecs().singleton<PlayerSlots>().entities;
            const auto found = slots.find(revive->slot);
            if (found == slots.end() || !room.m_reviving || room.m_failure || room.m_usedRevives >= room.m_maxRevives) continue;
            if (world.ecs().get<RoleStatus>(found->second).isDeath())
            {
                world.ecs().get<RoleStatus>(found->second).eEntityStatus = EntityRoleStatus::Revive;
                auto& vitals = world.ecs().get<Vitals>(found->second);
                vitals.mp = vitals.maxMp;
                vitals.ep = vitals.maxEp;
                if (auto* pool = world.ecs().tryGet<SkillPool>(found->second))
                {
                    for (auto& entry : pool->m_skills)
                    {
                        entry.m_cd = {};
                        entry.m_timing = false;
                        entry.m_releaseCount = entry.m_releaseMax;
                        skill::refreshSkillPipe(entry);
                    }
                    skill::resetSkill(world, found->second);
                    buff::addEntityBuff(world, found->second, 202);
                }
                ++room.m_usedRevives;
                room.m_reviving = false;
                room.m_roleDeaths.clear();
                suspend(world, false);
            }
        }
        else if (const auto* done = std::get_if<RoomWarningDone>(&command))
        {
            if (done->m_roomId == room.roomId && room.m_state == State::ReadySpine)
            { suspend(world, false); setExpectRoomState(world, State::Tip); }
        }
        else if (const auto* back = std::get_if<ReturnToTownCommand>(&command))
        {
            if (world.ecs().singleton<PlayerSlots>().entities.contains(back->m_slot))
            { room.m_returnToTown = true; suspend(world, true); }
        }
    }
}
// 场景阶段依次推进状态、KO、死亡、结果和战斗计时。
void update(World& world)
{
    auto& room = world.ecs().singleton<sim::RoomState>();
    if (!room.m_enabled || room.paused || room.m_returnToTown) return;
    if (!room.m_pending.empty())
    {
        room.m_waitMs -= world.ecs().singleton<WorldClock>().dtMs;
        if (room.m_waitMs <= Fixed{})
        {
            const auto next = room.m_pending.front();
            room.m_pending.erase(room.m_pending.begin());
            enterState(world, next);
            room.m_waitMs = "30"_fx;
        }
    }
    if (room.m_state != State::Process || room.m_failure || room.m_victory) return;
    if (!room.m_reviving && !room.m_ko && room.m_haveBoss && room.m_config.roomType != 0 && room.m_config.battleRuleType < 2)
    {
        bool bossAlive = false;
        world.ecs().each<RoleIdentity, Vitals>([&](Entity, const RoleIdentity& role, const Vitals& vitals) {
            if (role.roleType == 4 && vitals.hp > Fixed{}) bossAlive = true;
        });
        if (!bossAlive)
        {
            room.m_ko = true;
            world.ecs().singleton<SlowMotion>().m_ko.start();
            world.emit({}, RoomStateChanged{static_cast<int32_t>(State::FightKo)});
            world.ecs().each<RoleIdentity, Vitals>([&](Entity entity, const RoleIdentity& role, Vitals& vitals) {
                if (!enemy(world, role)) vitals.m_beHitEnabled = false;
                else if (role.roleType == 2 || role.roleType == 3 || role.roleType == 5)
                { vitals.hp = {}; Role(world, entity).changeStatus(EntityRoleStatus::Death); }
            });
        }
    }
    if (!room.m_reviving && std::find(room.m_roleDeaths.begin(), room.m_roleDeaths.end(), hero(world)) != room.m_roleDeaths.end())
    {
        if (room.m_usedRevives >= room.m_maxRevives) setExpectRoomState(world, State::FightFailure);
        else
        {
            room.m_reviving = true;
            suspend(world, true);
            world.emit({}, RoomStateChanged{static_cast<int32_t>(State::FightRevive)});
        }
    }
    room.m_roleDeaths.clear();
    if (room.m_reviving || !room.m_pending.empty()) return;
    if (room.m_waveReady)
    {
        room.m_waveReady = false;
        room.m_waveTimer = 0;
        const int32_t id = room.m_config.battleRuleParam.at(static_cast<size_t>(room.m_wave++));
        if (const auto* wave = conf::Config::instance().find<conf::RoomConfig>(id)) spawn(world, *wave);
        else { MG_LOG_E("Wave room not found: {}", id); setExpectRoomState(world, State::FightFailure); return; }
    }
    if (!world.ecs().singleton<SlowMotion>().m_ko.m_active)
    {
        bool pass = checkWipeOutEnemys(world);
        const int32_t rule = room.m_config.battleRuleType;
        if (rule == 2 && pass && room.m_wave < static_cast<int32_t>(room.m_config.battleRuleParam.size()))
        {
            pass = false;
            if (room.m_waveTimer == 0) room.m_waveTimer = world.ecs().singleton<LogicTimers>().add({}, "2000"_fx, false, RoomWaveReady{room.roomId});
        }
        if (rule == 3) pass |= room.m_gameSeconds >= room.m_limitSeconds;
        if (rule == 1 && room.m_limitSeconds > Fixed{} && room.m_gameSeconds > room.m_limitSeconds)
            setExpectRoomState(world, State::FightFailure);
        else if (pass) setExpectRoomState(world, State::FightVictory);
    }
    room.m_gameSeconds += world.ecs().singleton<WorldClock>().dtSec;
}
}
