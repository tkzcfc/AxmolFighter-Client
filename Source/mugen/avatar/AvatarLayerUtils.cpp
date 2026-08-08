#include "AvatarLayerUtils.h"

#include "mugen/avatar/AvatarLayerDef.h"
#include "mugen/component/AvatarComponent.h"

#include <algorithm>
#include <cctype>

NS_MG_BEGIN

std::string AvatarLayerUtils::resolveAssetPath(const std::string& baseDir, const std::string& fileName)
{
    if (fileName.empty())
        return baseDir;
    if (baseDir.empty())
    {
        std::string file = fileName;
        for (char& c : file)
        {
            if (c == '\\')
                c = '/';
        }
        std::transform(file.begin(), file.end(), file.begin(),
                       [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        return file;
    }

    std::string file = fileName;
    for (char& c : file)
    {
        if (c == '\\')
            c = '/';
    }

    std::string result;
    if (file.rfind("mugen/", 0) == 0)
        result = file;
    else
    {
        std::string base = baseDir;
        for (char& c : base)
        {
            if (c == '\\')
                c = '/';
        }
        while (!base.empty() && base.back() == '/')
            base.pop_back();
        result = base + "/" + file;
    }

    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    return result;
}

std::string AvatarLayerUtils::spinePathToBoxDir(const std::string& spineSkeleton)
{
    if (spineSkeleton.empty())
        return {};

    std::string norm = spineSkeleton;
    for (char& c : norm)
    {
        if (c == '\\')
            c = '/';
    }

    const std::string kSpinePrefix = "mugen/spine/";
    const auto pos                 = norm.find(kSpinePrefix);
    std::string under;
    if (pos != std::string::npos)
        under = norm.substr(pos + kSpinePrefix.size());
    else
    {
        const auto slash = norm.find_last_of('/');
        under            = slash == std::string::npos ? norm : norm.substr(slash + 1);
    }

    const auto dot = under.find_last_of('.');
    if (dot != std::string::npos)
        under = under.substr(0, dot);

    if (under.empty())
        return {};
    return "mugen/box/" + under;
}

std::vector<AvatarLayerDef> AvatarLayerUtils::resolveLayersFromSpine(const std::string& spineSkeleton,
                                                                     const std::string& motionFile,
                                                                     const std::string& defaultAnimationPath)
{
    std::vector<AvatarLayerDef> result;
    AvatarLayerDef def;
    def.motionMapPath = motionFile;
    def.baseDir       = defaultAnimationPath;
    if (def.motionMapPath.empty() && !spineSkeleton.empty())
    {
        const std::string boxDir = spinePathToBoxDir(spineSkeleton);
        if (!boxDir.empty())
            def.baseDir = boxDir;
    }
    def.order = 0;
    def.tag   = kAvatarLayerTagCharacter;
    result.push_back(std::move(def));
    return result;
}

std::vector<AvatarLayerDef> AvatarLayerUtils::resolveLayers(const AvatarComponent* avatar)
{
    if (!avatar)
        return {};
    return resolveLayersFromSpine(avatar->getSpineSkeleton(), avatar->motionFile, avatar->defaultAnimationPath);
}

NS_MG_END
