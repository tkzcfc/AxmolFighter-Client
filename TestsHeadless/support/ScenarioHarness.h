#pragma once
#include "mugen/sim_legacy/World.h"

namespace mugen::test
{
class ScenarioHarness
{
public:
    explicit ScenarioHarness(sim_legacy::WorldSetup setup = {}, sim_legacy::WorldHooks hooks = {});
    static void loadAssets();
    static sim_legacy::WorldSetup roomSetup(int32_t roomId, uint64_t seed = 1);
    static sim_legacy::WorldHooks roomHooks();
    static sim_legacy::World roomWorld(int32_t roomId, uint64_t seed = 1);
    sim_legacy::World& world() { return m_world; }
    const sim_legacy::World& world() const { return m_world; }
    bool inject(uint64_t frame, uint32_t slot, sim_legacy::PlayerCommand command);
    bool injectSystem(uint64_t frame, sim_legacy::SystemCommand command);
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
    sim_legacy::World m_world;
    std::map<uint64_t, sim_legacy::FrameInput> m_inputs;
};
}  // namespace mugen::test
