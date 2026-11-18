#include "ui/battle/BattleDebugPanel.h"

#include "mugen/conf/TypeConversions.h"
#include "imgui.h"

namespace gameui
{
void BattleDebugPanel::render(mugen::sim::World& world, bool& paused, bool& singleStep, bool& collisionDebug)
{
    ImGui::Begin("Battle Debug");
    const auto frame = mugen::sim::view::debugFrame(world);
    ImGui::Text("world hash %llu %016llx", static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mugen::sim::view::debugHash(world)));
    ImGui::Text("Frame %llu  %.2fs", static_cast<unsigned long long>(frame),
                static_cast<double>(frame) / 30.0);
    ImGui::Checkbox("Pause", &paused);
    if (ImGui::Button("Single step"))
        singleStep = true;
    ImGui::Checkbox("Collision boxes (F8)", &collisionDebug);
    ImGui::Separator();
    const mugen::Entity player = mugen::sim::view::playerEntity(world, 0);
    if (player.value == 0)
        ImGui::TextUnformatted(mugen::sim::view::kNoProfessionTree.data());
    else
    {
        const auto info       = mugen::sim::view::readEntityRoleInfo(world, player);
        const auto occupation = mugen::type_conversions::toCharacterClass(info.roleId);
        const auto rows       = mugen::sim::view::offlineKitRows(occupation);
        if (rows.empty())
            ImGui::TextUnformatted(mugen::sim::view::kNoProfessionTree.data());
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
                        mugen::sim::view::debugCastSlot(world, player, row.poolSlot);
                    ImGui::PopID();
                }
                ++index;
            }
        }
    }
    ImGui::Separator();
    std::vector<mugen::sim::view::RoleDebugView> roles;
    mugen::sim::view::collectRoleDebug(world, roles);
    for (const auto& role : roles)
    {
        ImGui::Text("entity=%u role=%d level=%d status=%d pos=(%.2f, %.2f, %.2f)", role.entity.value, role.roleId,
                    role.level, role.state, role.position.x.toFloatForRender(), role.position.y.toFloatForRender(),
                    role.position.z.toFloatForRender());
    }
    ImGui::End();
}

void BattleDebugPanel::renderNet(mugen::sim::World& world, const BattleNetInfo& net)
{
    ImGui::Begin("Battle Debug");
    const auto frame = mugen::sim::view::debugFrame(world);
    ImGui::Text("world hash %llu %016llx", static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(mugen::sim::view::debugHash(world)));
    ImGui::Text("Frame %llu  %.2fs", static_cast<unsigned long long>(frame),
                static_cast<double>(frame) / 30.0);
    renderNetLatency(net);
    ImGui::Separator();
    std::vector<mugen::sim::view::RoleDebugView> roles;
    mugen::sim::view::collectRoleDebug(world, roles);
    for (const auto& role : roles)
    {
        ImGui::Text("entity=%u role=%d level=%d status=%d pos=(%.2f, %.2f, %.2f)", role.entity.value, role.roleId,
                    role.level, role.state, role.position.x.toFloatForRender(), role.position.y.toFloatForRender(),
                    role.position.z.toFloatForRender());
    }
    ImGui::End();
}

void BattleDebugPanel::renderNetLatency(const BattleNetInfo& net)
{
    // 绿 / 黄 / 红：<100ms 流畅，<200ms 可接受，之后输入领先量会明显拉长。
    ImVec4 color(0.4f, 1.0f, 0.4f, 1.0f);
    if (net.m_rttMs >= 200)
        color = ImVec4(1.0f, 0.4f, 0.3f, 1.0f);
    else if (net.m_rttMs >= 100)
        color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
    ImGui::TextColored(color, "延迟(RTT): %lld ms  抖动: %lld ms", static_cast<long long>(net.m_rttMs),
                       static_cast<long long>(net.m_jitterMs));
    ImGui::Text("输入领先: %u 帧 (~%u ms)", net.m_leadFrames, net.m_leadFrames * net.m_frameMs);
    ImGui::Text("本地输入延迟: %u 帧 (~%u ms) [%s]", net.m_localDelayFrames, net.m_localDelayFrames * net.m_frameMs,
                net.m_localDelayMode);
}
}  // namespace gameui
