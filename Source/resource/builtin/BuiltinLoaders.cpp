#include "resource/builtin/BuiltinLoaders.h"

#include "resource/LoadTask.h"
#include "resource/ResourceLoaderRegistry.h"

#include "resource/builtin/AudioResource.h"
#include "resource/builtin/ConfigResource.h"
#include "resource/builtin/FguiPackageResource.h"
#include "resource/builtin/SpineResource.h"
#include "resource/builtin/SpriteFramesResource.h"
#include "resource/builtin/TextureResource.h"

#include "ui/core/FGUIPackageManager.h"

#include "mugen/avatar/data/AvatarAssetCache.h"
#include "mugen/conf/Config.h"
#include "mugen/render/spine/MgSpineUtils.h"
#include "mugen/render/spine/SpineSkeletonCache.h"

#include "FairyGUI.h"
#include "GCache.h"

#include "audio/AudioEngine.h"

namespace gameres
{

namespace
{

// 推断 spine atlas 文件名：同名 .atlas。
std::string deriveFguiSpineAtlas(const std::string& skeletonFile)
{
    auto pos = skeletonFile.find_last_of('.');
    return (pos == std::string::npos ? "" : skeletonFile.substr(0, pos)) + ".atlas";
}

// 推断 plist 精灵图集的纹理文件名
std::string deriveSpriteFramesTexture(const std::string& plist)
{
    // 0 表示不启用 plist metadata 解析，避免在后台线程调用 FileUtils（它对相对路径的缓存不是线程安全的）。
    // 并且在大多数项目中plist和png是同名的，解析metadata并没有什么必要。
    // 1 表示启用 plist metadata 解析，和引擎 PlistSpriteSheetLoader::load(plist) 的推断规则保持一致：优先读 plist 里
    // metadata.textureFileName（相对 plist 所在目录解析），否则退回 plist 同名的 .png。
#define ENABLE_PLIST_METADATA_TEXTURE_FILENAME 0

#if ENABLE_PLIST_METADATA_TEXTURE_FILENAME
    auto* fileUtils     = ax::FileUtils::getInstance();
    const auto fullPath = fileUtils->fullPathForFilename(plist);
    std::string texturePath;
    if (!fullPath.empty())
    {
        auto dict = fileUtils->getValueMapFromFile(fullPath);
        if (auto metaIt = dict.find("metadata"); metaIt != dict.end())
        {
            auto& metadataDict = metaIt->second.asValueMap();
            if (auto nameIt = metadataDict.find("textureFileName"); nameIt != metadataDict.end())
                texturePath = nameIt->second.asString();
        }
        if (!texturePath.empty())
            return fileUtils->fullPathFromRelativeFile(texturePath, plist);
    }
#endif

    auto pos = plist.find_last_of('.');
    return (pos == std::string::npos ? plist : plist.substr(0, pos)) + ".png";
}

// 说明：LoadTask 在取消后所有方法都是空操作，下面的异步回调不需要再判断 isCancelled()。

void registerTexture(ResourceLoaderRegistry& registry)
{
    registry.registerType<TextureResource>(ResourceType::Texture,
                                           [](std::shared_ptr<TextureResource> res, const LoadTaskPtr& task) {
        ax::Director::getInstance()->getTextureCache()->addImageAsync(res->getPath(),
                                                                      [res, task](ax::Texture2D* texture) {
            if (!texture)
            {
                task->fail(fmt::format("failed to decode texture: {}", res->getPath()));
                return;
            }
            res->setTexture(texture);
            task->complete();
        });
    }, 1.0f);
}

void registerSpriteFrames(ResourceLoaderRegistry& registry)
{
    registry.registerType<SpriteFramesResource>(ResourceType::SpriteFrames,
                                                [](std::shared_ptr<SpriteFramesResource> res, const LoadTaskPtr& task) {
        std::string texturePath = res->getTexturePath();
        if (texturePath.empty())
            texturePath = deriveSpriteFramesTexture(res->getPlist());

        auto texture = task->spawn<TextureResource>(texturePath);
        task->onChildrenDone([res, task, texture]() {
            if (!texture->isLoaded())
            {
                task->fail("failed to load sprite frame texture: " + texture->getPath());
                return;
            }
            ax::SpriteFrameCache::getInstance()->addSpriteFramesWithFile(res->getPlist(), texture->getTexture());
            task->complete();
        });
    }, 1.0f);
}

void registerSpine(ResourceLoaderRegistry& registry)
{
    registry.registerType<SpineResource>(ResourceType::Spine,
                                         [](std::shared_ptr<SpineResource> res, const LoadTaskPtr& task) {
        auto onDone = [res, task](mugen::MgSkeletonDataPtr data) {
            if (!data)
            {
                task->fail("failed to load spine skeleton");
                return;
            }
            res->setData(std::move(data));
            task->complete();
        };

        auto* cache = mugen::SpineSkeletonCache::getInstance();
        if (res->isByResId())
            cache->loadAsync(res->getResSpineId(), onDone);
        else
            cache->loadAsync(res->getSkeletonFile(), res->getAtlasFiles(), res->getScale(), onDone);
    }, 3.0f);
}

void registerFguiPackage(ResourceLoaderRegistry& registry)
{
    registry.registerType<FguiPackageResource>(ResourceType::FguiPackage,
                                               [](std::shared_ptr<FguiPackageResource> res, const LoadTaskPtr& task) {
        gameui::FGUIPackageManager::getInstance().load({res->getPath()});
        // 引用计数已经加上，不管后续是否成功都要在 releaseHold() 里对称释放
        res->markAcquired();

        auto* pkg = fairygui::UIPackage::getByName(gameui::FGUIPackageManager::getPackageName(res->getPath()));
        if (!pkg)
        {
            task->fail("failed to add FairyGUI package");
            return;
        }

        std::vector<fairygui::PackageItem*> atlasItems;
        std::vector<fairygui::PackageItem*> spineItems;
        for (auto* item : pkg->getItems())
        {
            if (item->type == fairygui::PackageItemType::ATLAS)
            {
                atlasItems.push_back(item);
            }
            // 此处不管是否开启 fairygui::UIConfig::useSkeletonCache，都预热 spine
            // 纹理；如果不开启，纹理预热也不会有任何副作用。
            else if (item->type == fairygui::PackageItemType::SPINE)
            // else if (item->type == fairygui::PackageItemType::SPINE && fairygui::UIConfig::useSkeletonCache)
            {
                spineItems.push_back(item);
            }
        }

        for (auto* item : atlasItems)
            task->spawn<TextureResource>(item->file);
        for (auto* item : spineItems)
        {
            for (auto& texturePath : mugen::collectTexturePaths({deriveFguiSpineAtlas(item->file)}))
                task->spawn<TextureResource>(texturePath);
        }

        task->onChildrenDone([task, pkg, atlasItems, spineItems]() {
            // 纹理已经在 TextureCache 中，FairyGUI 在这里接管它们（见 UIPackage::loadAtlas）
            for (auto* item : atlasItems)
                pkg->getItemAsset(item);
            // 骨架解析在主线程同步执行；纹理已经预热，不会卡在贴图解码上
            for (auto* item : spineItems)
                fairygui::GCache::getInstance()->preloadSkeletonData(item->file, deriveFguiSpineAtlas(item->file));

            if (task->anyChildFailed())
                task->fail("one or more textures failed to preload");
            else
                task->complete();
        });
    }, 10.0f);
}

void registerAudio(ResourceLoaderRegistry& registry)
{
    registry.registerType<AudioResource>(ResourceType::Audio,
                                         [](std::shared_ptr<AudioResource> res, const LoadTaskPtr& task) {
        ax::AudioEngine::preload(res->getPath(), [task](bool success) {
            if (success)
                task->complete();
            else
                task->fail("failed to preload audio");
        });
    }, 1.0f);
}

void registerConfig(ResourceLoaderRegistry& registry)
{
    registry.registerType<ConfigResource>(ResourceType::Config,
                                          [](std::shared_ptr<ConfigResource> res, const LoadTaskPtr& task) {
        const bool isMugen = res->getKind() == ConfigResource::Kind::Mugen;
        if (isMugen ? mugen::Config::getInstance()->isLoaded() : mugen::AvatarAssetCache::getInstance()->isLoaded())
        {
            task->complete();
            return;
        }

        // 全路径在主线程解析；worker 线程只用全路径读文件
        std::string fullPath = ax::FileUtils::getInstance()->fullPathForFilename(res->getPath());
        if (fullPath.empty())
        {
            task->fail("config file not found");
            return;
        }

        auto ok = std::make_shared<bool>(false);
        task->runOnWorker([isMugen, fullPath, ok]() {
            *ok = isMugen ? mugen::Config::getInstance()->loadConfig(fullPath)
                          : mugen::AvatarAssetCache::getInstance()->load(fullPath);
        }, [task, ok]() {
            if (*ok)
                task->complete();
            else
                task->fail("failed to load config");
        });
    }, 2.0f);
}
}  // namespace

void registerBuiltinLoaders(ResourceLoaderRegistry& registry)
{
    registerTexture(registry);
    registerSpriteFrames(registry);
    registerSpine(registry);
    registerFguiPackage(registry);
    registerAudio(registry);
    registerConfig(registry);
}

}  // namespace gameres
