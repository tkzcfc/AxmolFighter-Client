#include "LoginScreen.h"
#include "ui/widgets/login/LoginDialog.h"
#include "core/audio/AudioManager.h"

namespace gameui
{

void LoginScreen::onEnter()
{
    game::AudioManager::getInstance()->playBGM("mugen/sound/bgm_fightdusk.mp3");
    this->getScreenManager()->getUIManager()->open<LoginDialog>();
}

}  // namespace gameui
