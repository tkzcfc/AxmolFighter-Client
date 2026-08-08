#include "StringUtils.h"

NS_MG_BEGIN

std::string StringUtils::replaceAll(const std::string& str, std::string_view from, std::string_view to)
{
    if (from.empty())
        return str;

    std::string out = str;
    size_t pos      = 0;
    while ((pos = out.find(from, pos)) != std::string::npos)
    {
        out.replace(pos, from.size(), to);
        pos += to.size();
    }
    return out;
}

NS_MG_END
