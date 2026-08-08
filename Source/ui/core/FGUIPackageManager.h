#pragma once

#include <string>
#include <map>
#include <vector>

namespace gameui
{

// FairyGUI 包管理器
class FGUIPackageManager
{
public:
    static FGUIPackageManager& getInstance();

    // 加载 FairyGUI 包资源，支持引用计数，重复加载不会重复加载包
    void load(const std::vector<std::string>& paths);

    // 卸载 FairyGUI 包资源，支持引用计数，只有当引用计数为 0 时才会真正卸载包
    void unload(const std::vector<std::string>& paths);

private:
    FGUIPackageManager() = default;

    std::map<std::string, int> m_refCounts;
};

}  // namespace gameui
