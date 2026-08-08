#include "LoginView.h"
#include "ui/widgets/login/LoginPanel.h"

namespace gameui
{

void LoginView::onEnter()
{
    this->getViewManager()->getUIManager()->open<LoginPanel>();
}

}  // namespace gameui
