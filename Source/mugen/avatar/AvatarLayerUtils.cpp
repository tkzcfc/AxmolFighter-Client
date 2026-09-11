#include "AvatarLayerUtils.h"

#include "mugen/core/utils/StringUtils.h"
#include "mugen/avatar/AvatarLayerDef.h"
#include "mugen/component/AvatarComponent.h"

NS_MG_BEGIN

std::string AvatarLayerUtils::spinePathToMotionFile(const std::string& spineSkeleton)
{
    if (spineSkeleton.empty())
        return {};

    std::string path = spineSkeleton;
    for (char& c : path)
    {
        if (c == '\\')
        {
// 尽量在debug期间发现路径中有反斜杠，让路径格式统一为正斜杠
#if _DEBUG
                MG_LOG_E("AvatarLayerUtils: spineSkeleton path contains '\\' separator: '{}'", spineSkeleton);
                MG_ASSERT(false && "spineSkeleton path contains '\\' separator");
#endif
            c = '/';
        }
    }

    const char kSpine[] = "mugen/spine/";
    if (const auto pos = path.find(kSpine); pos != std::string::npos)
        path.replace(pos, sizeof(kSpine) - 1, "mugen/motion/");

    const auto slash = path.find_last_of('/');
    const auto dot   = path.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        path.replace(dot, std::string::npos, ".motion");
    else
    {
        // spineSkeleton 不应该没有扩展名
        MG_LOG_E("AvatarLayerUtils: spineSkeleton path has no extension: '{}'", spineSkeleton);
        MG_ASSERT(false && "spineSkeleton path has no extension");
        return {};
    }
    return path;
}

std::vector<AvatarLayerDef> AvatarLayerUtils::resolveLayersFromSpine(const std::string& motionFile)
{
    if (motionFile.empty())
        return {};
    AvatarLayerDef def;
    def.motionMapPath = motionFile;
    def.order         = 0;
    def.tag           = AvatarLayerTag::kBody;
    return {std::move(def)};
}

std::vector<AvatarLayerDef> AvatarLayerUtils::resolveLayers(const AvatarComponent* avatar)
{
    if (!avatar)
        return {};
    return resolveLayersFromSpine(avatar->motionFile);
}

NS_MG_END
