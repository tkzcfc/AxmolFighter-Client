#include "FGUIPackageManager.h"
#include "FairyGUI.h"

using namespace fairygui;

namespace
{
// 从资源路径中提取包名，例如 "UI/Common" -> "Common"
std::string extractName(const std::string& path)
{
    auto pos = path.rfind('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}
}  // namespace

namespace gameui
{

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
            UIPackage::addPackage(path + "/" + extractName(path));
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
            UIPackage::removePackage(extractName(path));
            m_refCounts.erase(it);
        }
    }
}

}  // namespace gameui
