#include "Panel.h"

namespace gameui
{

Panel::Panel()
{
    m_options.hasBackground  = false;
    m_options.draggable      = false;
    m_options.fullscreen     = true;
    m_options.closeOnClickBg = false;
}

Panel::~Panel() {}

GComponent* Panel::onCreateContent()
{
    GComponent* content = onCreatePanelContent();
    content->setSize(m_root->getWidth(), m_root->getHeight());
    content->addRelation(m_root, RelationType::Size);
    return content;
}

}  // namespace gameui
