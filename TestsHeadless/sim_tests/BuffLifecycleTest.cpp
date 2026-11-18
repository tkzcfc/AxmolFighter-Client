#include "BuffTestSupport.h"

using namespace buff_test;
using namespace mugen;
using namespace mugen::sim;
using namespace mugen::conf::battle;

TEST_CASE("buff lifecycle admit add and remove emit events")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.persistent.push_back(flagOp(BuffFlag::SuperArmor));
    const int32_t id = h.add(def);
    h.admit(id);
    REQUIRE(findBuff(h.world, h.self, id) != nullptr);
    CHECK(hasFlag(h.world, h.self, ExtraState::SuperArmor));
    CHECK(eventCount(h.world, BuffPhase::Added, id) == 1);
    h.remove(id);
    CHECK(findBuff(h.world, h.self, id) == nullptr);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::SuperArmor));
    CHECK(eventCount(h.world, BuffPhase::Removed, id) == 1);
}

TEST_CASE("buff lifecycle same-frame add then remove runs Enter and Exit")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.persistent.push_back(flagOp(BuffFlag::Invincible));
    const int32_t id = h.add(def);
    h.admit(id);
    h.remove(id);
    CHECK(findBuff(h.world, h.self, id) == nullptr);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Invincible));
    CHECK(eventCount(h.world, BuffPhase::Added, id) == 1);
    CHECK(eventCount(h.world, BuffPhase::Removed, id) == 1);
}

TEST_CASE("buff lifecycle remove then admit same id is a new instance")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.admission.resetType = 4;
    const int32_t id = h.add(def);
    h.admit(id);
    const uint32_t first = findBuff(h.world, h.self, id)->serial;
    h.remove(id);
    h.admit(id);
    REQUIRE(findBuff(h.world, h.self, id) != nullptr);
    CHECK(findBuff(h.world, h.self, id)->serial != first);
    CHECK(findBuff(h.world, h.self, id)->stacks == 1);
}

TEST_CASE("buff lifecycle duration times=0 interval expires after ticks")
{
    Harness h;
    h.setTickSeconds(1);
    BuffDef def = makeBuff(nextBuffId());
    def.lifecycle.times       = 0;
    def.lifecycle.intervalSec = Fixed::fromInt(1);
    def.persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t id = h.add(def);
    h.admit(id);
    REQUIRE(findBuff(h.world, h.self, id) != nullptr);
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, id) != nullptr);
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, id) == nullptr);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Stealth));
}

TEST_CASE("buff lifecycle times=-1 interval=0 ticks every frame")
{
    Harness h;
    h.setTickSeconds(1);
    const Fixed hp = h.world.ecs().get<HealthComponent>(h.self).hp;
    BuffDef def    = makeBuff(nextBuffId());
    def.lifecycle.times = -1;
    def.onTick.push_back(resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-3))));
    const int32_t id = h.add(def);
    h.admit(id);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp);
    h.tickBuff();
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp - Fixed::fromInt(3));
    h.tickBuff();
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp - Fixed::fromInt(6));
}

TEST_CASE("buff lifecycle times>0 interval drives Enter then drop")
{
    Harness h;
    h.setTickSeconds(1);
    BuffDef def = makeBuff(nextBuffId());
    def.lifecycle.times       = 1;
    def.lifecycle.intervalSec = Fixed::fromInt(1);
    def.persistent.push_back(flagOp(BuffFlag::Silence));
    const int32_t id = h.add(def);
    h.admit(id);
    REQUIRE(findBuff(h.world, h.self, id) != nullptr);
    CHECK(findBuff(h.world, h.self, id)->times == 1);
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, id) == nullptr);
}

TEST_CASE("buff lifecycle executeType 2 waits for interval")
{
    Harness h;
    h.setTickSeconds(1);
    BuffDef def = makeBuff(nextBuffId());
    def.lifecycle.executeType = 2;
    def.lifecycle.times       = -1;
    def.lifecycle.intervalSec = Fixed::fromInt(1);
    def.onTick.push_back(resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-8))));
    const int32_t id = h.add(def);
    const Fixed hp   = h.world.ecs().get<HealthComponent>(h.self).hp;
    h.admit(id);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp);
    h.tickBuff();
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp);
    h.tickBuff();
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp - Fixed::fromInt(8));
}

