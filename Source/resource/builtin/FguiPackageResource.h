#pragma once

#include "resource/Resource.h"

#include <string>

namespace gameres
{

// 预热一个 FairyGUI 包：加载包描述文件，异步预热其中 ATLAS 项的纹理，
// 再让 FairyGUI 从 TextureCache 接管纹理（见 UIPackage::getItemAsset）。
//
// SOUND 等其他项不处理。SPINE 项只在 fairygui::UIConfig::useSkeletonCache 为 true 时预热到
// fairygui::GCache（否则 GLoader3D 每次都同步加载，预热没有意义）。
// GCache 当前没有按需释放接口，预热过的数据会常驻到进程退出。
class FguiPackageResource : public Resource
{
public:
    explicit FguiPackageResource(std::string path) : Resource(ResourceType::FguiPackage), m_path(std::move(path)) {}

    std::string getKey() const override { return m_path; }
    const std::string& getPath() const { return m_path; }

    // 加载函数调用 FGUIPackageManager::load() 后立即标记，失败或取消时也能对称释放引用计数。
    void markAcquired() { m_acquired = true; }

    void releaseHold() override;

private:
    std::string m_path;
    bool m_acquired = false;
};

}  // namespace gameres
