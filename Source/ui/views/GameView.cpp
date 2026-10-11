#include "GameView.h"

#include "TownView.h"
#include "ui/battle/LocalBattleMode.h"
#include "ui/core/ViewManager.h"

#include "imgui.h"

namespace gameui
{

GameView::GameView() = default;

GameView::GameView(LocalBattleParams local) : m_local(std::move(local)) {}

GameView::~GameView() = default;

void GameView::onPrepareLoad()
{
    addLoadStep([this] { return initInput(); });
    addLoadStep([this] {
        m_battleMode = createBattleMode();
        if (!m_battleMode || !m_battleMode->init())
        {
            AXLOGE("GameView: battle mode init failed");
            m_battleMode = nullptr;
            return false;
        }
        return true;
    });
}

void GameView::onEnter()
{
    Super::onEnter();
    m_slotMap.clear();
    using Key = ax::EventKeyboard::KeyCode;
    using ControlKey = mugen::sim::ControlKey;
    m_slotMap[Key::KEY_LEFT_ARROW] = static_cast<uint32_t>(ControlKey::Left);
    m_slotMap[Key::KEY_RIGHT_ARROW] = static_cast<uint32_t>(ControlKey::Right);
    m_slotMap[Key::KEY_UP_ARROW] = static_cast<uint32_t>(ControlKey::Up);
    m_slotMap[Key::KEY_DOWN_ARROW] = static_cast<uint32_t>(ControlKey::Down);
    m_slotMap[Key::KEY_U] = static_cast<uint32_t>(ControlKey::AttackA);
    m_slotMap[Key::KEY_I] = static_cast<uint32_t>(ControlKey::AttackB);
    m_slotMap[Key::KEY_O] = static_cast<uint32_t>(ControlKey::AttackC);
    m_slotMap[Key::KEY_J] = static_cast<uint32_t>(ControlKey::AttackD);
    m_slotMap[Key::KEY_K] = static_cast<uint32_t>(ControlKey::AttackE);
    m_slotMap[Key::KEY_L] = static_cast<uint32_t>(ControlKey::AttackF);
    m_slotMap[Key::KEY_M] = static_cast<uint32_t>(ControlKey::AttackG);
    m_slotMap[Key::KEY_N] = static_cast<uint32_t>(ControlKey::AttackH);
    m_slotMap[Key::KEY_8] = static_cast<uint32_t>(ControlKey::AttackI);
    m_slotMap[Key::KEY_9] = static_cast<uint32_t>(ControlKey::AttackJ);
    m_slotMap[Key::KEY_0] = static_cast<uint32_t>(ControlKey::AttackK);
    m_slotMap[Key::KEY_SEMICOLON] = static_cast<uint32_t>(ControlKey::AttackL);
    m_slotMap[Key::KEY_Y] = static_cast<uint32_t>(ControlKey::AttackM);
    m_slotMap[Key::KEY_T] = static_cast<uint32_t>(ControlKey::AttackN);
    m_slotMap[Key::KEY_H] = static_cast<uint32_t>(ControlKey::AttackO);
    m_slotMap[Key::KEY_Q] = static_cast<uint32_t>(ControlKey::AttackQ);
    AXLOGI("GameView: entered (localRoom={})", m_local.has_value() ? m_local->roomId : 0);
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

    m_placeholder = nullptr;
    Super::onExit();
}

void GameView::onUpdate(float delta)
{
    if (m_battleMode)
        m_battleMode->onUpdate(delta);
    if (auto* local = dynamic_cast<LocalBattleMode*>(m_battleMode.get()); local && local->returnToTownRequested())
        getViewManager()->switchView<TownView>();
}

void GameView::onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event* /*event*/)
{
    if (code == ax::EventKeyboard::KeyCode::KEY_F8)
    {
        if (auto* local = dynamic_cast<LocalBattleMode*>(m_battleMode.get()))
            local->toggleCollisionDebug();
        return;
    }
    setInputFromKey(code, true);
}

void GameView::onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event* /*event*/)
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

bool GameView::initInput()
{
    auto* currentScene = ax::Director::getInstance()->getRunningScene();
    if (!currentScene)
    {
        AXLOGE("GameView: running scene is null");
        return false;
    }

    m_keyboardListener                = ax::EventListenerKeyboard::create();
    m_keyboardListener->onKeyPressed  = AX_CALLBACK_2(GameView::onKeyPressed, this);
    m_keyboardListener->onKeyReleased = AX_CALLBACK_2(GameView::onKeyReleased, this);
    ax::Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(m_keyboardListener,
                                                                                              currentScene);
    return true;
}

std::unique_ptr<IBattleMode> GameView::createBattleMode()
{
    if (m_local)
        return std::make_unique<LocalBattleMode>(*m_local);
    return std::make_unique<LocalBattleMode>();
}

void GameView::showPlaceholder()
{
    auto* scene = ax::Director::getInstance()->getRunningScene();
    if (!scene)
        return;

    m_placeholder      = ax::Label::createWithSystemFont("Battle is being rebuilt", "Arial", 28);
    const auto visible = ax::Director::getInstance()->getVisibleSize();
    m_placeholder->setPosition(visible.width * 0.5f, visible.height * 0.5f);
    scene->addChild(m_placeholder, 100);
}

void GameView::onImGUIRender()
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
        if (auto* local = dynamic_cast<LocalBattleMode*>(m_battleMode.get())) local->requestReturnToTown();
    ImGui::End();
}

}  // namespace gameui
