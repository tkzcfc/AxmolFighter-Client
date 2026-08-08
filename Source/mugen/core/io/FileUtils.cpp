#include "FileUtils.h"

#if RUNTIME_IN_AXMOL
// Axmol 环境下使用 ax::FileUtils
#else
#    include <filesystem>
#    include <fstream>
#    include <iterator>
namespace fs = std::filesystem;
#endif

NS_MG_BEGIN

namespace io
{

#if !RUNTIME_IN_AXMOL
namespace
{
std::vector<std::string>& searchPaths()
{
    static std::vector<std::string> paths;
    return paths;
}

std::string normalizePath(const fs::path& path)
{
    std::string normalized = path.lexically_normal().string();
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

std::string resolvePath(const std::string& path)
{
    fs::path input(path);
    if (input.empty())
        return {};

    std::error_code ec;
    if (input.is_absolute() || fs::exists(input, ec))
        return normalizePath(input);

    for (const auto& searchPath : searchPaths())
    {
        fs::path candidate = fs::path(searchPath) / input;
        ec.clear();
        if (fs::exists(candidate, ec))
            return normalizePath(candidate);
    }

    return normalizePath(input);
}
}  // namespace
#endif

void addSearchPath(const std::string& path, bool front)
{
#if RUNTIME_IN_AXMOL
    ax::FileUtils::getInstance()->addSearchPath(path, front);
#else
    if (path.empty())
        return;

    auto normalized = normalizePath(path);
    auto& paths     = searchPaths();
    paths.erase(std::remove(paths.begin(), paths.end(), normalized), paths.end());

    if (front)
        paths.insert(paths.begin(), std::move(normalized));
    else
        paths.emplace_back(std::move(normalized));
#endif
}

void clearSearchPaths()
{
#if RUNTIME_IN_AXMOL
    // Axmol 搜索路径由引擎管理，此处不清理
#else
    searchPaths().clear();
#endif
}

std::string fullFilePath(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->fullPathForFilename(path);
#else
    return resolvePath(path);
#endif
}

bool isDirectoryExist(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->isDirectoryExist(path);
#else
    std::error_code ec;
    return fs::is_directory(fullFilePath(path), ec);
#endif
}

bool isFileExist(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->isFileExist(path);
#else
    std::error_code ec;
    return fs::is_regular_file(fullFilePath(path), ec);
#endif
}

bool createDirectory(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->createDirectory(path);
#else
    if (path.empty())
        return false;
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
#endif
}

bool writeStringToFile(const std::string& content, const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->writeStringToFile(content, path);
#else
    // 确保父目录存在
    fs::path filePath(path);
    if (auto parent = filePath.parent_path(); !parent.empty())
    {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec)
            return false;
    }

    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out)
        return false;

    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
#endif
}

bool writeDataToFile(const char* data, size_t dataSize, const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::writeBinaryToFile(data, dataSize, path);
#else
    // UTF-8 路径：Windows 下用 fs::u8path，避免中文动画名写文件失败
    fs::path filePath =
#    if defined(_WIN32)
        fs::u8path(path);
#    else
        fs::path(path);
#    endif
    if (auto parent = filePath.parent_path(); !parent.empty())
    {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec)
            return false;
    }

    std::ofstream out(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out)
        return false;

    out.write(data, static_cast<std::streamsize>(dataSize));
    return out.good();
#endif
}

std::vector<uint8_t> getDataFromFile(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    auto data = ax::FileUtils::getInstance()->getDataFromFile(path);
    if (data.isNull())
        return {};
    return std::vector<uint8_t>(data.getBytes(), data.getBytes() + data.getSize());
#else
    std::ifstream in(fullFilePath(path), std::ios::in | std::ios::binary);
    if (!in)
        return {};

    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
#endif
}

std::string getStringFromFile(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->getStringFromFile(path);
#else
    std::ifstream in(fullFilePath(path), std::ios::in | std::ios::binary);
    if (!in)
        return {};

    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
#endif
}

uint8_t* getFileData(const std::string& path, size_t& size)
{
    size = 0;
#if RUNTIME_IN_AXMOL
    auto data = ax::FileUtils::getInstance()->getDataFromFile(path);
    if (data.isNull())
        return nullptr;

    ssize_t dataSize = 0;
    auto ptr         = data.takeBuffer(&dataSize);
    if (dataSize <= 0)
    {
        if (ptr)
        {
            free(ptr);
        }
        return nullptr;
    }

    size = static_cast<size_t>(dataSize);
    return ptr;
#else
    std::ifstream in(fullFilePath(path), std::ios::in | std::ios::binary);
    if (!in)
        return nullptr;

    in.seekg(0, std::ios::end);
    const auto fileSize = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    if (fileSize == 0)
        return nullptr;

    uint8_t* buf = static_cast<uint8_t*>(malloc(fileSize));
    if (!buf)
        return nullptr;

    in.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(fileSize));
    if (!in)
    {
        free(buf);
        return nullptr;
    }
    size = fileSize;
    return buf;
#endif
}

std::vector<std::string> listFiles(const std::string& dirPath)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->listFiles(dirPath);
#else
    // 返回逻辑相对路径（相对 content 根），与 Axmol 搜索路径语义一致
    const std::string resolved = fullFilePath(dirPath);
    std::error_code ec;
    if (!fs::is_directory(resolved, ec))
        return {};

    std::string logicalRoot = dirPath;
    std::replace(logicalRoot.begin(), logicalRoot.end(), '\\', '/');
    while (!logicalRoot.empty() && logicalRoot.back() == '/')
        logicalRoot.pop_back();

    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(resolved, ec))
    {
        const bool isDir = entry.is_directory();
        if (!isDir && !entry.is_regular_file())
            continue;

        std::error_code relEc;
        auto rel = fs::relative(entry.path(), resolved, relEc);
        if (relEc)
            continue;

        std::string p = logicalRoot + "/" + normalizePath(rel);
        if (isDir && p.back() != '/')
            p += '/';
        files.emplace_back(std::move(p));
    }
    return files;
#endif
}

void listFilesRecursively(const std::string& dirPath, std::vector<std::string>* files)
{
#if RUNTIME_IN_AXMOL
    ax::FileUtils::getInstance()->listFilesRecursively(dirPath, files);
#else
    if (!files)
        return;

    const std::string resolved = fullFilePath(dirPath);
    std::error_code ec;
    if (!fs::is_directory(resolved, ec))
        return;

    std::string logicalRoot = dirPath;
    std::replace(logicalRoot.begin(), logicalRoot.end(), '\\', '/');
    while (!logicalRoot.empty() && logicalRoot.back() == '/')
        logicalRoot.pop_back();

    for (const auto& entry : fs::recursive_directory_iterator(resolved, ec))
    {
        const bool isDir = entry.is_directory();
        if (!isDir && !entry.is_regular_file())
            continue;

        std::error_code relEc;
        auto rel = fs::relative(entry.path(), resolved, relEc);
        if (relEc)
            continue;

        std::string p = logicalRoot + "/" + normalizePath(rel);
        if (isDir && p.back() != '/')
            p += '/';
        files->emplace_back(std::move(p));
    }
#endif
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
