#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/conf/GameDef.h"

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
    bool m_pvp = false;
    // 场景驱动提供的战斗类型编号，供效果区域过滤使用，不从房间主键猜测。
    int32_t m_fightType = 0;
    // 为假时角色更新跳过怪物 AI。写入回放与快照，保证重放一致。
    bool m_aiEnabled = true;
    // 玩家职业；未知时按 m_playerRoleId 直接出生。
    CharacterClass m_playerClass = CharacterClass::kUnknown;
    // 玩家角色配置标识；用于按角色配置直接出生。
    int32_t m_playerRoleId = 0;
    // 玩家等级。
    int32_t m_playerLevel = 1;
    // 可选覆盖角色默认阵营；0 表示使用角色配置默认值。
    int32_t m_playerCamp = 0;
    // 玩家 AI 难度。
    int32_t m_playerAiDifficulty = 1;
    // 玩家槽位。
    uint32_t m_playerSlot = 0;
    // 玩家是否由 AI 驱动。
    bool m_playerIsAi = false;
    // 进入房间时使用的关卡标识。
    int32_t m_stageId = 0;
    // 进入房间时使用的副本标识。
    int32_t m_copyId = 0;
    // 为真时，调用方应使用 ScenarioHarness 的房间钩子驱动。
    bool m_useScenarioHooks = false;
    // 参与世界配置归档的字段。
    MG_REFLECT(WorldSetup,
               seed,
               mode,
               roomId,
               mapKey,
               m_pvp,
               m_fightType,
               m_aiEnabled,
               m_playerClass,
               m_playerRoleId,
               m_playerLevel,
               m_playerCamp,
               m_playerAiDifficulty,
               m_playerSlot,
               m_playerIsAi,
               m_stageId,
               m_copyId,
               m_useScenarioHooks)
};
// 固定逻辑帧的秒数。
inline constexpr Fixed kLogicDtSec = "0.03333332"_fx;
// 固定逻辑帧的毫秒数。
inline constexpr Fixed kLogicDtMs = "33.33332"_fx;
}  // namespace mugen::sim
