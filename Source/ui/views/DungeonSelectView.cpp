#include "DungeonSelectView.h"

#include "GameView.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/TableConfig.h"
#include "ui/battle/BattleBootParams.h"
#include "ui/core/ViewManager.h"
#include "ui/widgets/common/MessagePopup.h"

#include "imgui.h"

#include <algorithm>

using namespace mugen;

namespace gameui
{

void DungeonSelectView::onEnter()
{
    Super::onEnter();
    rebuildChapterList();
}

void DungeonSelectView::rebuildChapterList()
{
    m_chapters.clear();
    m_selectedChapterIndex = 0;
    m_selectedCopyId       = 0;

    auto* config = Config::getInstance();
    for (const auto& [id, chapter] : config->getChapterConfigs())
    {
        if (chapter.isHide != 0)
            continue;
        if (chapter.mainCopys.empty())
            continue;

        ChapterItem item;
        item.id       = chapter.id;
        item.nameId   = chapter.nameId;
        item.mainCopys = chapter.mainCopys;
        m_chapters.push_back(std::move(item));
    }

    std::sort(m_chapters.begin(), m_chapters.end(),
              [](const ChapterItem& a, const ChapterItem& b) { return a.id < b.id; });

    if (!m_chapters.empty() && !m_chapters.front().mainCopys.empty())
        m_selectedCopyId = m_chapters.front().mainCopys.front();
}

void DungeonSelectView::onUpdate(float /*delta*/)
{
    if (m_requestClose)
    {
        m_requestClose = false;
        getViewManager()->popView();
    }
}

void DungeonSelectView::requestClose()
{
    m_requestClose = true;
}

void DungeonSelectView::enterCopy(int32_t copyId)
{
    auto* config = Config::getInstance();
    const auto* copy = config->getCopyConfigById(copyId);
    if (!copy)
    {
        MessagePopup::show(fmt::format("副本配置不存在: {}", copyId));
        return;
    }

    const auto* stage = config->getStageConfigById(copy->stageId);
    if (!stage)
    {
        MessagePopup::show(fmt::format("关卡配置不存在: stageId={}", copy->stageId));
        return;
    }

    if (stage->roomId <= 0)
    {
        MessagePopup::show(fmt::format("关卡未配置房间: stageId={}", copy->stageId));
        return;
    }

    const auto* room = config->getRoomConfigById(stage->roomId);
    if (!room)
    {
        MessagePopup::show(fmt::format("房间配置不存在: roomId={}", stage->roomId));
        return;
    }

    AXLOGI("DungeonSelectView: enter copy={} stage={} room={} mapKey={}", copyId, copy->stageId, stage->roomId,
           room->mapKey);

    LocalBattleParams params;
    params.roomId = stage->roomId;
    getViewManager()->switchView<GameView>(params);
}

void DungeonSelectView::onImGUIRender()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(720.0f, 420.0f), ImGuiCond_FirstUseEver);

    ImGui::Begin("副本选择", nullptr, ImGuiWindowFlags_NoCollapse);

    if (m_chapters.empty())
    {
        ImGui::TextUnformatted("没有可用章节");
        if (ImGui::Button("返回", ImVec2(120.0f, 0.0f)))
            requestClose();
        ImGui::End();
        return;
    }

    // 左侧章节
    ImGui::BeginChild("chapters", ImVec2(200.0f, -40.0f), true);
    ImGui::TextUnformatted("章节");
    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(m_chapters.size()); ++i)
    {
        const auto& chapter = m_chapters[static_cast<size_t>(i)];
        const std::string label = fmt::format("章节 {} (nameId:{})##ch_{}", chapter.id, chapter.nameId, chapter.id);
        if (ImGui::Selectable(label.c_str(), m_selectedChapterIndex == i))
        {
            m_selectedChapterIndex = i;
            if (!chapter.mainCopys.empty())
                m_selectedCopyId = chapter.mainCopys.front();
            else
                m_selectedCopyId = 0;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // 右侧关卡
    ImGui::BeginChild("copies", ImVec2(0.0f, -40.0f), true);
    ImGui::TextUnformatted("关卡");
    ImGui::Separator();

    if (m_selectedChapterIndex >= 0 && m_selectedChapterIndex < static_cast<int>(m_chapters.size()))
    {
        auto* config              = Config::getInstance();
        const auto& chapter       = m_chapters[static_cast<size_t>(m_selectedChapterIndex)];
        for (int32_t copyId : chapter.mainCopys)
        {
            const auto* copy = config->getCopyConfigById(copyId);
            std::string label;
            if (copy)
            {
                label = fmt::format("关卡 {}  nameId:{}  战力:{}##copy_{}", copyId, copy->nameId,
                                    copy->recommendFighting, copyId);
            }
            else
            {
                label = fmt::format("关卡 {} (缺失配置)##copy_{}", copyId, copyId);
            }

            if (ImGui::Selectable(label.c_str(), m_selectedCopyId == copyId))
                m_selectedCopyId = copyId;

            if (ImGui::IsItemHovered() && copy)
            {
                ImGui::BeginTooltip();
                ImGui::Text("copyId=%d stageId=%d", copyId, copy->stageId);
                if (const auto* stage = config->getStageConfigById(copy->stageId))
                    ImGui::Text("roomId=%d mapKey=%s", stage->roomId, stage->mapKey.c_str());
                ImGui::EndTooltip();
            }
        }
    }
    ImGui::EndChild();

    // 底部按钮
    if (ImGui::Button("进入", ImVec2(120.0f, 0.0f)))
    {
        if (m_selectedCopyId > 0)
            enterCopy(m_selectedCopyId);
        else
            MessagePopup::show("请先选择关卡");
    }
    ImGui::SameLine();
    if (ImGui::Button("返回", ImVec2(120.0f, 0.0f)))
        requestClose();

    ImGui::End();
}

}  // namespace gameui
