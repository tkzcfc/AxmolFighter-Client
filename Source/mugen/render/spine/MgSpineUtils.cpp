#include "MgSpineUtils.h"

#ifdef RUNTIME_IN_AXMOL

#    include <atomic>
#    include <cstring>
#    include <memory>
#    include <set>

NS_MG_BEGIN

namespace
{

inline void runOnMain(std::function<void()> fn)
{
    ax::Director::getInstance()->getScheduler()->runOnAxmolThread(std::move(fn));
}

inline void runAsync(std::function<void()> fn)
{
    ax::Director::getInstance()->getJobSystem()->enqueue(std::move(fn));
}

// 从 atlas 文本提取全部页贴图的完整路径（libgdx atlas：空行分页，每页块首行为贴图文件名）。
// 纯文本处理，可在后台线程调用；路径拼接规则与 spine 加载器一致（atlas 所在目录 + 图名）。
std::vector<std::string> atlasTexturePaths(std::string_view atlasText, std::string_view atlasFile)
{
    std::vector<std::string> out;

    const std::string file(atlasFile);
    const auto slash      = file.find_last_of("/\\");
    const std::string dir = (slash == std::string::npos) ? std::string() : file.substr(0, slash + 1);

    bool expectPage = true;
    size_t pos      = 0;
    while (pos < atlasText.size())
    {
        size_t eol = atlasText.find('\n', pos);
        if (eol == std::string_view::npos)
            eol = atlasText.size();
        std::string_view line = atlasText.substr(pos, eol - pos);
        pos                   = eol + 1;

        // 去 \r 与两端空白
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        const auto begin = line.find_first_not_of(" \t");
        if (begin == std::string_view::npos)
        {
            expectPage = true;
            continue;
        }
        line = line.substr(begin, line.find_last_not_of(" \t") - begin + 1);

        if (expectPage)
        {
            out.push_back(dir + std::string(line));
            expectPage = false;
        }
    }
    return out;
}

// 探测 skeletonData 是否可能是 spine 3.4 版本
bool headerLooksLikeSpine34(const void* bytes, size_t size)
{
    constexpr std::size_t kSpineVersionProbeBytes = 200;
    const std::size_t window                      = size < kSpineVersionProbeBytes ? size : kSpineVersionProbeBytes;
    if (window < 3)
        return false;

    static constexpr char kMarker[] = "3.4";
    for (std::size_t i = 0; i + 3 <= window; ++i)
    {
        if (std::memcmp(static_cast<const unsigned char*>(bytes) + i, kMarker, 3) == 0)
            return true;
    }
    return false;
}

unsigned char firstPayloadByte(const ax::Data& data)
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

}  // namespace

std::string atlasFromSpine(std::string_view spine)
{
    const std::string path(spine);
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + ".atlas";
    return path.substr(0, dot) + ".atlas";
}

bool checkIsJsonFormat(const ax::Data& data)
{
    return firstPayloadByte(data) == static_cast<unsigned char>('{');
}

MgSpineRuntime runtimeFromSkeletonData(const ax::Data& data)
{
    return runtimeFromSkeletonData(data.getBytes(), static_cast<size_t>(data.getSize()));
}

MgSpineRuntime runtimeFromSkeletonData(const void* bytes, size_t size)
{
#    if MG_SPINE_USE_3_4
    if (headerLooksLikeSpine34(bytes, size))
        return MgSpineRuntime::Spine34;
#    endif
    return MgSpineRuntime::Axmol;
}

// 收集图集引用的全部页贴图路径（去重）；纯文本 IO，主线程/工作线程均可
std::vector<std::string> collectTexturePaths(const std::vector<std::string>& atlasFiles)
{
    std::vector<std::string> out;
    std::set<std::string> dedup;
    for (const auto& atlasFile : atlasFiles)
    {
        if (atlasFile.empty())
            continue;
        ax::Data text = ax::FileUtils::getInstance()->getDataFromFile(atlasFile);
        if (text.isNull() || text.getSize() <= 0)
            continue;
        std::string_view sv(reinterpret_cast<const char*>(text.getBytes()), static_cast<size_t>(text.getSize()));
        for (auto& path : atlasTexturePaths(sv, atlasFile))
        {
            if (dedup.insert(path).second)
                out.push_back(std::move(path));
        }
    }
    return out;
}

// 主线程：异步解码全部贴图，全部完成（含单张失败）后于主线程 onDone
void prewarmTexturesOnMain(std::vector<std::string> texturePaths, std::function<void()> onDone)
{
    if (texturePaths.empty())
    {
        onDone();
        return;
    }
    auto remaining = std::make_shared<std::atomic<int>>(static_cast<int>(texturePaths.size()));
    auto shared    = std::make_shared<std::function<void()>>(std::move(onDone));
    auto* cache    = ax::Director::getInstance()->getTextureCache();
    for (const auto& path : texturePaths)
    {
        cache->addImageAsync(path, [remaining, shared](ax::Texture2D*) {
            if (remaining->fetch_sub(1) == 1 && *shared)
                (*shared)();
        });
    }
}

NS_MG_END

#endif
