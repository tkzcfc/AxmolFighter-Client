#include "LoginView.h"
#include "ui/widgets/login/LoginPanel.h"
#include "ui/core/AudioManager.h"

namespace gameui
{

void LoginView::onEnter()
{
    AudioManager::getInstance()->playBGM("mugen/sound/bgm_fightdusk.mp3");
    this->getViewManager()->getUIManager()->open<LoginPanel>();
}

}  // namespace gameui
