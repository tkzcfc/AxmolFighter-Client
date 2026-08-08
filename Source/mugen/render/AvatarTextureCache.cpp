#include "AvatarTextureCache.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/core/io/FileUtils.h"
#    include "yasio/ibstream.hpp"

NS_MG_BEGIN

namespace
{
constexpr char kVFrameMagic[] = {'V', 'F', '0', '1'};

std::tuple<std::string, bool> toVFramePath(const std::string& filePath)
{
    auto ext           = io::getPathExtension(filePath);
    std::string vfPath = filePath;
    if (ext == ".png")
    {
        // 优先加载同名 .vf
        auto vfPath = filePath.substr(0, filePath.size() - 4) + ".vf";
        if (ax::FileUtils::getInstance()->isFileExist(vfPath))
        {
            return {vfPath, true};
        }
    }

    return {filePath, false};
}
}  // namespace

ax::SpriteFrame* AvatarTextureCache::getSpriteFrame(const std::string& contentRelativePath)
{
    if (contentRelativePath.empty())
        return nullptr;

    auto [filePath, isVf] = toVFramePath(contentRelativePath);

    if (ax::SpriteFrame* cached = ax::SpriteFrameCache::getInstance()->findFrame(filePath))
        return cached;

    if (isVf)
    {
        return loadVFrame(filePath);
    }
    else
    {
        ax::Texture2D* tex = loadTexture(filePath);
        if (!tex)
            return nullptr;

        const auto size     = tex->getContentSizeInPixels();
        ax::SpriteFrame* sf = ax::SpriteFrame::createWithTexture(tex, ax::Rect(0.0f, 0.0f, size.width, size.height));
        if (!sf)
            return nullptr;
        ax::SpriteFrameCache::getInstance()->addSpriteFrame(sf, filePath);
        return sf;
    }
}

ax::Texture2D* AvatarTextureCache::loadTexture(const std::string& filePath)
{
    if (filePath.empty())
        return nullptr;

    auto* cache        = ax::Director::getInstance()->getTextureCache();
    ax::Texture2D* tex = cache->getTextureForKey(filePath);
    if (tex)
        return tex;

    tex = cache->addImage(filePath);
    if (!tex)
    {
        MG_LOG_W("AvatarTextureCache: PNG not found '{}'", filePath);
        return nullptr;
    }
    return tex;
}

ax::SpriteFrame* AvatarTextureCache::loadVFrame(const std::string& filePath)
{
    ax::Data data = ax::FileUtils::getInstance()->getDataFromFile(filePath);
    if (data.isNull())
        return nullptr;

    try
    {
        yasio::ibstream_view ibs(data.getBytes(), data.getSize());

        char magic[4] = {};
        ibs.read_bytes(magic, 4);
        if (std::memcmp(magic, kVFrameMagic, 4) != 0)
        {
            MG_LOG_W("AvatarTextureCache: invalid .vf magic '{}'", filePath);
            return nullptr;
        }

        const int32_t canvasWidth  = ibs.read<int32_t>();
        const int32_t canvasHeight = ibs.read<int32_t>();
        const int32_t offsetX      = ibs.read<int32_t>();
        const int32_t offsetY      = ibs.read<int32_t>();
        const int32_t width        = ibs.read<int32_t>();
        const int32_t height       = ibs.read<int32_t>();
        const uint32_t dataSize    = ibs.read<uint32_t>();

        if (canvasWidth <= 0 || canvasHeight <= 0)
        {
            MG_LOG_W("AvatarTextureCache: invalid canvas size in '{}'", filePath);
            return nullptr;
        }

        auto* textureCache = ax::Director::getInstance()->getTextureCache();
        auto texure        = textureCache->getTextureForKey(filePath);

        int texW = 0;
        int texH = 0;
        ax::Vec2 frameOffset(0.0f, 0.0f);

        if (width > 0 && height > 0 && dataSize > 0)
        {
            texW = width;
            texH = height;
            frameOffset.x =
                static_cast<float>(offsetX) + static_cast<float>(width) * 0.5f - static_cast<float>(canvasWidth) * 0.5f;
            frameOffset.y = static_cast<float>(canvasHeight) * 0.5f -
                            (static_cast<float>(offsetY) + static_cast<float>(height) * 0.5f);

            // 加载纹理
            if (texure == nullptr)
            {
                const uint32_t expected = static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4u;
                if (dataSize != expected)
                {
                    MG_LOG_W("AvatarTextureCache: .vf dataSize mismatch in '{}'", filePath);
                    return nullptr;
                }
                const auto rgbaView = ibs.read_bytes(static_cast<int>(dataSize));
                if (rgbaView.size() != dataSize)
                {
                    MG_LOG_W("AvatarTextureCache: .vf truncated '{}'", filePath);
                    return nullptr;
                }

                ax::RefPtr<ax::Image> image;
                image.weakAssign(new ax::Image());
                if (!image->initWithRawData(reinterpret_cast<const uint8_t*>(rgbaView.data()),
                                            static_cast<ssize_t>(rgbaView.size()), width, height, 8, false))
                {
                    MG_LOG_W("AvatarTextureCache: failed to init image from '{}'", filePath);
                    return nullptr;
                }
                texure = textureCache->addImage(image, filePath);
            }
        }
        else
        {
            // 空白纹理
            texure = textureCache->getDummyTexture();
            texW   = 1;
            texH   = 1;
        }

        if (!texure)
        {
            MG_LOG_W("AvatarTextureCache: failed to create texture from '{}'", filePath);
            return nullptr;
        }

        auto rect            = ax::Rect(0.0f, 0.0f, static_cast<float>(texW), static_cast<float>(texH));
        auto originImageSize = ax::Vec2(static_cast<float>(canvasWidth), static_cast<float>(canvasHeight));
        ax::SpriteFrame* sf  = ax::SpriteFrame::createWithTexture(texure, rect, false, frameOffset, originImageSize);
        if (!sf)
        {
            MG_LOG_W("AvatarTextureCache: failed to create SpriteFrame from '{}'", filePath);
            return nullptr;
        }

        ax::SpriteFrameCache::getInstance()->addSpriteFrame(sf, filePath);
        return sf;
    }
    catch (const std::exception& ex)
    {
        MG_LOG_W("AvatarTextureCache: failed to parse .vf '{}': {}", filePath, ex.what());
        return nullptr;
    }
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
