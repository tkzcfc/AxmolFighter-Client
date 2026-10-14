#include "ui/battle/BattleDebugPanel.h"

#include "mugen/conf/TypeConversions.h"
#include "mugen/sim/skill/SkillCaster.h"
#include "imgui.h"

namespace gameui
{
void BattleDebugPanel::render(mugen::sim::World& world, bool& paused, bool& singleStep, bool& collisionDebug)
{
    ImGui::Begin("Battle Debug");
    const auto& clock = world.ecs().singleton<mugen::sim::WorldClock>();
    ImGui::Text("world hash %llu %016llx", static_cast<unsigned long long>(clock.frame),
                static_cast<unsigned long long>(world.hash()));
    ImGui::Checkbox("Pause", &paused);
    if (ImGui::Button("Single step"))
        singleStep = true;
    ImGui::Checkbox("Collision boxes (F8)", &collisionDebug);
    ImGui::Separator();
    mugen::Entity player{};
    const auto& slots = world.ecs().singleton<mugen::sim::PlayerSlots>().entities;
    if (const auto it = slots.find(0); it != slots.end())
        player = it->second;
    else if (!slots.empty())
        player = slots.begin()->second;
    if (player.value == 0)
        ImGui::TextUnformatted(mugen::sim::skill::kNoProfessionTree.data());
    else
    {
        const auto& identity  = world.ecs().get<mugen::sim::RoleIdentity>(player);
        const auto occupation = mugen::type_conversions::toCharacterClass(identity.roleId);
        const auto rows       = mugen::sim::skill::offlineKitRows(occupation);
        if (rows.empty())
            ImGui::TextUnformatted(mugen::sim::skill::kNoProfessionTree.data());
        else
        {
            ImGui::TextUnformatted("key  slot  leaf  skill  pool");
            int index = 0;
            for (const auto& row : rows)
            {
                ImGui::Text("%s  %s  %d  %d  %s", row.key.empty() ? "-" : row.key.c_str(), row.slotName.c_str(),
                            row.leafId, row.skillId, row.poolSlot.c_str());
                if (!row.hasControlKey && !row.poolSlot.empty())
                {
                    ImGui::SameLine();
                    ImGui::PushID(index);
                    if (ImGui::Button("Cast"))
                        mugen::sim::skill::SkillCaster(world, player).presetSkill(row.poolSlot);
                    ImGui::PopID();
                }
                ++index;
            }
        }
    }
    ImGui::Separator();
    world.ecs().each<mugen::sim::RoleIdentity, mugen::sim::RoleStatus, mugen::sim::Transform>(
        [](mugen::Entity entity, const mugen::sim::RoleIdentity& identity, const mugen::sim::RoleStatus& status,
           const mugen::sim::Transform& transform) {
            ImGui::Text("entity=%u role=%d level=%d status=%d pos=(%.2f, %.2f, %.2f)", entity.value, identity.roleId,
                        identity.level, static_cast<int>(status.eEntityStatus), transform.position.x.toFloatForRender(),
                        transform.position.y.toFloatForRender(), transform.position.z.toFloatForRender());
        });
    ImGui::End();
}
}
