#pragma once

namespace mugen
{
namespace sim_legacy
{
// 声明系统更新所操作的仿真世界。
class World;
}  // namespace sim_legacy

// 仿真系统的最小逐帧更新接口。
struct System
{
    // 支持通过基类指针销毁派生系统。
    virtual ~System() = default;
    // 在世界固定 tick 中执行系统逻辑。
    virtual void update(sim_legacy::World&) = 0;
};
}  // namespace mugen
