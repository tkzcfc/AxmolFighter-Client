#include <doctest.h>
#include "mugen/sim/ai/DecisionTree.h"
#include "mugen/core/serialize/BinaryArchive.h"
#include <array>
#include <initializer_list>

namespace
{
using namespace mugen::sim::ai;
struct Probe
{
    std::array<Result, 13> results;
    std::vector<Method> calls;
    Probe() { results.fill(Result::Success); }
    void set(Method method, Result result) { results[static_cast<size_t>(method)] = result; }
    static Result invoke(void* context, Method method)
    {
        auto& probe = *static_cast<Probe*>(context);
        probe.calls.push_back(method);
        return probe.results[static_cast<size_t>(method)];
    }
    Result tick(DecisionState& state)
    {
        calls.clear();
        return tickEntityTree(state, this, invoke);
    }
    void expect(std::initializer_list<Method> methods) const
    {
        CHECK(calls == std::vector<Method>(methods));
    }
};
}

TEST_CASE("Ai decision tree patrol resumes cached leaf and does not recheck conditions")
{
    DecisionState state;
    Probe probe;
    probe.set(Method::CheckHit, Result::Failure);
    probe.set(Method::CheckTarget, Result::Failure);
    probe.set(Method::CheckMoveEnd, Result::Running);
    CHECK(probe.tick(state) == Result::Running);
    probe.expect({Method::CheckHit, Method::CheckTarget, Method::ExecutePatrol, Method::CheckMoveEnd});
    REQUIRE(!state.m_stack.empty());
    CHECK(state.m_stack.back().m_node == 25);

    probe.set(Method::CheckHit, Result::Success);
    CHECK(probe.tick(state) == Result::Running);
    probe.expect({Method::CheckMoveEnd});
    probe.set(Method::CheckMoveEnd, Result::Failure);
    CHECK(probe.tick(state) == Result::Failure);
    probe.expect({Method::CheckMoveEnd});
    CHECK(state.m_stack.empty());
    CHECK(state.m_alert == AlertStatus::NoAlert);
}

TEST_CASE("Ai decision tree patrol completion dispatches Z then waits for attack")
{
    DecisionState state;
    Probe probe;
    probe.set(Method::CheckHit, Result::Failure);
    probe.set(Method::CheckTarget, Result::Failure);
    probe.set(Method::ExecuteRandAttack, Result::Running);
    CHECK(probe.tick(state) == Result::Running);
    probe.expect({Method::CheckHit, Method::CheckTarget, Method::ExecutePatrol,
                  Method::CheckMoveEnd, Method::ExecuteRandAttack});
    probe.set(Method::ExecuteRandAttack, Result::Success);
    CHECK(probe.tick(state) == Result::Success);
    probe.expect({Method::ExecuteRandAttack, Method::CheckAttackEnd});
    CHECK(state.m_alert == AlertStatus::NoAlert);
}

TEST_CASE("Ai decision tree hit and target branches assign Alert only after X completes")
{
    DecisionState state;
    Probe probe;
    SUBCASE("hit path")
    {
        probe.set(Method::CheckHitEnd, Result::Running);
        CHECK(probe.tick(state) == Result::Running);
        probe.expect({Method::CheckHit, Method::CheckHitEnd});
        probe.set(Method::CheckHitEnd, Result::Success);
        probe.set(Method::CheckAttackEnd, Result::Running);
        CHECK(probe.tick(state) == Result::Running);
        probe.expect({Method::CheckHitEnd, Method::ExecuteFirstAttack, Method::CheckAttackEnd});
    }
    SUBCASE("target path")
    {
        probe.set(Method::CheckHit, Result::Failure);
        probe.set(Method::CheckAttackEnd, Result::Running);
        CHECK(probe.tick(state) == Result::Running);
        probe.expect({Method::CheckHit, Method::CheckTarget, Method::ExecuteFirstAttack, Method::CheckAttackEnd});
    }
    CHECK(state.m_alert == AlertStatus::NoAlert);
    probe.set(Method::CheckAttackEnd, Result::Success);
    CHECK(probe.tick(state) == Result::Success);
    probe.expect({Method::CheckAttackEnd});
    CHECK(state.m_alert == AlertStatus::Alert);
}

TEST_CASE("Ai decision tree custom interruption runs at dispatch boundaries not every tick")
{
    DecisionState state;
    state.m_alert = AlertStatus::Alert;
    Probe probe;
    probe.set(Method::CheckAttackEnd, Result::Running);
    CHECK(probe.tick(state) == Result::Running);
    probe.expect({Method::CheckAttack, Method::ExecuteAttack, Method::CheckAttackEnd});
    probe.set(Method::CheckAttack, Result::Failure);
    CHECK(probe.tick(state) == Result::Running);
    probe.expect({Method::CheckAttackEnd});
    probe.set(Method::CheckAttackEnd, Result::Success);
    CHECK(probe.tick(state) == Result::Failure);
    probe.expect({Method::CheckAttackEnd, Method::CheckAttack});
    CHECK(state.m_alert == AlertStatus::Alert);
    CHECK(probe.tick(state) == Result::Failure);
    probe.expect({Method::CheckAttack});
    CHECK(state.m_stack.empty());
}

TEST_CASE("Ai decision tree selects alert or chase and preserves selected IfElse branch")
{
    DecisionState state;
    state.m_alert = AlertStatus::Alert;
    Probe probe;
    Method selected = Method::ExecuteChase;
    SUBCASE("outside chase scope") { probe.set(Method::CheckChase, Result::Success); }
    SUBCASE("inside chase scope")
    {
        selected = Method::ExecuteAlert;
        probe.set(Method::CheckChase, Result::Failure);
    }
    probe.set(selected, Result::Running);
    CHECK(probe.tick(state) == Result::Running);
    probe.expect({Method::CheckAttack, Method::ExecuteAttack, Method::CheckAttackEnd,
                  Method::CheckAttack, Method::CheckChase, selected});
    state.m_alert = AlertStatus::NoAlert;
    probe.set(selected, Result::Success);
    CHECK(probe.tick(state) == Result::Success);
    probe.expect({selected});
    CHECK(state.m_stack.empty());
}

TEST_CASE("Ai decision tree running stack survives archive and deterministic resumption")
{
    DecisionState original;
    Probe probe;
    probe.set(Method::CheckHitEnd, Result::Running);
    REQUIRE(probe.tick(original) == Result::Running);
    mugen::ByteBuffer bytes;
    mugen::BinaryWriter writer(bytes);
    REQUIRE(writer.write(original));
    bytes.writeFinish();
    mugen::BinaryReader reader(bytes);
    DecisionState restored;
    REQUIRE(reader.read(restored));
    probe.set(Method::CheckHitEnd, Result::Success);
    CHECK(probe.tick(original) == Result::Success);
    const auto calls = probe.calls;
    CHECK(probe.tick(restored) == Result::Success);
    CHECK(probe.calls == calls);
    CHECK(restored.m_alert == original.m_alert);
    CHECK(restored.m_stack.empty());
}
