#pragma once
#include "mugen/core/reflect/Reflect.h"

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
    uint64_t seed  = 1;
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
    // 为假时角色更新跳过怪物 AI。驱动开关，不写入回放与快照。
    bool m_aiEnabled = true;
    // 参与世界配置归档的字段。m_aiEnabled 故意不在此列。
    MG_REFLECT(WorldSetup, seed, mode, roomId, mapKey, m_pvp, m_fightType)
};
// 固定逻辑帧的秒数。
inline constexpr Fixed kLogicDtSec = "0.03333332"_fx;
// 固定逻辑帧的毫秒数。
inline constexpr Fixed kLogicDtMs  = "33.33332"_fx;
}  // namespace mugen::sim
