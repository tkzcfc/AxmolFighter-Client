#include "ui/battle/BattleDebugPanel.h"

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
