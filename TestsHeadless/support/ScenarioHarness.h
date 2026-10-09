#pragma once
#include "mugen/sim/World.h"

namespace mugen::test
{
class ScenarioHarness
{
public:
    explicit ScenarioHarness(sim::WorldSetup setup = {}, sim::WorldHooks hooks = {});
    static void loadAssets();
    static sim::WorldSetup roomSetup(int32_t roomId, uint64_t seed = 1);
    static sim::WorldHooks roomHooks();
    static sim::World roomWorld(int32_t roomId, uint64_t seed = 1);
    sim::World& world() { return m_world; }
    const sim::World& world() const { return m_world; }
    bool inject(uint64_t frame, uint32_t slot, sim::PlayerCommand command);
    bool injectSystem(uint64_t frame, sim::SystemCommand command);
    void tick();
    void runTo(uint64_t frame);
    size_t entityCount();
    bool expectEntityCount(size_t expected);
    bool expectHash(uint64_t expected) const;
    template <class Component, class Value>
    bool expectField(Entity entity, Value Component::* field, const Value& expected) const
    {
        return m_world.ecs().get<Component>(entity).*field == expected;
    }

private:
    sim::World m_world;
    std::map<uint64_t, sim::FrameInput> m_inputs;
};
}  // namespace mugen::test
