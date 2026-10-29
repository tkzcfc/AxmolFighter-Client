#include "doctest.h"
#include "../support/ContentVfs.h"

#include "mugen/conf/ReflectedConfig.h"

TEST_CASE("SkillTree config loads tree 4 and leaf 40010")
{
    REQUIRE(mugen::test::ensureContentVfs());
    mugen::conf::Config config;
    REQUIRE(config.load("/mugen/config/config.bin"));

    const auto* tree = config.find<mugen::conf::SkillTreeConfig>(4);
    REQUIRE(tree != nullptr);
    CHECK(tree->professionType == 4);
    CHECK(tree->leafIds.size() >= 10);

    const auto* leaf = config.find<mugen::conf::SkillLeafConfig>(40010);
    REQUIRE(leaf != nullptr);
    CHECK(leaf->skillId == 980000);
}
