#include "doctest.h"

#include "mugen/conf/ReflectedConfig.h"

#include <filesystem>

TEST_CASE("SkillTree config loads tree 4 and leaf 40010")
{
    const auto path =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Content/mugen/config/config.bin";
    mugen::conf::Config config;
    REQUIRE(config.load(path.string()));

    const auto* tree = config.find<mugen::conf::SkillTreeConfig>(4);
    REQUIRE(tree != nullptr);
    CHECK(tree->professionType == 4);
    CHECK(tree->leafIds.size() >= 10);

    const auto* leaf = config.find<mugen::conf::SkillLeafConfig>(40010);
    REQUIRE(leaf != nullptr);
    CHECK(leaf->skillId == 980000);
}
