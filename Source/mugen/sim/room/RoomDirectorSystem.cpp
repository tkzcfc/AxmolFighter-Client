// [10.1] RoomDirectorSystem
// 读：`DeathEvent`、`RoomRequest`、`RoomState`、`Identity`、`Health`
// 写：`RoomState`、`SlowMotion`(KO 慢放)、复活
// 消费：`RoomRequest`
// 产生：`RoomEvent`、`SpawnRequest`(波次)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 RoomDirectorSystem：10.1 行，见 DESIGN.md §3.2。首波 Boss 清空后触发 KO 慢放。
// 旧实现的房间导演先于碰撞更新，因此 Boss 死亡要到下一帧才被观察到；这里用 m_pendingKo 复现该一帧延迟。
void runRoomDirector(World& world)
{
    auto& room = world.ecs().singleton<RoomState>();
    if (!room.m_enabled || room.m_paused)
        return;
    if (room.m_pendingKo)
    {
        room.m_pendingKo = false;
        room.m_ko        = true;
        world.ecs().singleton<SlowMotion>().m_ko.start();
        return;
    }
    if (!room.m_ko && room.m_haveBoss && room.m_roomType != 0 && room.m_battleRuleType < 2)
    {
        bool bossAlive = false;
        world.ecs().each<Identity, Health>([&](Entity, const Identity& identity, const Health& health) {
            if (identity.m_roleType == 4 && health.m_hp > Fixed{})
                bossAlive = true;
        });
        if (!bossAlive)
            room.m_pendingKo = true;
    }
}
}  // namespace mugen::sim
