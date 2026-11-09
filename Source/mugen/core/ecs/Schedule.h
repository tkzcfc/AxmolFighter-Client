#pragma once

#include <cstdint>
#include <span>

namespace mugen
{
// 调度表中的一行：系统名、所属阶段与入口函数指针。
template <class W>
struct SystemEntry
{
    // 系统名，用于日志、追踪与表检查。
    const char* m_name;
    // 所属阶段编号，表内必须非递减。
    uint8_t m_phase;
    // 系统入口，不使用 std::function。
    void (*m_run)(W&);
};

// 逐系统观察钩子：begin 为真表示系统即将运行，否则表示刚运行完。
template <class W>
using ScheduleObserver = void (*)(W&, const SystemEntry<W>&, bool begin);

// 按表顺序运行全部系统；observer 为空时不做任何观察。
template <class W>
void runSchedule(W& world, std::span<const SystemEntry<W>> table, ScheduleObserver<W> observer = nullptr)
{
    for (const SystemEntry<W>& entry : table)
    {
        if (observer != nullptr)
            observer(world, entry, true);
        entry.m_run(world);
        if (observer != nullptr)
            observer(world, entry, false);
    }
}
}  // namespace mugen