TEST_CASE("buff lifecycle executeType 3 waits for trigger")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.lifecycle.executeType = 3;
    def.activation.beginOn    = TriggerPoint::AfterHit;
    def.activation.endOn      = TriggerPoint::AfterToBeHit;
    def.activation.latchOn    = static_cast<int32_t>(BuffHook::Begin);
    def.activation.releaseOn  = static_cast<int32_t>(BuffHook::End);
    def.persistent.push_back(flagOp(BuffFlag::Invincible));
    const int32_t id = h.add(def);
    h.admit(id);
    REQUIRE(findBuff(h.world, h.self, id) != nullptr);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Invincible));
    h.fire(TriggerPoint::AfterHit);
    CHECK(hasFlag(h.world, h.self, ExtraState::Invincible));
    CHECK(findBuff(h.world, h.self, id)->latched);
    h.fire(TriggerPoint::AfterToBeHit);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Invincible));
    CHECK_FALSE(findBuff(h.world, h.self, id)->latched);
}

TEST_CASE("buff resetType 0..7")
{
    Harness h;
    for (int32_t type = 0; type < 8; ++type)
    {
        CAPTURE(type);
        BuffDef def = makeBuff(nextBuffId());
        def.lifecycle.times       = 5;
        def.lifecycle.intervalSec = Fixed::fromInt(3);
        def.admission.resetType   = type;
        def.admission.maxStacks   = 9;
        const int32_t id = h.add(def);
        h.admit(id);
        auto* inst = findBuff(h.world, h.self, id);
        REQUIRE(inst != nullptr);
        inst->times    = 3;
        inst->interval = Fixed::fromInt(2);
        h.admit(id);
        inst = findBuff(h.world, h.self, id);
        REQUIRE(inst != nullptr);
        const bool resetTimes    = type == 1 || type == 2 || type == 6 || type == 7;
        const bool resetInterval = type == 1 || type == 3 || type == 5 || type == 7;
        const bool stack         = type >= 4;
        CHECK(inst->times == (resetTimes ? 0 : 3));
        CHECK(inst->interval == (resetInterval ? Fixed{} : Fixed::fromInt(2)));
        CHECK(inst->stacks == (stack ? 2 : 1));
    }
}

TEST_CASE("buff latch does not reapply Flag on stacking")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.admission.resetType = 4;
    def.persistent.push_back(flagOp(BuffFlag::SuperArmor));
    const int32_t id = h.add(def);
    h.admit(id);
    h.admit(id);
    CHECK(findBuff(h.world, h.self, id)->stacks == 2);
    CHECK(h.world.ecs().get<StatusFlagsComponent>(h.self).refs[static_cast<size_t>(ExtraState::SuperArmor) - 1] == 1);
    h.remove(id);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::SuperArmor));
}

TEST_CASE("buff R7 new instance skips the in-flight AfterHit dispatch")
{
    Harness h;
    BuffDef child = makeBuff(nextBuffId());
    child.lifecycle.executeType = 3;
    child.activation.beginOn    = TriggerPoint::AfterHit;
    child.onBegin.push_back(resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-100))));
    const int32_t childId = h.add(child);
    BuffDef parent = makeBuff(nextBuffId());
    parent.lifecycle.executeType = 3;
    parent.activation.beginOn    = TriggerPoint::AfterHit;
    BuffOp apply;
    apply.kind = BuffOpKind::Apply;
    apply.ids  = {childId};
    apply.to   = ApplyTo::Self;
    parent.onBegin.push_back(apply);
    h.admit(h.add(parent));
    const Fixed hp = h.world.ecs().get<HealthComponent>(h.self).hp;
    h.fire(TriggerPoint::AfterHit);
    REQUIRE(findBuff(h.world, h.self, childId) != nullptr);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp);
    h.fire(TriggerPoint::AfterHit);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp - Fixed::fromInt(100));
}

