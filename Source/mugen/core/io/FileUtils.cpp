#include "FileUtils.h"

#include <algorithm>
#include <cctype>

NS_MG_BEGIN

namespace io
{
namespace
{
vfs::VirtualFileSystem* s_vfs = nullptr;
}  // namespace

void setVfs(vfs::VirtualFileSystem* fs)
{
    s_vfs = fs;
}

vfs::VirtualFileSystem* vfs()
{
    MG_ASSERT(s_vfs != nullptr);
    return s_vfs;
}

std::string getPathBaseNameNoExtension(std::string_view filePath)
{
    size_t pos = filePath.find_last_of("/\\");
    size_t dot = filePath.find_last_of('.');
    if (pos != std::string::npos)
        return std::string{filePath.substr(pos + 1, dot != std::string_view::npos ? dot - (pos + 1) : dot)};

    return std::string{filePath.substr(0, dot)};
}

std::string getPathExtension(std::string_view filePath)
{
    std::string fileExtension;
    size_t pos = filePath.find_last_of('.');
    if (pos != std::string::npos)
    {
        fileExtension = filePath.substr(pos, filePath.length());
        std::transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), ::tolower);
    }

    return fileExtension;
}

std::string normalizePath(std::string path)
{
    for (char& c : path)
    {
        if (c == '\\')
            c = '/';
    }
    return path;
}

std::string normalizeDirPath(std::string path)
{
    for (char& c : path)
    {
        if (c == '\\')
            c = '/';
    }
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    return path + "/";
}

std::string joinPath(const std::string& a, const std::string& b)
{
    if (a.empty())
        return b;
    if (b.empty())
        return a;

    auto normalizedA = normalizeDirPath(a);
    auto normalizedB = normalizeDirPath(b);
    return normalizedA + normalizedB;
}

}  // namespace io

NS_MG_END
