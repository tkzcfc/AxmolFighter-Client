#pragma once

#include <string>
#include <map>
#include <vector>

namespace fairygui
{
class UIPackage;
class PackageItem;
}  // namespace fairygui

namespace gameui
{

// 与 registerFguiPackage 使用同一套规则收集的包内资源项
struct FguiPackageItems
{
    std::vector<fairygui::PackageItem*> atlasItems;
    std::vector<fairygui::PackageItem*> spineItems;
};

// FairyGUI 包管理器
class FGUIPackageManager
{
public:
    static FGUIPackageManager& getInstance();

    // 加载 FairyGUI 包资源，支持引用计数，重复加载不会重复加载包
    void load(const std::vector<std::string>& paths);

    // 卸载 FairyGUI 包资源，支持引用计数，只有当引用计数为 0 时才会真正卸载包
    void unload(const std::vector<std::string>& paths);

    // 从包路径提取包名，例如 "UI/Common" -> "Common"
    static std::string getPackageName(const std::string& path);

    // 遍历已加载包，收集 ATLAS / SPINE 项
    static FguiPackageItems collectPackageItems(fairygui::UIPackage* pkg);

    // 推断 spine atlas 文件名：同名 .atlas
    static std::string deriveSpineAtlas(const std::string& skeletonFile);

private:
    FGUIPackageManager() = default;

    std::map<std::string, int> m_refCounts;
};

}  // namespace gameui
