#include "doctest.h"  // IWYU pragma: keep
#include "BTTestTypes.h"
#include "mugen/core/reflect/SchemaHash.h"

#include <cstring>

using namespace bt_test;

namespace
{
struct Key
{
    uint32_t templateId                = 0;
    uint32_t groupId                   = 0;
    auto operator<=>(const Key&) const = default;
    MG_REFLECT(Key, templateId, groupId)
};
struct Config
{
    std::vector<Param> pool = params();
};
struct Component
{
    Key defKey;
    Instance instance;
    MG_REFLECT(Component, defKey, instance)
};
Def buildDefinition(const Key& key, const Config& config)
{
    return Def({composite(BTNodeType::Parallel, 2), action(key.templateId), composite(BTNodeType::Selector, 2, {0, 1}),
                action(key.groupId), action(2)},
               {{0, 0}}, config.pool);
}
using Cache = BTDefCache<Key, Param, Config>;
}  // namespace

TEST_CASE("BT copied and archived instances continue identically with definitions rebuilt by key")
{
    // Given
    Config config;
    Cache originalCache(config, buildDefinition);
    const Key key{0, 1};
    const auto& def = originalCache.get(key);
    Component original{key, Instance(def)};
    Ctx originalCtx{original.instance};
    Runner originalRunner(def, registry, original.instance);
    REQUIRE(originalRunner.enter(originalCtx));
    originalCtx.results[0] = BTStatus::Success;
    REQUIRE(originalRunner.update(originalCtx));
    REQUIRE(original.instance.nodes[0].running == std::vector<uint16_t>{1});
    auto cloned = original;
    ByteBuffer bytes;
    BinaryWriter writer(bytes);
    writer.write(original);
    writer.finish();
    Component restored;
    BinaryReader reader(bytes);
    REQUIRE(reader.read(restored));
    ByteBuffer roundtrip;
    BinaryWriter secondWriter(roundtrip);
    secondWriter.write(restored);
    secondWriter.finish();
    REQUIRE(bytes.len() == roundtrip.len());
    REQUIRE(std::memcmp(bytes.data(), roundtrip.data(), bytes.len()) == 0);
    Cache restoredCache(config, buildDefinition);
    const auto& rebuilt = restoredCache.get(restored.defKey);
    Ctx cloneCtx{cloned.instance};
    Ctx restoredCtx{restored.instance};
    Runner cloneRunner(def, registry, cloned.instance);
    Runner restoredRunner(rebuilt, registry, restored.instance);
    originalCtx.log.clear();
    originalCtx.results.fill(BTStatus::Success);
    cloneCtx.results    = originalCtx.results;
    restoredCtx.results = originalCtx.results;
    // When
    for (int tick = 0; tick < 3; ++tick)
    {
        REQUIRE(originalRunner.update(originalCtx));
        REQUIRE(cloneRunner.update(cloneCtx));
        REQUIRE(restoredRunner.update(restoredCtx));
    }
    // Then
    CHECK(originalCtx.log == Log{"C0.check", "A1.update:2", "A1.exit", "A2.enter", "C0.check", "A2.update:1", "A2.exit",
                                 "A1.enter", "C0.check", "A1.update:1", "A1.exit", "A2.enter"});
    CHECK(cloneCtx.log == originalCtx.log);
    CHECK(restoredCtx.log == originalCtx.log);
    CHECK(hashCanonical(cloned) == hashCanonical(original));
    CHECK(hashCanonical(restored) == hashCanonical(original));
    CHECK(&originalCache.get(key) == &def);
    CHECK(&restoredCache.get(key) == &rebuilt);
}

TEST_CASE("BT shared definition instances keep independent leaf and cursor state")
{
    // Given
    const Def def({composite(BTNodeType::Selector, 2), action(0), action(1)}, {}, params());
    Instance first(def);
    Instance second(def);
    Ctx firstCtx{first};
    Ctx secondCtx{second};
    Runner firstRunner(def, registry, first);
    Runner secondRunner(def, registry, second);
    REQUIRE(firstRunner.enter(firstCtx));
    REQUIRE(secondRunner.enter(secondCtx));
    firstCtx.results[0] = BTStatus::Success;
    // When
    REQUIRE(firstRunner.update(firstCtx));
    // Then
    CHECK(first.nodes[0].currentIndex == 1);
    CHECK(second.nodes[0].currentIndex == 0);
    CHECK(std::get<Counter>(second.leaves[0]).ticks == 0);
    CHECK(std::holds_alternative<std::monostate>(second.leaves[1]));
}

TEST_CASE("BT ready snapshot preserves monostate alternatives and their schema identity")
{
    // Given
    const Def def({composite(BTNodeType::Selector, 2), action(0), action(1)}, {}, params());
    Instance original(def);
    ByteBuffer bytes;
    BinaryWriter writer(bytes);
    writer.write(original);
    writer.finish();
    Instance restored;
    BinaryReader reader(bytes);
    // When
    const bool read = reader.read(restored);
    // Then
    REQUIRE(read);
    REQUIRE(restored.leaves.size() == 2);
    CHECK(std::holds_alternative<std::monostate>(restored.leaves[0]));
    CHECK(std::holds_alternative<std::monostate>(restored.leaves[1]));
    CHECK(hashCanonical(original) == hashCanonical(restored));
    CHECK(reflect::schemaHash<std::monostate>() != reflect::schemaHash<Counter>());
}

TEST_CASE("BT definition rejects malformed DFS and pool references before execution")
{
    // Given
    const auto pool = params();
    // When / Then
    CHECK_THROWS_AS(Def({}, {}, pool), std::invalid_argument);
    CHECK_THROWS_AS(Def({action(0), action(1)}, {}, pool), std::invalid_argument);
    CHECK_THROWS_AS(Def({composite(BTNodeType::Selector, 2), action(0)}, {}, pool), std::invalid_argument);
    CHECK_THROWS_AS(Def({composite(BTNodeType::Sequence, 0)}, {}, pool), std::invalid_argument);
    CHECK_THROWS_AS(Def({action(8)}, {}, pool), std::invalid_argument);
    CHECK_THROWS_AS(Def({composite(BTNodeType::Action, 1), action(0)}, {}, pool), std::invalid_argument);
    CHECK_THROWS_AS(Def({composite(BTNodeType::Parallel, 0, {0, 1})}, {{0, 8}}, pool), std::invalid_argument);
}

TEST_CASE("BT empty Selector and Parallel reject entry as in the reference enter path")
{
    for (auto type : {BTNodeType::Selector, BTNodeType::Parallel})
    {
        // Given
        Fixture f({composite(type, 0)});
        StderrCapture capture;
        // When
        const bool entered = f.runner.enter(f.ctx);
        // Then
        CHECK_FALSE(entered);
        CHECK(f.instance.nodes[0].status == BTStatus::Failure);
        CHECK(f.ctx.log.empty());
    }
}

TEST_CASE("BT Parallel rejects entry when all child guards reject")
{
    // Given
    Fixture f({composite(BTNodeType::Parallel, 1), composite(BTNodeType::Selector, 1, {0, 1}), action(0)}, {{0, 0}});
    f.ctx.gates[0] = false;
    StderrCapture capture;
    // When
    const bool entered = f.runner.enter(f.ctx);
    // Then
    CHECK_FALSE(entered);
    CHECK(f.instance.nodes[0].status == BTStatus::Failure);
    CHECK(f.ctx.log == Log{"C0.check"});
}
