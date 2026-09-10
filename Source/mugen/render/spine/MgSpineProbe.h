#pragma once

#include "mugen/render/spine/MgSpineRuntime.h"

#ifdef RUNTIME_IN_AXMOL

#include <cstring>
#include <string_view>

NS_MG_BEGIN

inline std::string atlasFromSpine(std::string_view spine)
{
    const std::string path(spine);
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + ".atlas";
    return path.substr(0, dot) + ".atlas";
}

// 获取数据的第一个有效字节，跳过BOM和空白字符
inline unsigned char firstPayloadByte(const ax::Data& data)
{
    const unsigned char* bytes = data.getBytes();
    size_t size                = static_cast<size_t>(data.getSize());
    size_t i                   = 0;
    if (size >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
        i = 3;
    while (i < size)
    {
        const unsigned char c = bytes[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            ++i;
            continue;
        }
        return c;
    }
    return 0;
}

// 判断spine skeleton文件的头部是否包含3.4版本标记
inline bool headerLooksLikeSpine34(const ax::Data& data)
{
    constexpr std::size_t kSpineVersionProbeBytes = 200;
    const unsigned char* bytes                    = data.getBytes();
    const std::size_t size                        = static_cast<std::size_t>(data.getSize());
    const std::size_t window                      = size < kSpineVersionProbeBytes ? size : kSpineVersionProbeBytes;
    if (window < 3)
        return false;

    static constexpr char kMarker[] = "3.4";
    for (std::size_t i = 0; i + 3 <= window; ++i)
    {
        if (std::memcmp(bytes + i, kMarker, 3) == 0)
            return true;
    }
    return false;
}

NS_MG_END

#endif
