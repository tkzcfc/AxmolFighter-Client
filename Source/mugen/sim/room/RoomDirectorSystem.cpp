// [10.1] RoomDirectorSystem
// 读：`DeathEvent`、`RoomRequest`、`RoomState`、`IdentityComponent`、`HealthComponent`、`RoleStateComponent`
// 写：`RoomState`、`SlowMotion`(KO 慢放)、复活、怪物清场
// 消费：`RoomRequest`
// 产生：`RoomEvent`、`SpawnRequest`(波次)、`CueEvent`、`RoomEvent`(PvpEnded)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include <map>

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
    const auto& slots = world.ecs().singleton<PlayerSlots>().entities;
    return slots.empty() ? Entity{} : slots.begin()->second;
}
// 实体是否与主控玩家敌对。
bool isEnemy(World& world, const IdentityComponent& identity)
{
    const Entity own = hero(world);
    if (own.value == 0)
        return false;
    const auto& heroIdentity = world.ecs().get<IdentityComponent>(own);
    return (heroIdentity.hostileMask & identity.camp) != 0;
}
// 房间是否还有需要清场的敌人（怪物 / 精英 / Boss / 矿石，移植旧实现 checkWipeOutEnemys）。
bool enemiesAlive(World& world)
{
    bool alive = false;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        const int32_t type = identity.roleType;
        if (isEnemy(world, identity) && ((type >= 2 && type <= 4) || (type >= 7 && type <= 9)))
            alive = true;
    });
    return alive;
}
// 去重地排队进入状态，每次切换等待 30 毫秒（移植旧实现 setExpectRoomState）。
void expectState(RoomState& room, mugen::RoomState state)
{
    if (std::find(room.recorded.begin(), room.recorded.end(), state) != room.recorded.end())
        return;
    room.recorded.push_back(state);
    room.pending.push_back(state);
    if (room.pending.size() == 1)
    {
        room.waitMs = Fixed::fromInt(30);
        room.state  = mugen::RoomState::Check;
    }
}
// 进入单个状态，只产生房间事实与本地副作用（移植旧实现 _enterState）。
void enterState(World& world, RoomState& room, mugen::RoomState state)
{
    auto& queue = world.ecs().singleton<EventQueue<RoomEvent>>();
    room.state = state;
    RoomEvent event;
    event.kind    = RoomEventKind::StateChanged;
    event.payload = static_cast<int32_t>(state);
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
        for (const auto& [slot, entity] : world.ecs().singleton<PlayerSlots>().entities)
        {
            const auto* state = world.ecs().tryGet<RoleStateComponent>(entity);
            auto* motion      = world.ecs().tryGet<MotionStateComponent>(entity);
            if (state && motion && state->state == RoleStateId::Idle &&
                motion->motionName != role::motionName(RoleStateId::Idle))
            {
                motion->motionName     = role::motionName(RoleStateId::Idle);
                motion->timeMs         = {};
                motion->completedLoops = 0;
                motion->loop           = true;
            }
        }
        expectState(room, mugen::RoomState::ReadySpine);
        break;
    case mugen::RoomState::ReadySpine:
        if (at(room.config.isWarning, 0, 0) == 1)
        {
            RoomEvent warning;
            warning.kind    = RoomEventKind::Warning;
            warning.payload = 1;
            queue.push(warning);
        }
        // 警告确认流程（isWarning[1] != 0）未实现暂停，直接推进到 Tip。
        expectState(room, mugen::RoomState::Tip);
        break;
    case mugen::RoomState::Tip:
        if (room.config.openTips != -1)
        {
            CueEvent tips;
            tips.kind = CueKind::Tip;
            tips.id   = room.config.openTips;
            world.ecs().singleton<EventQueue<CueEvent>>().push(tips);
        }
        if (room.config.sceneName > 0)
        {
            CueEvent scene;
            scene.kind = CueKind::Tip;
            scene.id   = room.config.sceneName;
            world.ecs().singleton<EventQueue<CueEvent>>().push(scene);
        }
        expectState(room, mugen::RoomState::Process);
        break;
    case mugen::RoomState::FightFailure:
        room.failure = true;
        {
            RoomEvent fail;
            fail.kind    = RoomEventKind::Failure;
            fail.payload = room.roomId;
            queue.push(fail);
        }
        expectState(room, mugen::RoomState::FightBalance);
        break;
    case mugen::RoomState::FightVictory:
        room.victory = true;
        {
            RoomEvent win;
            win.kind    = RoomEventKind::Victory;
            win.payload = room.roomId;
            queue.push(win);
        }
        expectState(room, room.roomType == 0 ? mugen::RoomState::FightEnd : mugen::RoomState::FightBalance);
        break;
    case mugen::RoomState::FightBalance:
        if (room.victory || (room.pvp && room.failure))
            expectState(room, mugen::RoomState::FightFinish);
        break;
    case mugen::RoomState::FightFinish:
        expectState(room, mugen::RoomState::FightEnd);
        break;
    case mugen::RoomState::FightEnd:
        if (room.victory)
        {
            if (std::find(room.clearedRooms.begin(), room.clearedRooms.end(), room.roomId) ==
                room.clearedRooms.end())
                room.clearedRooms.push_back(room.roomId);
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
    room.ko = true;
    world.ecs().singleton<SlowMotion>().ko.start();
    RoomEvent event;
    event.kind    = RoomEventKind::StateChanged;
    event.payload = static_cast<int32_t>(mugen::RoomState::FightKo);
    world.ecs().singleton<EventQueue<RoomEvent>>().push(event);
    world.ecs().each<IdentityComponent, HealthComponent, RoleStateComponent>(
        [&](Entity entity, const IdentityComponent& identity, HealthComponent& health, RoleStateComponent& state) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (!isEnemy(world, identity))
                health.beHitEnabled = false;
            else if (identity.roleType == 2 || identity.roleType == 3 || identity.roleType == 5)
            {
                health.hp   = {};
                health.dead = true;
                state.deathRequested = true;
            }
        });
}
// 复活主控玩家：回满 MP/EP、清 CD、挂无敌 Buff，并置复活请求位；状态转移 Death→Revive→Idle 交给 FSM（移植旧实现 Revive）。
void reviveHero(World& world, RoomState& room, Entity entity)
{
    auto& health = world.ecs().get<HealthComponent>(entity);
    health.mp   = health.maxMp;
    health.ep   = health.maxEp;
    auto& state = world.ecs().get<RoleStateComponent>(entity);
    state.reviveRequested = true;
    if (auto* book = world.ecs().tryGet<SkillBookComponent>(entity))
        for (auto& entry : book->skills)
        {
            entry.cd           = {};
            entry.timing       = false;
            entry.releaseCount = entry.releaseMax;
        }
    if (auto* cast = world.ecs().tryGet<SkillCastComponent>(entity))
    {
        cast->skill     = kNoSkill;
        cast->nextSkill = kNoSkill;
        cast->switched    = false;
    }
    buff::rules::admit(world, entity, 202);
    ++room.usedRevives;
    room.reviving = false;
}
// 推进 PVP 决斗：先判定 KO（一方阵营英雄被清空），再判定超时（按剩余生命比例）。
void updatePvp(World& world, RoomState& room)
{
    room.roleDeaths.clear();
    room.gameSeconds += world.ecs().singleton<WorldClock>().dtSec;
    if (room.failure || room.victory)
        return;
    if (room.ko)
    {
        if (!world.ecs().singleton<SlowMotion>().ko.active)
        {
            const int32_t localCamp =
                hero(world).value != 0 ? world.ecs().get<IdentityComponent>(hero(world)).camp : 0;
            expectState(room, room.winnerCamp != 0 && room.winnerCamp == localCamp ? mugen::RoomState::FightVictory
                                                                                  : mugen::RoomState::FightFailure);
        }
        return;
    }
    std::map<int32_t, int32_t> living, present;
    std::map<int32_t, Fixed> hpSum, maxHpSum;
    world.ecs().each<IdentityComponent, RoleStateComponent, HealthComponent>(
        [&](Entity entity, const IdentityComponent& identity, const RoleStateComponent& state,
            const HealthComponent& health) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (identity.roleType != 1 || !identity.playerSlot.has_value())
                return;
            ++present[identity.camp];
            if (state.state != RoleStateId::Death && !state.deathRequested && health.hp > Fixed{})
                ++living[identity.camp];
            hpSum[identity.camp] += health.hp;
            maxHpSum[identity.camp] += health.maxHp;
        });
    std::map<int32_t, Fixed> ratios;
    for (const auto& [camp, hp] : hpSum)
        ratios[camp] = maxHpSum[camp] > Fixed{} ? hp / maxHpSum[camp] : Fixed{};
    int32_t winnerCamp   = 0;
    int32_t aliveCamps   = 0;
    bool anyEliminated   = false;
    for (const auto& [camp, count] : present)
    {
        if (living[camp] > 0)
        {
            ++aliveCamps;
            winnerCamp = camp;
        }
        else if (count > 0)
            anyEliminated = true;
    }
    if (anyEliminated && aliveCamps == 1)
    {
        room.ko         = true;
        room.winnerCamp = winnerCamp;
        world.ecs().singleton<SlowMotion>().ko.start();
        world.ecs().singleton<EventQueue<RoomEvent>>().push(
            RoomEvent{RoomEventKind::StateChanged, static_cast<int32_t>(mugen::RoomState::FightKo), 0, {}});
        world.ecs().each<IdentityComponent, HealthComponent>(
            [&](Entity entity, const IdentityComponent& identity, HealthComponent& health) {
                if (world.ecs().has<DespawningComponent>(entity))
                    return;
                if (identity.roleType == 1)
                    health.beHitEnabled = false;
            });
        world.ecs().singleton<EventQueue<RoomEvent>>().push(
            RoomEvent{RoomEventKind::PvpEnded, winnerCamp, static_cast<int32_t>(PvpRoundEndReason::Ko), {}});
        return;
    }
    if (room.gameSeconds > room.pvpTimeLimit)
    {
        int32_t timeoutWinner = 0;
        Fixed bestRatio;
        for (const auto& [camp, ratio] : ratios)
        {
            if (present[camp] == 0)
                continue;
            if (ratio > bestRatio)
            {
                bestRatio     = ratio;
                timeoutWinner = camp;
            }
            else if (ratio == bestRatio)
                timeoutWinner = 0;
        }
        room.winnerCamp = timeoutWinner;
        world.ecs().singleton<EventQueue<RoomEvent>>().push(
            RoomEvent{RoomEventKind::PvpEnded, timeoutWinner, static_cast<int32_t>(PvpRoundEndReason::Timeout), {}});
        const int32_t localCamp =
            hero(world).value != 0 ? world.ecs().get<IdentityComponent>(hero(world)).camp : 0;
        expectState(room, timeoutWinner != 0 && timeoutWinner == localCamp ? mugen::RoomState::FightVictory
                                                                          : mugen::RoomState::FightFailure);
    }
}
}  // namespace

