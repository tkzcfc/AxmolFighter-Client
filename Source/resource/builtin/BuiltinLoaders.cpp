#include "resource/builtin/BuiltinLoaders.h"

#include "resource/LoadTask.h"
#include "resource/ResourceLoaderRegistry.h"

#include "resource/builtin/AudioResource.h"
#include "resource/builtin/ConfigResource.h"
#include "resource/builtin/FguiPackageResource.h"
#include "resource/builtin/MapResource.h"
#include "resource/builtin/SpineResource.h"
#include "resource/builtin/SpriteFramesResource.h"
#include "resource/builtin/TextureResource.h"

#include "ui/core/FGUIPackageManager.h"

#include "mugen/avatar/data/AvatarAssetCache.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/render/LayerRuntimeLoader.h"
#include "mugen/render/spine/MgSpineUtils.h"
#include "mugen/render/spine/SpineSkeletonCache.h"

#include "FairyGUI.h"
#include "GCache.h"

#include "audio/AudioEngine.h"

namespace gameres
{

namespace
{

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

        const bool preloadTextures = res->preloadTextures();
        const bool preloadSpine    = res->preloadSpine();
        auto items                 = gameui::FGUIPackageManager::collectPackageItems(pkg);

        bool spawned = false;
        if (preloadTextures)
        {
            for (auto* item : items.atlasItems)
            {
                task->spawn<TextureResource>(item->file);
                spawned = true;
            }
        }
        if (preloadSpine)
        {
            for (auto* item : items.spineItems)
            {
                for (auto& texturePath :
                     mugen::collectTexturePaths({gameui::FGUIPackageManager::deriveSpineAtlas(item->file)}))
                {
                    task->spawn<TextureResource>(texturePath);
                    spawned = true;
                }
            }
        }

        if (!spawned)
        {
            task->complete();
            return;
        }

        task->onChildrenDone([task, pkg, preloadTextures, preloadSpine, atlasItems = std::move(items.atlasItems),
                              spineItems = std::move(items.spineItems)]() {
            if (preloadTextures)
            {
                // 纹理已经在 TextureCache 中，FairyGUI 在这里接管它们（见 UIPackage::loadAtlas）
                for (auto* item : atlasItems)
                    pkg->getItemAsset(item);
            }
            if (preloadSpine)
            {
                // 骨架解析在主线程同步执行；纹理已经预热，不会卡在贴图解码上
                for (auto* item : spineItems)
                    fairygui::GCache::getInstance()->preloadSkeletonData(
                        item->file, gameui::FGUIPackageManager::deriveSpineAtlas(item->file));
            }

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
        if (isMugen ? mugen::conf::Config::instance().loaded() : mugen::AvatarAssetCache::getInstance()->isLoaded())
        {
            task->complete();
            return;
        }

        const std::string path = res->getPath();
        auto ok = std::make_shared<bool>(false);
        task->runOnWorker([isMugen, path, ok]() {
            *ok = isMugen ? mugen::conf::Config::instance().load(path)
                          : mugen::AvatarAssetCache::getInstance()->load(path);
        }, [task, ok]() {
            if (*ok)
                task->complete();
            else
                task->fail("failed to load config");
        });
    }, 2.0f);
}

void registerMap(ResourceLoaderRegistry& registry)
{
    registry.registerType<MapResource>(ResourceType::Map,
                                       [](std::shared_ptr<MapResource> res, const LoadTaskPtr& task) {
        const std::string layerFile = res->getLayerFile();
        // 只解析 JSON、不创建节点，解析逻辑和真正建图共用（见 LayerRuntimeLoader::collectResources）
        auto resources = std::make_shared<mugen::LayerResourceList>();
        task->runOnWorker([layerFile, resources]() {
            *resources = mugen::LayerRuntimeLoader::collectResources(layerFile);
        }, [task, resources]() {
            for (const auto& [skeleton, atlas] : resources->spines)
                task->spawn<SpineResource>(skeleton, std::vector<std::string>{atlas}, 1.0f);
            for (const auto& texture : resources->textures)
                task->spawn<TextureResource>(texture);
            for (const auto& plist : resources->spriteFrames)
                task->spawn<SpriteFramesResource>(plist);
            if (resources->soundId > 0)
            {
                if (const auto* sound =
                        mugen::conf::Config::instance().find<mugen::conf::ResSoundConfig>(resources->soundId))
                    task->spawn<AudioResource>(sound->fileName);
            }

            task->onChildrenDone([task]() {
                if (task->anyChildFailed())
                    task->fail("one or more map resources failed to preload");
                else
                    task->complete();
            });
        });
    }, 10.0f);
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
    registerMap(registry);
}

}  // namespace gameres
