#pragma once

#include "mugen/avatar/data/MotionMap.h"

#include <string>

NS_MG_BEGIN

// 在动作图中查找动作。空名称不记日志。
// 同一动作文件上的缺失名称只警告一次，之后跳过查找。
const Motion* findAvatarMotion(const MotionMap& motionMap,
                               const std::string& motionFile,
                               const std::string& motionName);

NS_MG_END
