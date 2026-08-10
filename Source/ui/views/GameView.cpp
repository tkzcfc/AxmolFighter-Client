#include "GameView.h"

#include "TownView.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "ui/battle/LocalBattleMode.h"
#include "ui/battle/OnlineBattleMode.h"
#include "ui/core/ViewManager.h"
#include "ui/input/DefaultInputSlotMap.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>

using namespace mugen;

namespace gameui
{

GameView::GameView() = default;

GameView::GameView(BattleBootParams boot) : m_boot(std::move(boot)) {}

GameView::GameView(LocalBattleParams local) : m_local(std::move(local)) {}

GameView::~GameView() = default;

void GameView::onEnter()
{
    Super::onEnter();

    fillCombatInputSlotMap(m_slotMap);

    if (!initGameWord())
    {
        MG_LOG_E("GameView: initGameWord failed");
        m_gameWord = nullptr;
        return;
    }

    m_battleMode = createBattleMode();
    if (!m_battleMode)
    {
        MG_LOG_E("GameView: createBattleMode returned null");
        return;
    }

    if (!m_battleMode->init(m_gameWord.get()))
    {
        MG_LOG_E("GameView: battleMode init failed");
        m_battleMode = nullptr;
        return;
    }

    MG_LOG_I("GameView: entered (online={}, localRoom={})", m_boot.has_value(),
             m_local.has_value() ? m_local->roomId : 0);
}

void GameView::onExit()
{
    if (m_keyboardListener)
    {
        ax::Director::getInstance()->getEventDispatcher()->removeEventListener(m_keyboardListener);
        m_keyboardListener = nullptr;
    }

    if (m_battleMode)
    {
        m_battleMode->onExit();
        m_battleMode = nullptr;
    }

    m_gameWord = nullptr;
    Super::onExit();
}

void GameView::onUpdate(float delta)
{
    if (m_battleMode)
        m_battleMode->onUpdate(delta);
}

void GameView::onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event* event)
{
    setInputFromKey(code, true);
}

void GameView::onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event* event)
{
    setInputFromKey(code, false);
}

void GameView::setInputFromKey(ax::EventKeyboard::KeyCode code, bool pressed)
{
    if (!m_battleMode)
        return;

    auto it = m_slotMap.find(code);
    if (it == m_slotMap.end())
        return;

    m_battleMode->setInput(it->second, pressed);
}

bool GameView::initGameWord()
{
    auto currentScene = ax::Director::getInstance()->getRunningScene();
    if (!currentScene)
    {
        MG_LOG_E("GameView: running scene is null");
        return false;
    }

    m_keyboardListener                = ax::EventListenerKeyboard::create();
    m_keyboardListener->onKeyPressed  = AX_CALLBACK_2(GameView::onKeyPressed, this);
    m_keyboardListener->onKeyReleased = AX_CALLBACK_2(GameView::onKeyReleased, this);
    ax::Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(m_keyboardListener,
                                                                                              currentScene);

    m_gameWord               = std::make_unique<mugen::GameWord>();
    const std::uint64_t seed = (m_boot && m_boot->randomSeed != 0) ? m_boot->randomSeed : 0xe53c2;
    if (!m_gameWord->init(currentScene, seed))
    {
        MG_LOG_E("GameView: GameWord::init failed");
        return false;
    }

    return true;
}

std::unique_ptr<IBattleMode> GameView::createBattleMode()
{
    if (m_boot)
        return std::make_unique<OnlineBattleMode>(std::move(*m_boot));

    if (m_local)
        return std::make_unique<LocalBattleMode>(m_local->roomId);

    return std::make_unique<LocalBattleMode>();
}

