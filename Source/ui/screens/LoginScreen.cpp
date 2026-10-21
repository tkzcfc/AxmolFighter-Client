#include "LoginScreen.h"
#include "ui/widgets/login/LoginPanel.h"
#include "ui/core/AudioManager.h"

namespace gameui
{

void LoginScreen::onEnter()
{
    AudioManager::getInstance()->playBGM("mugen/sound/bgm_fightdusk.mp3");
    this->getScreenManager()->getUIManager()->open<LoginPanel>();
}

}  // namespace gameui