TEST_CASE("buff R16 trigger chain depth is capped at 8")
{
    Harness h;
    const int32_t id = nextBuffId();
    BuffDef def      = makeBuff(id);
    def.admission.subType  = -1;
    def.activation.beginOn = TriggerPoint::AddBuff;
    BuffOp apply;
    apply.kind = BuffOpKind::Apply;
    apply.ids  = {id};
    apply.to   = ApplyTo::Self;
    def.onBegin.push_back(apply);
    h.add(def);
    h.admit(id);
    const auto count = static_cast<int32_t>(h.world.ecs().get<BuffListComponent>(h.self).instances.size());
    CHECK(count == 9);
    CHECK(count <= 9);
}

TEST_CASE("buff add CD rejects a second admit")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.admission.cd = Fixed::fromInt(5);
    def.persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t id = h.add(def);
    h.admit(id);
    const uint32_t serial = findBuff(h.world, h.self, id)->serial;
    h.admit(id);
    CHECK(findBuff(h.world, h.self, id)->serial == serial);
    CHECK(h.world.ecs().get<BuffListComponent>(h.self).instances.size() == 1);
}

TEST_CASE("buff InactiveComponent and DespawningComponent reject admit")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    const int32_t id = h.add(def);
    h.world.ecs().add(h.self, InactiveComponent{});
    h.admit(id);
    CHECK(h.world.ecs().get<BuffListComponent>(h.self).instances.empty());
    h.world.ecs().remove<InactiveComponent>(h.self);
    h.world.ecs().add(h.self, DespawningComponent{});
    h.admit(id);
    CHECK(h.world.ecs().get<BuffListComponent>(h.self).instances.empty());
}

TEST_CASE("buff probability uses DrawPolicy")
{
    Harness h;
    h.policy.probability = Fixed::fromInt(50);
    BuffDef fail = makeBuff(nextBuffId());
    fail.admission.probability = Fixed::fromInt(40);
    fail.admission.subType     = -1;
    const int32_t failId = h.add(fail);
    h.admit(failId);
    CHECK(findBuff(h.world, h.self, failId) == nullptr);
    BuffDef pass = makeBuff(nextBuffId());
    pass.admission.probability = Fixed::fromInt(60);
    pass.admission.subType     = -1;
    const int32_t passId = h.add(pass);
    h.admit(passId);
    CHECK(findBuff(h.world, h.self, passId) != nullptr);
}

TEST_CASE("buff snapshot round trip keeps instances and hash")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.persistent.push_back(flagOp(BuffFlag::SuperArmor));
    def.persistent.push_back(statOp(BuffStat::Atk, constVal(Fixed::fromInt(11))));
    const int32_t id = h.add(def);
    h.admit(id);
    REQUIRE(findBuff(h.world, h.self, id) != nullptr);
    ByteBuffer buffer;
    h.world.serialize(buffer);
    buffer.resetPosition();
    World restored;
    REQUIRE(restored.deserialize(buffer));
    CHECK(restored.hash() == h.world.hash());
    Entity restoredSelf;
    restored.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.roleType == 1)
            restoredSelf = entity;
    });
    REQUIRE(restoredSelf.value != 0);
    CHECK(findBuff(restored, restoredSelf, id) != nullptr);
    CHECK(hasFlag(restored, restoredSelf, ExtraState::SuperArmor));
    CHECK(statSum(restored, restoredSelf, 5) == Fixed::fromInt(11));
    h.tickWorld(4);
    restored.setDrawPolicy(&h.policy);
    for (int i = 0; i < 4; ++i)
    {
        FrameInput input;
        input.frame = restored.frame() + 1;
        restored.tick(input);
    }
    CHECK(restored.hash() == h.world.hash());
}

TEST_CASE("buff determinism two identical runs share hash")
{
    auto run = []() {
        Harness h;
        BuffDef def = makeBuff(99030001);
        def.persistent.push_back(flagOp(BuffFlag::SuperArmor));
        def.onEnter.push_back(resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-4))));
        h.add(def);
        h.admit(99030001);
        h.tickWorld(6);
        const uint64_t hash = h.world.hash();
        return hash;
    };
    CHECK(run() == run());
}