void GameView::onImGUIRender()
{
    // 仅本地副本模式显示返回城镇按钮
    if (!m_local || m_local->roomId <= 0)
        return;

    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200.0f, 80.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("副本");

    ImGui::Text("房间 ID: %d", m_local->roomId);
    if (ImGui::Button("返回城镇", ImVec2(-1, 0)))
    {
        getViewManager()->switchView<TownView>();
    }

    ImGui::End();

    // 技能调试面板
    Entity* localPlayer = nullptr;
    if (m_gameWord && m_gameWord->getDirector())
    {
        if (auto* director = MG_GET_COMPONENT(m_gameWord->getDirector(), DirectorComponent))
        {
            if (director->localPlayerEntityId != INVALID_ENTITY_ID)
                localPlayer = m_gameWord->ecsManager.getEntity(director->localPlayerEntityId);
        }
    }

    ImGui::SetNextWindowPos(ImVec2(20.0f, 120.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("技能调试");

    if (!localPlayer)
    {
        ImGui::TextUnformatted("无本地玩家");
        ImGui::End();
        return;
    }

    auto* attr     = MG_GET_COMPONENT(localPlayer, AttributeComponent);
    auto* behavior = MG_GET_COMPONENT(localPlayer, BehaviorComponent);
    auto* cast     = MG_GET_COMPONENT(localPlayer, SkillCastComponent);
    auto* deck     = MG_GET_COMPONENT(localPlayer, SkillDeckComponent);
    auto* skillBar = MG_GET_COMPONENT(localPlayer, SkillBarComponent);

    if (attr)
    {
        ImGui::Text("HP %.0f / %.0f", attr->currentAttribute.hp, static_cast<float>(attr->currentAttribute.hpMax));
        ImGui::Text("MP %.0f / %.0f", attr->currentAttribute.mp, static_cast<float>(attr->currentAttribute.mpMax));
        ImGui::Text("EP %.0f / %.0f", attr->ep, attr->epMax);
    }

    if (cast)
    {
        ImGui::Separator();
        ImGui::Text("active=%d pending=%d", cast->activeSkillAttackId, cast->pendingSkillAttackId);
        ImGui::Text("interrupt=%d extra=%d dash=%d", cast->interruptOpen ? 1 : 0, cast->interruptExtraOpen ? 1 : 0,
                    (behavior && (behavior->statusTags & StateTag::kTagDashState)) ? 1 : 0);
        ImGui::Text("thrust=%d", cast->thrustSkillAttackId);
    }

    if (deck)
    {
        ImGui::Separator();
        if (ImGui::Button("清空 CD"))
        {
            for (auto& e : deck->skills)
            {
                e.coolDownMs   = 0;
                e.releaseCount = e.releaseMax > 0 ? e.releaseMax : 1;
            }
        }

        ImGui::TextUnformatted("键位: A=SLOT0, 1-9=SLOT1-9, 0=SLOT10");
        ImGui::BeginChild("skill_list", ImVec2(0, 0), true);
        for (size_t i = 0; i < deck->skills.size(); ++i)
        {
            const auto& e = deck->skills[i];
            const auto* cfg = Config::getInstance()->getSkillAttackConfigById(e.skillAttackId);
            const int32_t mpCost = cfg ? cfg->mp : 0;
            const int32_t epCost = cfg ? cfg->ep : 0;
            const int32_t sorder = cfg ? cfg->sorder : 0;

            // 按 skillBar 真实槽位反查热键（skillIndexs 存的是 actorDataComp->skills 下标，与 deck 下标一致）
            char keyLabel[8] = "?";
            int32_t slotOffset = -1;
            if (skillBar)
            {
                for (const auto& slot : skillBar->skillSlots)
                {
                    if (std::find(slot.skillIndexs.begin(), slot.skillIndexs.end(), static_cast<int32_t>(i)) !=
                        slot.skillIndexs.end())
                    {
                        slotOffset = slot.slotIndex - static_cast<int32_t>(INPUT_SLOT_0);
                        break;
                    }
                }
            }
            if (slotOffset == 0)
                std::snprintf(keyLabel, sizeof(keyLabel), "A");
            else if (slotOffset >= 1 && slotOffset <= 9)
                std::snprintf(keyLabel, sizeof(keyLabel), "%d", slotOffset);
            else if (slotOffset == 10)
                std::snprintf(keyLabel, sizeof(keyLabel), "0");
            else
                std::snprintf(keyLabel, sizeof(keyLabel), "-");

            ImGui::Text("[%s] id %d | CD %d/%d | mp %d ep %d | sorder %d | next %d", keyLabel, e.skillAttackId,
                        e.coolDownMs, e.coolDownMaxMs, mpCost, epCost, sorder, e.nextSkillAttackId);
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

}  // namespace gameui
