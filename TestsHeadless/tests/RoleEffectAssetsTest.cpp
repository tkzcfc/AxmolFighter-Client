#include "SkillTestSupport.h"
#include "mugen/sim/spawn/RoleEffectAssets.h"

#include <algorithm>
#include <variant>

using namespace skill_test;

TEST_CASE("RoleEffectAssets collects mage attack effect spines")
{
    loadConfig();
    const auto ids = collectRoleEffectSpineIds(104);
    REQUIRE_FALSE(ids.empty());
    for (const int32_t id : ids)
        CHECK(conf::Config::instance().find<conf::ResSpineConfig>(id) != nullptr);

    bool found = false;
    int32_t skillId = 980000;
    std::vector<int32_t> visited;
    while (skillId > 0 && std::find(visited.begin(), visited.end(), skillId) == visited.end())
    {
        visited.push_back(skillId);
        const auto* skill = conf::Config::instance().find<conf::SkillAttackConfig>(skillId);
        if (!skill)
            break;
        for (const auto& group : skill->actionIds)
            for (const int32_t actionId : group)
            {
                const auto* action = conf::Config::instance().find<conf::ActionConfig>(actionId);
                if (!action)
                    continue;
                for (const auto& effectRef : action->effectIds)
                {
                    const auto* effectId = std::get_if<int32_t>(&effectRef);
                    if (!effectId || *effectId <= 0)
                        continue;
                    const auto* effect = conf::Config::instance().find<conf::EffectConfig>(*effectId);
                    if (!effect || effect->resSpineId <= 0)
                        continue;
                    CHECK(std::find(ids.begin(), ids.end(), effect->resSpineId) != ids.end());
                    found = true;
                }
            }
        skillId = skill->nextSkill;
    }
    CHECK(found);
}

TEST_CASE("RoleEffectAssets ignores unknown roles")
{
    loadConfig();
    CHECK(collectRoleEffectSpineIds(0).empty());
    CHECK(collectRoleEffectSpineIds(-1).empty());
}
