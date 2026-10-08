#include "doctest.h"

#include "mugen/conf/ReflectedConfig.h"

#include <filesystem>

TEST_CASE("ConfigLoad finds real skill 920000")
{
    namespace fs = std::filesystem;
    const fs::path configPath =
        fs::path(__FILE__).parent_path().parent_path().parent_path() / "Content" / "mugen" / "config" / "config.bin";
    mugen::conf::Config config;
    REQUIRE(config.load(configPath.string()));
    const auto* skill = config.find<mugen::conf::SkillAttackConfig>(920000);
    REQUIRE(skill != nullptr);
    CHECK(skill->actionIds == std::vector<std::vector<int32_t>>{{920000, 920010, 920020}});
    CHECK(config.find<mugen::conf::SkillAttackConfig>(-123456) == nullptr);
}
