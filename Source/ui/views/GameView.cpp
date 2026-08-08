#include "GameView.h"

#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/GameDef.h"
#include "ui/battle/LocalBattleMode.h"
#include "ui/battle/OnlineBattleMode.h"

using namespace mugen;

namespace gameui
{

GameView::GameView() = default;

GameView::GameView(BattleBootParams boot) : m_boot(std::move(boot)) {}

GameView::~GameView() = default;

void GameView::onEnter()
{
    Super::onEnter();

    m_slotMap[ax::EventKeyboard::KeyCode::KEY_LEFT_ARROW]  = INPUT_SLOT_MOVE_LEFT;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_RIGHT_ARROW] = INPUT_SLOT_MOVE_RIGHT;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_UP_ARROW]    = INPUT_SLOT_MOVE_UP;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_DOWN_ARROW]  = INPUT_SLOT_MOVE_DOWN;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_A]           = INPUT_SLOT_0;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_C]           = INPUT_SLOT_C;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_Z]           = INPUT_SLOT_Z;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_X]           = INPUT_SLOT_X;

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

    MG_LOG_I("GameView: entered (local={}, seed ready)", !m_boot.has_value());
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

    return std::make_unique<LocalBattleMode>();
}

}  // namespace gameui
