#include "SkillTestSupport.h"
#include <algorithm>

using namespace skill_test;

TEST_CASE("OfflineKitDump mage dump contains leaf 40010 skill 980000 key U")
{
    loadConfig();
    const auto rows = skill::SkillAssembly::offlineKitRows(CharacterClass::kMage);
    REQUIRE(!rows.empty());
    const auto it =
        std::find_if(rows.begin(), rows.end(), [](const auto& row) { return row.leafId == 40010; });
    REQUIRE(it != rows.end());
    CHECK(it->skillId == 980000);
    CHECK(it->key == "U");
    CHECK(it->slotName == "Attack");
    CHECK(it->slot == EnumSkillSlot::Attack);
    CHECK(it->poolSlot == "A");
    CHECK(it->hasControlKey);
}

TEST_CASE("OfflineKitDump fighter dump is no profession tree")
{
    loadConfig();
    const auto rows = skill::SkillAssembly::offlineKitRows(CharacterClass::kFighter);
    CHECK(rows.empty());
    CHECK(skill::kNoProfessionTree == "no profession tree");
    const auto it =
        std::find_if(rows.begin(), rows.end(), [](const auto& row) { return row.skillId == 82010; });
    CHECK(it == rows.end());
}
