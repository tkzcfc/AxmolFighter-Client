#include "MissingAvatarMotion.h"

#include "mugen/core/StdC.h"

#include <set>

namespace
{
// 进程级诊断：同一动作文件上的缺失动作名只记一次，不参与战斗状态。
std::set<std::string> g_loggedMissingAvatarMotions;
}  // namespace

NS_MG_BEGIN

// 查找动作；缺失时按文件与名称只警告一次。
const Motion* findAvatarMotion(const MotionMap& motionMap, const std::string& motionFile, const std::string& motionName)
{
    if (motionName.empty())
        return nullptr;

    const std::string key = motionFile + '\n' + motionName;
    if (g_loggedMissingAvatarMotions.find(key) != g_loggedMissingAvatarMotions.end())
        return nullptr;

    const Motion* motion = motionMap.findMotion(motionName);
    if (!motion && g_loggedMissingAvatarMotions.insert(key).second)
        MG_LOG_W("Avatar: motion '{}' not found in '{}'", motionName, motionFile);
    return motion;
}

NS_MG_END
