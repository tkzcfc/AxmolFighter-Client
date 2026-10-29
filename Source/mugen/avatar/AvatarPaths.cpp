#include "mugen/avatar/AvatarPaths.h"
#include "mugen/conf/ReflectedConfig.h"

NS_MG_BEGIN

namespace
{

// 用新扩展名替换路径末尾扩展名。
std::string replaceExtension(const std::string& path, const std::string& newExt)
{
    const auto slash = path.find_last_of('/');
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + newExt;
    return path.substr(0, dot) + newExt;
}

}  // namespace

// 从骨架路径推导同名动作图路径。
std::string AvatarPaths::motionFileFromSpine(const std::string& spineSkeleton)
{
    if (spineSkeleton.empty())
        return {};

    // spineSkeleton必须以/开头
    MG_ASSERT(!spineSkeleton.empty() && spineSkeleton.front() == '/' && "spineSkeleton path must start with '/'");

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

// 按角色配置的 resSpineId 解析渲染所需路径和缩放。
SpineInfo AvatarPaths::resolveRoleSpine(int32_t roleId)
{
    const auto* role = conf::Config::instance().find<conf::RoleConfig>(roleId);
    if (!role)
        return {};
    return resolveSpine(role->resSpineId);
}

// 从 res_spine 行解析渲染所需路径与缩放。
SpineInfo AvatarPaths::resolveSpine(int32_t resSpineId)
{
    SpineInfo info;
    if (resSpineId <= 0)
        return info;

    const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    if (!spine || spine->fileName.empty())
        return info;

    info.skeleton = spine->fileName;
    info.atlas    = replaceExtension(spine->fileName, ".atlas");
    info.scale    = spine->scale > Fixed{} ? spine->scale.toFloatForRender() : 1.0f;
    info.valid    = true;
    return info;
}

NS_MG_END
