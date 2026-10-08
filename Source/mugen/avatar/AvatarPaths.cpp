#include "mugen/avatar/AvatarPaths.h"
#include "mugen/conf/Config.h"

NS_MG_BEGIN

namespace
{

std::string replaceExtension(const std::string& path, const std::string& newExt)
{
    const auto slash = path.find_last_of('/');
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + newExt;
    return path.substr(0, dot) + newExt;
}

}  // namespace

std::string AvatarPaths::motionFileFromSpine(const std::string& spineSkeleton)
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
            MG_LOG_E("AvatarPaths: spineSkeleton path contains '\\' separator: '{}'", spineSkeleton);
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
        MG_LOG_E("AvatarPaths: spineSkeleton path has no extension: '{}'", spineSkeleton);
        MG_ASSERT(false && "spineSkeleton path has no extension");
        return {};
    }
    return path;
}

RoleSpineInfo AvatarPaths::resolveRoleSpine(int32_t roleId)
{
    RoleSpineInfo info;
    const auto* role = Config::getInstance()->getRoleConfigById(roleId);
    if (!role || role->resSpineId <= 0)
        return info;

    const auto* spine = Config::getInstance()->getResSpineConfigById(role->resSpineId);
    if (!spine || spine->spine.empty())
        return info;

    info.skeleton = spine->spine;
    info.atlas    = replaceExtension(spine->spine, ".atlas");
    info.scale    = spine->scale > 0.0f ? spine->scale : 1.0f;
    info.valid    = true;
    return info;
}

NS_MG_END
