#pragma once

#include "resource/Resource.h"

#include <string>

namespace gameres
{

// 预热一个 FairyGUI 包：始终加载包描述文件。preloadTextures 为 true 时异步预热 ATLAS
// 项纹理，再让 FairyGUI 从 TextureCache 接管（见 UIPackage::getItemAsset）。
// preloadSpine 为 true 时预热 SPINE 项的图集页贴图，骨架只在
// fairygui::UIConfig::useSkeletonCache 为 true 时写入 GCache。
// 两个开关都默认 true。同一 ResourceLoader 内按路径去重，第一次 add 的开关生效。
//
// SOUND 等其他项不处理。卸载由 FGUIPackageManager::unload 按包内 ATLAS/SPINE
// 清单从 TextureCache 和 GCache 精确释放（含运行时懒加载的资源）。
class FguiPackageResource : public Resource
{
public:
    explicit FguiPackageResource(std::string path, bool preloadTextures = true, bool preloadSpine = true)
        : Resource(ResourceType::FguiPackage)
        , m_path(std::move(path))
        , m_preloadTextures(preloadTextures)
        , m_preloadSpine(preloadSpine)
    {}

    std::string getKey() const override { return m_path; }
    const std::string& getPath() const { return m_path; }

    bool preloadTextures() const { return m_preloadTextures; }
    bool preloadSpine() const { return m_preloadSpine; }

    // 加载函数调用 FGUIPackageManager::load() 后立即标记，失败或取消时也能对称释放引用计数。
    void markAcquired() { m_acquired = true; }

    void releaseHold() override;

private:
    std::string m_path;
    bool m_preloadTextures = true;
    bool m_preloadSpine    = true;
    bool m_acquired        = false;
};

}  // namespace gameres
