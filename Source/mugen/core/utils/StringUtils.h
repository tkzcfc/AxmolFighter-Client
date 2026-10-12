#pragma once

#include "../StdC.h"

#include <string>
#include <string_view>

NS_MG_BEGIN

// 无状态字符串变换工具集合。
class StringUtils
{
public:
    // 将 str 中所有 from 替换为 to，返回新字符串；from 为空则原样返回
    static std::string replaceAll(const std::string& str, std::string_view from, std::string_view to);
};

NS_MG_END
