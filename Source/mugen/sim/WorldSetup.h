#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/Fixed.h"
#include <vector>

namespace mugen::sim
{
// 世界运行模式，决定场景与角色流程。
enum class WorldMode : uint8_t
{
    // 战斗场景模式。
    Battle,
    // 城镇场景模式。
    Town
};
// 单个玩家槽位的出生配置。
struct PlayerSetup
{
    // 玩家槽位，须在世界内唯一。
    uint32_t slot = 0;
    // 玩家角色配置标识；职业一律由 roleId 推出。
    int32_t roleId = 0;
    // 玩家等级。
    int32_t level = 1;
    // 可选覆盖角色默认阵营；0 表示使用角色配置默认值。不得为中立阵营 1（特效无法命中）。
    int32_t camp = 0;
    // 玩家 AI 难度。
    int32_t aiDifficulty = 1;
    // 玩家是否由 AI 驱动。
    bool isAi = false;
    // 参与世界配置归档的字段。
    MG_REFLECT(PlayerSetup, slot, roleId, level, camp, aiDifficulty, isAi)
};
// 创建世界时写入回放和快照的固定参数。
struct WorldSetup
{
    // 唯一随机数发生器的初始种子。
    uint64_t seed = 1;
    // 本世界运行的模式。
    WorldMode mode = WorldMode::Battle;
    // 房间实例标识。
    int32_t roomId = 0;
    // 地图配置键。
    std::string mapKey;
    // 驱动明确指定竞技战斗，避免根据房间标识猜测规则。
    bool pvp = false;
    // 场景驱动提供的战斗类型编号，供效果区域过滤使用，不从房间主键猜测。
    int32_t fightType = 0;
    // PVP 决斗限时（秒），超时按剩余生命比例判定胜负。
    Fixed pvpTimeLimit = Fixed::fromInt(100);
    // PVP 玩家出生生命倍率，作用于基础生命与生命上限。
    Fixed pvpHpScale = Fixed::fromInt(1);
    // 为假时角色更新跳过怪物 AI。写入回放与快照，保证重放一致。
    bool aiEnabled = true;
    // 参与本场战斗的玩家槽位，按出生顺序保存。
    std::vector<PlayerSetup> players;
    // 进入房间时使用的关卡标识。
    int32_t stageId = 0;
    // 进入房间时使用的副本标识。
    int32_t copyId = 0;
    // 为真时，调用方应使用 ScenarioHarness 的房间钩子驱动。
    bool useScenarioHooks = false;
    // 参与世界配置归档的字段。
    MG_REFLECT(WorldSetup,
               seed,
               mode,
               roomId,
               mapKey,
               pvp,
               fightType,
               pvpTimeLimit,
               pvpHpScale,
               aiEnabled,
               players,
               stageId,
               copyId,
               useScenarioHooks)
};
// 固定逻辑帧的秒数。
inline constexpr Fixed kLogicDtSec = "0.03333332"_fx;
// 固定逻辑帧的毫秒数。
inline constexpr Fixed kLogicDtMs = "33.33332"_fx;
// 一场战斗允许的最大玩家槽位数（当前 1v1 决斗为 2，后续合作副本扩展至 4）。
inline constexpr int32_t kMaxBattlePlayers = 4;
}  // namespace mugen::sim
