// [10.1] RoomDirectorSystem
// 读：`DeathEvent`、`RoomRequest`、`RoomState`、`IdentityComponent`、`HealthComponent`、`RoleStateComponent`
// 写：`RoomState`、`SlowMotion`(KO 慢放)、复活、怪物清场
// 消费：`RoomRequest`
// 产生：`RoomEvent`、`SpawnRequest`(波次)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"

namespace mugen::sim
{
namespace
{
// 读取配置数组中可省略的尾项。
int32_t at(const std::vector<int32_t>& values, size_t index, int32_t fallback)
{
    return index < values.size() ? values[index] : fallback;
}
// 主控玩家实体（首个槽位）。
Entity hero(World& world)
{
    const auto& slots = world.ecs().singleton<PlayerSlots>().m_entities;
    return slots.empty() ? Entity{} : slots.begin()->second;
}
// 实体是否与主控玩家敌对。
bool isEnemy(World& world, const IdentityComponent& identity)
{
    const Entity own = hero(world);
    if (own.value == 0)
        return false;
    const auto& heroIdentity = world.ecs().get<IdentityComponent>(own);
    return (heroIdentity.m_hostileMask & identity.m_camp) != 0;
}
// 房间是否还有需要清场的敌人（怪物 / 精英 / Boss / 矿石，移植旧实现 checkWipeOutEnemys）。
bool enemiesAlive(World& world)
{
    bool alive = false;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        const int32_t type = identity.m_roleType;
        if (isEnemy(world, identity) && ((type >= 2 && type <= 4) || (type >= 7 && type <= 9)))
            alive = true;
    });
    return alive;
}
// 去重地排队进入状态，每次切换等待 30 毫秒（移植旧实现 setExpectRoomState）。
void expectState(RoomState& room, mugen::RoomState state)
{
    if (std::find(room.m_recorded.begin(), room.m_recorded.end(), state) != room.m_recorded.end())
        return;
    room.m_recorded.push_back(state);
    room.m_pending.push_back(state);
    if (room.m_pending.size() == 1)
    {
        room.m_waitMs = Fixed::fromInt(30);
        room.m_state  = mugen::RoomState::Check;
    }
}
// 进入单个状态，只产生房间事实与本地副作用（移植旧实现 _enterState）。
void enterState(World& world, RoomState& room, mugen::RoomState state)
{
    auto& queue = world.ecs().singleton<EventQueue<RoomEvent>>();
    room.m_state = state;
    RoomEvent event;
    event.m_kind    = RoomEventKind::StateChanged;
    event.m_payload = static_cast<int32_t>(state);
    queue.push(event);
    switch (state)
    {
    case mugen::RoomState::Init:
        expectState(room, mugen::RoomState::Ready);
        break;
    case mugen::RoomState::Ready:
        expectState(room, mugen::RoomState::Enter);
        break;
    case mugen::RoomState::Enter:
        // 换房后遗留的移动动作在这里切回待机（对齐旧实现脑叶重入播放 stand 的时机）。
        for (const auto& [slot, entity] : world.ecs().singleton<PlayerSlots>().m_entities)
        {
            const auto* state = world.ecs().tryGet<RoleStateComponent>(entity);
            auto* motion      = world.ecs().tryGet<MotionStateComponent>(entity);
            if (state && motion && state->m_state == RoleStateId::Idle &&
                motion->m_motionName != role::motionName(RoleStateId::Idle))
            {
                motion->m_motionName     = role::motionName(RoleStateId::Idle);
                motion->m_timeMs         = {};
                motion->m_completedLoops = 0;
                motion->m_loop           = true;
            }
        }
        expectState(room, mugen::RoomState::ReadySpine);
        break;
    case mugen::RoomState::ReadySpine:
        if (at(room.m_config.isWarning, 0, 0) == 1)
        {
            RoomEvent warning;
            warning.m_kind    = RoomEventKind::Warning;
            warning.m_payload = 1;
            queue.push(warning);
        }
        // 警告确认流程（isWarning[1] != 0）未实现暂停，直接推进到 Tip。
        expectState(room, mugen::RoomState::Tip);
        break;
    case mugen::RoomState::Tip:
        expectState(room, mugen::RoomState::Process);
        break;
    case mugen::RoomState::FightFailure:
        room.m_failure = true;
        {
            RoomEvent fail;
            fail.m_kind    = RoomEventKind::Failure;
            fail.m_payload = room.m_roomId;
            queue.push(fail);
        }
        expectState(room, mugen::RoomState::FightBalance);
        break;
    case mugen::RoomState::FightVictory:
        room.m_victory = true;
        {
            RoomEvent win;
            win.m_kind    = RoomEventKind::Victory;
            win.m_payload = room.m_roomId;
            queue.push(win);
        }
        expectState(room, room.m_roomType == 0 ? mugen::RoomState::FightEnd : mugen::RoomState::FightBalance);
        break;
    case mugen::RoomState::FightBalance:
        if (room.m_victory || (room.m_pvp && room.m_failure))
            expectState(room, mugen::RoomState::FightFinish);
        break;
    case mugen::RoomState::FightFinish:
        expectState(room, mugen::RoomState::FightEnd);
        break;
    case mugen::RoomState::FightEnd:
        if (room.m_victory)
        {
            if (std::find(room.m_clearedRooms.begin(), room.m_clearedRooms.end(), room.m_roomId) ==
                room.m_clearedRooms.end())
                room.m_clearedRooms.push_back(room.m_roomId);
            room::rules::openPortals(world);
        }
        expectState(room, mugen::RoomState::End);
        break;
    default:
        break;
    }
}
// 触发 KO：慢放、清场杂兵并关闭友方受击（移植旧实现 KO 分支）。
void triggerKo(World& world, RoomState& room)
{
    room.m_ko = true;
    world.ecs().singleton<SlowMotion>().m_ko.start();
    RoomEvent event;
    event.m_kind    = RoomEventKind::StateChanged;
    event.m_payload = static_cast<int32_t>(mugen::RoomState::FightKo);
    world.ecs().singleton<EventQueue<RoomEvent>>().push(event);
    world.ecs().each<IdentityComponent, HealthComponent, RoleStateComponent>(
        [&](Entity entity, const IdentityComponent& identity, HealthComponent& health, RoleStateComponent& state) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (!isEnemy(world, identity))
                health.m_beHitEnabled = false;
            else if (identity.m_roleType == 2 || identity.m_roleType == 3 || identity.m_roleType == 5)
            {
                health.m_hp   = {};
                health.m_dead = true;
                state.m_deathRequested = true;
            }
        });
}
// 复活主控玩家：回满 MP/EP、清 CD、挂无敌 Buff，并置复活请求位；状态转移 Death→Revive→Idle 交给 FSM（移植旧实现 Revive）。
void reviveHero(World& world, RoomState& room, Entity entity)
{
    auto& health = world.ecs().get<HealthComponent>(entity);
    health.m_mp   = health.m_maxMp;
    health.m_ep   = health.m_maxEp;
    auto& state = world.ecs().get<RoleStateComponent>(entity);
    state.m_reviveRequested = true;
    if (auto* book = world.ecs().tryGet<SkillBookComponent>(entity))
        for (auto& entry : book->m_skills)
        {
            entry.m_cd           = {};
            entry.m_timing       = false;
            entry.m_releaseCount = entry.m_releaseMax;
        }
    if (auto* cast = world.ecs().tryGet<SkillCastComponent>(entity))
    {
        cast->m_skill     = kNoSkill;
        cast->m_nextSkill = kNoSkill;
        cast->m_switch    = false;
    }
    buff::rules::admit(world, entity, 202);
    ++room.m_usedRevives;
    room.m_reviving = false;
}
}  // namespace

