#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class AvatarTextureCache
{
public:
    /**
     * 获取 SpriteFrame。
     * 优先加载同名 .vf（裁切 RGBA + canvas 元数据）,否则加载 PNG。
     */
    static ax::SpriteFrame* getSpriteFrame(const std::string& contentRelativePath);

private:
    // 加载虚拟帧文件, 返回 SpriteFrame 并缓存到 SpriteFrameCache
    static ax::SpriteFrame* loadVFrame(const std::string& filePath);

    // 加载 PNG 文件, 返回 Texture2D 并缓存到 TextureCache
    static ax::Texture2D* loadTexture(const std::string& filePath);
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
