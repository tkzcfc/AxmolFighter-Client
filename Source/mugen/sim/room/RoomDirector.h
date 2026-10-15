#pragma once
#include "mugen/conf/tables/MapTables.h"
#include "mugen/sim/input/FrameInput.h"
#include "mugen/sim/role/RoleComponents.h"
#include <vector>

namespace mugen::sim
{
class World;
namespace room
{
// 房间流程与非角色实体装配的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class RoomDirector
{
public:
    // 构造指向指定世界实例的房间视图。
    explicit RoomDirector(World& world);
    // 初始化房间流程；英雄由调用方先创建。
    bool enter(const conf::RoomConfig& config, int32_t stageId, int32_t copyId);
    // 将尚未请求过的状态加入顺序队列。
    void setExpectRoomState(mugen::RoomState state);
    // 在场景阶段推进房间流程。
    void update();
    // 消费帧输入中的复活和返回请求。
    void commands(const FrameInput& input);
    // 检查计入清场的敌方角色是否已离场。
    bool checkWipeOutEnemys();
    // 装配房间非角色实体。
    void load(const conf::RoomConfig& config);
    // 创建可直接拾取或抛射的掉落物。
    Entity goods(int32_t id, Transform transform, bool spurt);
    // 创建房间障碍物。
    Entity obstacle(int32_t id, Transform transform, int32_t level);
    // 完成清场后开放本地房间门。
    void openPortals();
    // 执行角色死亡掉落。
    void drop(Entity entity);

private:
    // 暂停角色组，不暂停场景状态机。
    void _suspend(bool value);
    // 取得主控玩家实体。
    Entity _hero() const;
    // 按主控阵营的敌对掩码判断目标。
    bool _enemy(const RoleIdentity& identity) const;
    // 读取配置数组中可省略的尾项。
    static int32_t _value(const std::vector<int32_t>& values, size_t index, int32_t fallback);
    // 使用主房间难度及等级生成一组坐标配置。
    void _spawn(const conf::RoomConfig& positions);
    // 进入单个状态，结算仅产生本地事件。
    void _enterState(mugen::RoomState state);

    // 本场战斗世界。
    World& m_world;
};
}  // namespace room
}  // namespace mugen::sim
