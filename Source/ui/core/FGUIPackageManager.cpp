#include "FGUIPackageManager.h"

#include "FairyGUI.h"
#include "GCache.h"

#include "mugen/render/spine/MgSpineUtils.h"

#include <utility>

using namespace fairygui;

namespace gameui
{

std::string FGUIPackageManager::getPackageName(const std::string& path)
{
    auto pos = path.rfind('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string FGUIPackageManager::deriveSpineAtlas(const std::string& skeletonFile)
{
    auto pos = skeletonFile.find_last_of('.');
    return (pos == std::string::npos ? "" : skeletonFile.substr(0, pos)) + ".atlas";
}

FguiPackageItems FGUIPackageManager::collectPackageItems(fairygui::UIPackage* pkg)
{
    FguiPackageItems items;
    for (auto* item : pkg->getItems())
    {
        if (item->type == PackageItemType::ATLAS)
            items.atlasItems.push_back(item);
        else if (item->type == PackageItemType::SPINE)
            items.spineItems.push_back(item);
    }
    return items;
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
            const std::string name = getPackageName(path);
            auto* pkg              = UIPackage::getByName(name);

            std::vector<std::string> texturePaths;
            std::vector<std::pair<std::string, std::string>> spines;
            if (pkg)
            {
                auto items = collectPackageItems(pkg);
                for (auto* item : items.atlasItems)
                    texturePaths.push_back(item->file);
                for (auto* item : items.spineItems)
                {
                    std::string atlas = deriveSpineAtlas(item->file);
                    spines.emplace_back(item->file, atlas);
                    for (auto& texturePath : mugen::collectTexturePaths({atlas}))
                        texturePaths.push_back(std::move(texturePath));
                }
            }

            UIPackage::removePackage(name);

            auto* gcache = GCache::getInstance();
            for (const auto& [skel, atlas] : spines)
                gcache->removeSkeletonData(skel, atlas);

            auto* textureCache = ax::Director::getInstance()->getTextureCache();
            for (const auto& texturePath : texturePaths)
                textureCache->removeTextureForKey(texturePath);

            m_refCounts.erase(it);
        }
    }
}

}  // namespace gameui
