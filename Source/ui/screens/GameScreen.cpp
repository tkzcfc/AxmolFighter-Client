#include "GameScreen.h"

#include "TownScreen.h"
#include "mugen/conf/ReflectedConfig.h"
#include "resource/builtin/MapResource.h"
#include "ui/battle/LocalBattleMode.h"
#include "ui/core/ScreenManager.h"
#include "ui/widgets/common/MessageDialog.h"

#include "imgui.h"

namespace gameui
{

GameScreen::GameScreen() = default;

GameScreen::GameScreen(LocalBattleParams local) : m_local(std::move(local)) {}

GameScreen::~GameScreen() = default;

void GameScreen::onPrepareLoad()
{
    const int32_t roomId = m_local ? m_local->roomId : 0;
    if (roomId <= 0)
        return;
    const auto* room = mugen::conf::Config::instance().find<mugen::conf::RoomConfig>(roomId);
    if (!room || room->mapKey.empty())
        return;
    const std::string layer = room->mapKey.ends_with(".layer") ? room->mapKey : "mugen/map/" + room->mapKey + ".layer";
    getResourceLoader().add<gameres::MapResource>(layer);
}

void GameScreen::onEnter()
{
    Super::onEnter();
    if (!initInput())
    {
        MessageDialog::showGlobal("初始化失败", []() { ax::Director::getInstance()->end(); });
        return;
    }
    m_battleMode = createBattleMode();
    if (!m_battleMode || !m_battleMode->init())
    {
        AXLOGE("GameScreen: battle mode init failed");
        m_battleMode = nullptr;
        MessageDialog::showGlobal("初始化失败", []() { ax::Director::getInstance()->end(); });
        return;
    }
    m_slotMap.clear();
    using Key                       = ax::EventKeyboard::KeyCode;
    using ControlKey                = mugen::sim::ControlKey;
    m_slotMap[Key::KEY_LEFT_ARROW]  = static_cast<uint32_t>(ControlKey::Left);
    m_slotMap[Key::KEY_RIGHT_ARROW] = static_cast<uint32_t>(ControlKey::Right);
    m_slotMap[Key::KEY_UP_ARROW]    = static_cast<uint32_t>(ControlKey::Up);
    m_slotMap[Key::KEY_DOWN_ARROW]  = static_cast<uint32_t>(ControlKey::Down);
    m_slotMap[Key::KEY_X]           = static_cast<uint32_t>(ControlKey::AttackA);
    m_slotMap[Key::KEY_Z]           = static_cast<uint32_t>(ControlKey::AttackB);
    m_slotMap[Key::KEY_C]           = static_cast<uint32_t>(ControlKey::AttackC);
    m_slotMap[Key::KEY_A]           = static_cast<uint32_t>(ControlKey::AttackD);
    m_slotMap[Key::KEY_S]           = static_cast<uint32_t>(ControlKey::AttackE);
    m_slotMap[Key::KEY_D]           = static_cast<uint32_t>(ControlKey::AttackF);
    m_slotMap[Key::KEY_F]           = static_cast<uint32_t>(ControlKey::AttackG);
    m_slotMap[Key::KEY_G]           = static_cast<uint32_t>(ControlKey::AttackH);
    m_slotMap[Key::KEY_H]           = static_cast<uint32_t>(ControlKey::AttackI);
    m_slotMap[Key::KEY_Q]           = static_cast<uint32_t>(ControlKey::AttackJ);
    m_slotMap[Key::KEY_W]           = static_cast<uint32_t>(ControlKey::AttackK);
    m_slotMap[Key::KEY_E]           = static_cast<uint32_t>(ControlKey::AttackL);
    m_slotMap[Key::KEY_R]           = static_cast<uint32_t>(ControlKey::AttackM);
    m_slotMap[Key::KEY_T]           = static_cast<uint32_t>(ControlKey::AttackN);
    m_slotMap[Key::KEY_Y]           = static_cast<uint32_t>(ControlKey::AttackO);
    m_slotMap[Key::KEY_U]           = static_cast<uint32_t>(ControlKey::AttackQ);
    AXLOGI("GameScreen: entered (localRoom={})", m_local.has_value() ? m_local->roomId : 0);
}

void GameScreen::onExit()
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

    Super::onExit();
}

void GameScreen::onUpdate(float delta)
{
    if (m_battleMode)
        m_battleMode->onUpdate(delta);
    if (auto* local = dynamic_cast<LocalBattleMode*>(m_battleMode.get()); local && local->returnToTownRequested())
        getScreenManager()->switchScreen<TownScreen>();
}

void GameScreen::onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event* /*event*/)
{
    if (code == ax::EventKeyboard::KeyCode::KEY_F8)
    {
        if (auto* local = dynamic_cast<LocalBattleMode*>(m_battleMode.get()))
            local->toggleCollisionDebug();
        return;
    }
    setInputFromKey(code, true);
}

void GameScreen::onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event* /*event*/)
{
    setInputFromKey(code, false);
}

void GameScreen::setInputFromKey(ax::EventKeyboard::KeyCode code, bool pressed)
{
    if (!m_battleMode)
        return;

    auto it = m_slotMap.find(code);
    if (it == m_slotMap.end())
        return;

    m_battleMode->setInput(it->second, pressed);
}

bool GameScreen::initInput()
{
    auto* currentScene = ax::Director::getInstance()->getRunningScene();
    if (!currentScene)
    {
        AXLOGE("GameScreen: running scene is null");
        return false;
    }

    m_keyboardListener                = ax::EventListenerKeyboard::create();
    m_keyboardListener->onKeyPressed  = AX_CALLBACK_2(GameScreen::onKeyPressed, this);
    m_keyboardListener->onKeyReleased = AX_CALLBACK_2(GameScreen::onKeyReleased, this);
    ax::Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(m_keyboardListener,
                                                                                              currentScene);
    return true;
}

std::unique_ptr<IBattleMode> GameScreen::createBattleMode()
{
    if (m_local)
        return std::make_unique<LocalBattleMode>(*m_local);
    return std::make_unique<LocalBattleMode>();
}

void GameScreen::onImGUIRender()
{
    if (auto* localBattle = dynamic_cast<LocalBattleMode*>(m_battleMode.get()))
        localBattle->renderDebugPanel();
    if (!m_local || m_local->roomId <= 0)
        return;

    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(220.0f, 90.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("副本");
    ImGui::Text("房间 ID: %d", m_local->roomId);
    if (ImGui::Button("返回城镇", ImVec2(-1, 0)))
        if (auto* local = dynamic_cast<LocalBattleMode*>(m_battleMode.get()))
            local->requestReturnToTown();
    ImGui::End();
}

}  // namespace gameui
