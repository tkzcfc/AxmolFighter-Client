#pragma once

namespace mugen
{
namespace sim
{
class World;
}

struct System
{
    virtual ~System()                = default;
    virtual void update(sim::World&) = 0;
};
}  // namespace mugen
