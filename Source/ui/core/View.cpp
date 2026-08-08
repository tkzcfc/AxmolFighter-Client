#include "View.h"
#include "AppContext.h"
#include "FGUIPackageManager.h"

using namespace fairygui;

namespace gameui
{

View::View() {}

View::~View() {}

void View::_create()
{
    FGUIPackageManager::getInstance().load(getPackages());
    GComponent* content = onCreateContent();
    if (content)
    {
        m_root = content;
    }
    else
    {
        m_root = GComponent::create();
        m_root->setSize(GRoot::getInstance()->getWidth(), GRoot::getInstance()->getHeight());
        m_root->addRelation(GRoot::getInstance(), RelationType::Size);
    }
    onEnter();
}

void View::_destroy()
{
    detach();

    onExit();
    if (m_root)
    {
        m_root->removeFromParent();
        m_root = nullptr;
    }
    FGUIPackageManager::getInstance().unload(getPackages());
}

void View::addClickListener(UIEventDispatcher* dispatcher, const std::function<void(EventContext*)>& callback)
{
    dispatcher->addEventListener(UIEventType::Click, callback);
}

}  // namespace gameui
