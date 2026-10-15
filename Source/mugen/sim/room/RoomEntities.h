#pragma once
#include "mugen/sim/role/RoleComponents.h"
#include "mugen/sim/motion/Displacement.h"
#include "mugen/conf/tables/MapTables.h"

namespace mugen::sim
{
class World;
// 战斗传送门只保留关闭、开放和传送三个互斥阶段。
enum class PortalPhase : uint8_t { Close, Active, Transfer };
// 房间门保存本地目标，不执行网络场景切换。
struct PortalState
{
    // 门配置、目标房间、目标朝向和限制。
    int32_t m_id = 0, m_room = 0, m_toward = 1, m_limit = 0;
    // 到达位置及当前阶段。
    FixedVec3 m_destination;
    PortalPhase m_phase = PortalPhase::Close;
    MG_REFLECT(PortalState, m_id, m_room, m_toward, m_limit, m_destination, m_phase)
};
// 障碍物使用受击次数决定销毁，而不是角色生命值。
struct ObstacleState
{
    // 配置标识和剩余受击次数，负一代表不可破坏。
    int32_t m_id = 0, m_hits = -1;
    // 状态动作已运行毫秒。
    Fixed m_time;
    // 死亡位移只初始化一次，并等待制动或落地。
    bool m_deathStarted = false, m_displacementEnd = false;
    MG_REFLECT(ObstacleState, m_id, m_hits, m_time, m_deathStarted, m_displacementEnd)
};
// 掉落物按抛射、待机、收集顺序推进。
enum class GoodsPhase : uint8_t { Spurt, Idle, Collect };
// 掉落物携带自己的抛射状态和拾取者句柄。
struct GoodsState
{
    // 配置标识与阶段。
    int32_t m_id = 0;
    GoodsPhase m_phase = GoodsPhase::Idle;
    // 抛射速度和拾取者。
    FixedVec3 m_velocity;
    Entity m_collector;
    MG_REFLECT(GoodsState, m_id, m_phase, m_velocity, m_collector)
};
// 每个角色的本地死亡掉落参数。
struct DropGoods
{
    // 已解析的掉落实体配置，允许相同配置重复出现。
    std::vector<int32_t> m_goods;
    // 生命和能量药品百分比。
    Fixed m_hpRate, m_epRate;
    MG_REFLECT(DropGoods, m_goods, m_hpRate, m_epRate)
};
namespace room_entities
{
// 单个房间实体的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class RoomEntity
{
public:
    // 实体组回调，须保持 void(World&, Entity) 签名以注册到 WorldHooks。
    static void update(World& world, Entity entity);
    // 构造指向指定世界实例中某个房间实体的视图。
    RoomEntity(World& world, Entity entity);
    // 为三种非角色实体推进状态。
    void update();
    // 按类型处理碰撞，结束接触不产生行为。
    void onCollision(Entity other, CollisionEvent event);
    // 障碍物死亡等待共享位移系统的完成事件。
    bool onDisplacementEvent(DisplacementEvent event);

private:
    // 非角色动画独立采样，不调用角色攻击树完成回调。
    bool _animate();
    // 没有战斗动作时间轴时，按当前位置和配置半径写受击盒。
    void _writeRadiusDamage();

    World& m_world;
    Entity m_entity{};
};
}
}