// 系统 RoomDirectorSystem：10.1 行，见 DESIGN.md §3.2。推进房间状态机、KO、波次、复活与胜负结算。
void runRoomDirector(World& world)
{
    auto& room = world.ecs().singleton<RoomState>();
    if (!room.enabled || room.paused || room.returnToTown)
        return;

    // 1. 状态切换队列（30ms 一次）。
    if (!room.pending.empty())
    {
        room.waitMs -= world.ecs().singleton<WorldClock>().dtMs;
        if (room.waitMs <= Fixed{})
        {
            const mugen::RoomState next = room.pending.front();
            room.pending.erase(room.pending.begin());
            enterState(world, room, next);
            room.waitMs = Fixed::fromInt(30);
        }
    }

    // 2. 累积本帧死亡事实到持久列表（旧实现由死亡流程写 roleDeaths，供 Process 阶段消费）。
    for (const DeathEvent& event : world.ecs().singleton<EventQueue<DeathEvent>>().items)
        room.roleDeaths.push_back(event.entity);

    // 3. 仅 Process 状态推进战斗结算。
    if (room.state != mugen::RoomState::Process || room.failure || room.victory)
        return;

    // 4. 复活 / 回城 / 警告确认请求。
    auto& requests = world.ecs().singleton<EventQueue<RoomRequest>>().items;
    for (const RoomRequest& request : requests)
    {
        if (request.kind == RoomRequestKind::Revive && room.reviving && !room.failure && !room.pvp &&
            room.usedRevives < room.maxRevives)
        {
            const Entity own = hero(world);
            if (own.value != 0 && world.ecs().has<RoleStateComponent>(own) &&
                world.ecs().get<RoleStateComponent>(own).state == RoleStateId::Death)
                reviveHero(world, room, own);
        }
        else if (request.kind == RoomRequestKind::WarningDone && room.state == mugen::RoomState::ReadySpine)
        {
            expectState(room, mugen::RoomState::Tip);
        }
        else if (request.kind == RoomRequestKind::ReturnToTown)
        {
            room.returnToTown = true;
        }
    }
    if (room.returnToTown)
        return;
    if (room.pvp)
    {
        updatePvp(world, room);
        return;
    }

    // 5. Boss 清空触发 KO 慢放（保留「导演先于碰撞」的一帧延迟）。
    if (room.pendingKo)
    {
        room.pendingKo = false;
        triggerKo(world, room);
        return;
    }
    if (!room.ko && room.haveBoss && room.roomType != 0 && room.battleRuleType < 2)
    {
        bool bossAlive = false;
        world.ecs().each<IdentityComponent, HealthComponent>(
            [&](Entity entity, const IdentityComponent& identity, const HealthComponent& health) {
                if (world.ecs().has<DespawningComponent>(entity))
                    return;
                if (identity.roleType == 4 && health.hp > Fixed{})
                    bossAlive = true;
            });
        if (!bossAlive)
            room.pendingKo = true;
    }

    // 6. 英雄死亡 → 复活等待或失败。
    const Entity own = hero(world);
    const bool heroDied =
        own.value != 0 && std::find(room.roleDeaths.begin(), room.roleDeaths.end(), own) != room.roleDeaths.end();
    room.roleDeaths.clear();
    if (heroDied && !room.reviving)
    {
        if (room.usedRevives >= room.maxRevives)
            expectState(room, mugen::RoomState::FightFailure);
        else
        {
            room.reviving = true;
            RoomEvent event;
            event.kind    = RoomEventKind::StateChanged;
            event.payload = static_cast<int32_t>(mugen::RoomState::FightRevive);
            world.ecs().singleton<EventQueue<RoomEvent>>().push(event);
        }
    }

    // 7. 波次推进（战斗规则 2：清场后 2000ms 生成下一波）。
    bool waveSpawned = false;
    if (room.waveReady)
    {
        room.waveReady = false;
        room.waveTimerMs = {};
        waveSpawned        = true;
        const int32_t id = room.config.battleRuleParam.at(static_cast<size_t>(room.wave++));
        if (const auto* wave = conf::Config::instance().find<conf::RoomConfig>(id))
        {
            for (size_t i = 0; i < wave->monsterIds.size(); ++i)
            {
                const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(wave->monsterIds[i]);
                if (!row || wave->monsterIds[i] <= 0)
                    continue;
                RoleSpawnParams params;
                params.roleId       = row->id;
                params.isAi         = true;
                params.aiDifficulty = wave->monsterAiDifficulty;
                const int32_t type    = row->roleType;
                const size_t levelIndex = type == 6 || type == 10 || type == 12 ? 0 : static_cast<size_t>(type - 2);
                params.level        = (levelIndex < wave->monsterLevels.size() ? wave->monsterLevels[levelIndex] : 0) +
                                         room.levelBonus;
                params.position     = {Fixed::fromInt(i < wave->monsterPosX.size() ? wave->monsterPosX[i] : 0), {},
                                         Fixed::fromInt(i < wave->monsterPosZ.size() ? wave->monsterPosZ[i] : 0)};
                params.toward       = i < wave->monsterVectorX.size() ? wave->monsterVectorX[i] : 1;
                role::rules::spawnRole(world, params);
                room.haveBoss |= type == 4;
            }
        }
        else
        {
            MG_LOG_E("Wave room not found: {}", id);
            expectState(room, mugen::RoomState::FightFailure);
        }
    }

    // 8. 清场 / 限时判定。刚生成波次时本帧跳过（新怪物下一同步点才实体化）。
    if (!waveSpawned && !world.ecs().singleton<SlowMotion>().ko.active)
    {
        bool pass          = !enemiesAlive(world);
        const int32_t rule = room.battleRuleType;
        if (rule == 2 && pass && room.wave < static_cast<int32_t>(room.config.battleRuleParam.size()))
        {
            pass = false;
            if (room.waveTimerMs == Fixed{})
                room.waveTimerMs = Fixed::fromInt(2000);
        }
        if (rule == 3)
            pass = pass || room.gameSeconds >= room.limitSeconds;
        if (rule == 1 && room.limitSeconds > Fixed{} && room.gameSeconds > room.limitSeconds)
            expectState(room, mugen::RoomState::FightFailure);
        else if (pass)
            expectState(room, mugen::RoomState::FightVictory);
    }

    // 9. 波次倒计时。
    if (room.waveTimerMs > Fixed{})
    {
        room.waveTimerMs -= world.ecs().singleton<WorldClock>().dtMs;
        if (room.waveTimerMs <= Fixed{})
        {
            room.waveTimerMs = {};
            room.waveReady   = true;
        }
    }

    room.gameSeconds += world.ecs().singleton<WorldClock>().dtSec;
}
}  // namespace mugen::sim
