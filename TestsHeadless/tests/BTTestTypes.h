#pragma once

#include "mugen/core/bt/BTDefCache.h"
#include "mugen/core/bt/BTRunner.h"
#include "mugen/core/serialize/Hash.h"

#include <array>
#include <sstream>

namespace bt_test
{
using namespace mugen;

// Synthetic parameters/state only; production variants belong to the simulation layer.
struct Probe
{
    uint32_t slot  = 0;
    uint32_t owner = 0;
    uint32_t bit   = 0;
};
struct Counter
{
    int32_t ticks = 0;
    MG_REFLECT(Counter, ticks)
};
using Param    = std::variant<std::monostate, Probe>;
using State    = std::variant<std::monostate, Counter>;
using Def      = BTDef<Param>;
using Instance = BTInstance<State>;
using Log      = std::vector<std::string>;

struct Ctx
{
    Instance& instance;
    Log log{};
    std::array<bool, 8> gates{true, true, true, true, true, true, true, true};
    std::array<BTStatus, 8> results{BTStatus::Running, BTStatus::Running, BTStatus::Running, BTStatus::Running,
                                    BTStatus::Running, BTStatus::Running, BTStatus::Running, BTStatus::Running};
    uint32_t bits = 0;
    std::vector<std::pair<BTStatus, int16_t>> parentsAtExit{};
    std::vector<BTStatus> conditionsAtExit{};
};

inline void leafEnter(Ctx& ctx, const Param& param, State& state)
{
    const auto& probe = std::get<Probe>(param);
    state             = Counter{};
    ctx.log.push_back("A" + std::to_string(probe.slot) + ".enter");
}
inline BTStatus leafUpdate(Ctx& ctx, const Param& param, State& state)
{
    const auto& probe = std::get<Probe>(param);
    auto& counter     = std::get<Counter>(state);
    ++counter.ticks;
    ctx.log.push_back("A" + std::to_string(probe.slot) + ".update:" + std::to_string(counter.ticks));
    return ctx.results[probe.slot];
}
inline void leafExit(Ctx& ctx, const Param& param, State&)
{
    ctx.log.push_back("A" + std::to_string(std::get<Probe>(param).slot) + ".exit");
}
inline bool condCheck(Ctx& ctx, const Param& param)
{
    const auto& probe = std::get<Probe>(param);
    ctx.log.push_back("C" + std::to_string(probe.slot) + ".check");
    return ctx.gates[probe.slot] && (probe.bit == 0 || (ctx.bits & probe.bit) != 0);
}
inline void condEnter(Ctx& ctx, const Param& param)
{
    ctx.log.push_back("C" + std::to_string(std::get<Probe>(param).slot) + ".enter");
}
inline void condExit(Ctx& ctx, const Param& param)
{
    const auto& probe = std::get<Probe>(param);
    ctx.log.push_back("C" + std::to_string(probe.slot) + ".exit");
    ctx.bits &= ~probe.bit;
    const auto& owner = ctx.instance.nodes[probe.owner];
    ctx.parentsAtExit.emplace_back(owner.status, owner.currentIndex);
    ctx.conditionsAtExit.push_back(ctx.instance.conditions[probe.slot]);
}

using Registry = BTRegistry<Ctx, Param, State>;
using Runner   = BTRunner<Ctx, Param, State>;
inline const Registry registry({{leafEnter, leafUpdate, leafExit}}, {{condCheck, condEnter, condExit}});

inline BTNodeDef action(uint32_t slot)
{
    BTNodeDef node;
    node.paramIndex = slot;
    return node;
}
inline BTNodeDef composite(BTNodeType type, uint16_t children, std::pair<uint32_t, uint16_t> conditions = {})
{
    BTNodeDef node;
    node.type       = type;
    node.childCount = children;
    node.firstCond  = conditions.first;
    node.condCount  = conditions.second;
    return node;
}
inline std::vector<Param> params()
{
    std::vector<Param> result;
    for (uint32_t slot = 0; slot < 8; ++slot)
        result.emplace_back(Probe{slot});
    return result;
}

struct Fixture
{
    Def def;
    Instance instance{def};
    Ctx ctx{instance};
    Runner runner{def, registry, instance};
    Fixture(std::vector<BTNodeDef> nodes, std::vector<BTCondDef> conditions = {}, std::vector<Param> pool = params())
        : def(std::move(nodes), std::move(conditions), std::move(pool))
    {}
};

class StderrCapture
{
public:
    StderrCapture() : m_previous(std::cerr.rdbuf(m_stream.rdbuf())) {}
    ~StderrCapture() { std::cerr.rdbuf(m_previous); }
    std::string text() const { return m_stream.str(); }

private:
    std::ostringstream m_stream;
    std::streambuf* m_previous;
};
}  // namespace bt_test
