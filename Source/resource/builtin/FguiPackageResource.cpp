#include "resource/builtin/FguiPackageResource.h"

#include "ui/core/FGUIPackageManager.h"

namespace gameres
{

void FguiPackageResource::releaseHold()
{
    if (!m_acquired)
        return;
    m_acquired = false;
    gameui::FGUIPackageManager::getInstance().unload({m_path});
}

}  // namespace gameres