// 系统 RoomDirectorSystem：10.1 行，见 DESIGN.md §3.2。推进房间状态机、KO、波次、复活与胜负结算。
void runRoomDirector(World& world)
{
    auto& room = world.ecs().singleton<RoomState>();
    if (!room.m_enabled || room.m_paused || room.m_returnToTown)
        return;

    // 1. 状态切换队列（30ms 一次）。
    if (!room.m_pending.empty())
    {
        room.m_waitMs -= world.ecs().singleton<WorldClock>().m_dtMs;
        if (room.m_waitMs <= Fixed{})
        {
            const mugen::RoomState next = room.m_pending.front();
            room.m_pending.erase(room.m_pending.begin());
            enterState(world, room, next);
            room.m_waitMs = Fixed::fromInt(30);
        }
    }

    // 2. 累积本帧死亡事实到持久列表（旧实现由死亡流程写 m_roleDeaths，供 Process 阶段消费）。
    for (const DeathEvent& event : world.ecs().singleton<EventQueue<DeathEvent>>().m_items)
        room.m_roleDeaths.push_back(event.m_entity);

    // 3. 仅 Process 状态推进战斗结算。
    if (room.m_state != mugen::RoomState::Process || room.m_failure || room.m_victory)
        return;

    // 4. 复活 / 回城 / 警告确认请求。
    auto& requests = world.ecs().singleton<EventQueue<RoomRequest>>().m_items;
    for (const RoomRequest& request : requests)
    {
        if (request.m_kind == RoomRequestKind::Revive && room.m_reviving && !room.m_failure && !room.m_pvp &&
            room.m_usedRevives < room.m_maxRevives)
        {
            const Entity own = hero(world);
            if (own.value != 0 && world.ecs().has<RoleStateComponent>(own) &&
                world.ecs().get<RoleStateComponent>(own).m_state == RoleStateId::Death)
                reviveHero(world, room, own);
        }
        else if (request.m_kind == RoomRequestKind::WarningDone && room.m_state == mugen::RoomState::ReadySpine)
        {
            expectState(room, mugen::RoomState::Tip);
        }
        else if (request.m_kind == RoomRequestKind::ReturnToTown)
        {
            room.m_returnToTown = true;
        }
    }

    // 5. Boss 清空触发 KO 慢放（保留「导演先于碰撞」的一帧延迟）。
    if (room.m_pendingKo)
    {
        room.m_pendingKo = false;
        triggerKo(world, room);
        return;
    }
    if (!room.m_ko && room.m_haveBoss && room.m_roomType != 0 && room.m_battleRuleType < 2)
    {
        bool bossAlive = false;
        world.ecs().each<IdentityComponent, HealthComponent>(
            [&](Entity entity, const IdentityComponent& identity, const HealthComponent& health) {
                if (world.ecs().has<DespawningComponent>(entity))
                    return;
                if (identity.m_roleType == 4 && health.m_hp > Fixed{})
                    bossAlive = true;
            });
        if (!bossAlive)
            room.m_pendingKo = true;
    }

    // 6. 英雄死亡 → 复活等待或失败。
    const Entity own = hero(world);
    const bool heroDied =
        own.value != 0 && std::find(room.m_roleDeaths.begin(), room.m_roleDeaths.end(), own) != room.m_roleDeaths.end();
    room.m_roleDeaths.clear();
    if (heroDied && !room.m_reviving)
    {
        if (room.m_usedRevives >= room.m_maxRevives)
            expectState(room, mugen::RoomState::FightFailure);
        else
        {
            room.m_reviving = true;
            RoomEvent event;
            event.m_kind    = RoomEventKind::StateChanged;
            event.m_payload = static_cast<int32_t>(mugen::RoomState::FightRevive);
            world.ecs().singleton<EventQueue<RoomEvent>>().push(event);
        }
    }

    // 7. 波次推进（战斗规则 2：清场后 2000ms 生成下一波）。
    bool waveSpawned = false;
    if (room.m_waveReady)
    {
        room.m_waveReady = false;
        room.m_waveTimerMs = {};
        waveSpawned        = true;
        const int32_t id = room.m_config.battleRuleParam.at(static_cast<size_t>(room.m_wave++));
        if (const auto* wave = conf::Config::instance().find<conf::RoomConfig>(id))
        {
            for (size_t i = 0; i < wave->monsterIds.size(); ++i)
            {
                const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(wave->monsterIds[i]);
                if (!row || wave->monsterIds[i] <= 0)
                    continue;
                RoleSpawnParams params;
                params.m_roleId       = row->id;
                params.m_isAi         = true;
                params.m_aiDifficulty = wave->monsterAiDifficulty;
                const int32_t type    = row->roleType;
                const size_t levelIndex = type == 6 || type == 10 || type == 12 ? 0 : static_cast<size_t>(type - 2);
                params.m_level        = (levelIndex < wave->monsterLevels.size() ? wave->monsterLevels[levelIndex] : 0) +
                                         room.m_levelBonus;
                params.m_position     = {Fixed::fromInt(i < wave->monsterPosX.size() ? wave->monsterPosX[i] : 0), {},
                                         Fixed::fromInt(i < wave->monsterPosZ.size() ? wave->monsterPosZ[i] : 0)};
                params.m_toward       = i < wave->monsterVectorX.size() ? wave->monsterVectorX[i] : 1;
                role::rules::spawnRole(world, params);
                room.m_haveBoss |= type == 4;
            }
        }
        else
        {
            MG_LOG_E("Wave room not found: {}", id);
            expectState(room, mugen::RoomState::FightFailure);
        }
    }

    // 8. 清场 / 限时判定。刚生成波次时本帧跳过（新怪物下一同步点才实体化）。
    if (!waveSpawned && !world.ecs().singleton<SlowMotion>().m_ko.m_active)
    {
        bool pass          = !enemiesAlive(world);
        const int32_t rule = room.m_battleRuleType;
        if (rule == 2 && pass && room.m_wave < static_cast<int32_t>(room.m_config.battleRuleParam.size()))
        {
            pass = false;
            if (room.m_waveTimerMs == Fixed{})
                room.m_waveTimerMs = Fixed::fromInt(2000);
        }
        if (rule == 3)
            pass = pass || room.m_gameSeconds >= room.m_limitSeconds;
        if (rule == 1 && room.m_limitSeconds > Fixed{} && room.m_gameSeconds > room.m_limitSeconds)
            expectState(room, mugen::RoomState::FightFailure);
        else if (pass)
            expectState(room, mugen::RoomState::FightVictory);
    }

    // 9. 波次倒计时。
    if (room.m_waveTimerMs > Fixed{})
    {
        room.m_waveTimerMs -= world.ecs().singleton<WorldClock>().m_dtMs;
        if (room.m_waveTimerMs <= Fixed{})
        {
            room.m_waveTimerMs = {};
            room.m_waveReady   = true;
        }
    }

    room.m_gameSeconds += world.ecs().singleton<WorldClock>().m_dtSec;
}
}  // namespace mugen::sim
