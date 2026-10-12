#pragma once

namespace mugen
{
namespace sim
{
// 声明系统更新所操作的仿真世界。
class World;
}

// 仿真系统的最小逐帧更新接口。
struct System
{
    // 支持通过基类指针销毁派生系统。
    virtual ~System()                = default;
    // 在世界固定 tick 中执行系统逻辑。
    virtual void update(sim::World&) = 0;
};
}  // namespace mugen
