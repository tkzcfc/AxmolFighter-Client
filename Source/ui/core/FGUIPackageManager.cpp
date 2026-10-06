#include "FGUIPackageManager.h"
#include "FairyGUI.h"

using namespace fairygui;

namespace gameui
{

std::string FGUIPackageManager::getPackageName(const std::string& path)
{
    auto pos = path.rfind('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

FGUIPackageManager& FGUIPackageManager::getInstance()
{
    static FGUIPackageManager instance;
    return instance;
}

void FGUIPackageManager::load(const std::vector<std::string>& paths)
{
    for (const auto& path : paths)
    {
        auto& count = m_refCounts[path];
        if (count == 0)
        {
            UIPackage::addPackage(path + "/" + getPackageName(path));
        }
        ++count;
    }
}

void FGUIPackageManager::unload(const std::vector<std::string>& paths)
{
    for (const auto& path : paths)
    {
        auto it = m_refCounts.find(path);
        if (it == m_refCounts.end() || it->second <= 0)
            continue;
        --it->second;
        if (it->second == 0)
        {
            UIPackage::removePackage(getPackageName(path));
            m_refCounts.erase(it);
        }
    }
}

}  // namespace gameui
