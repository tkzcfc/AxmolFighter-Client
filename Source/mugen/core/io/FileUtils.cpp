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
// 保存无引擎运行时的进程级搜索路径顺序。
std::vector<std::string>& searchPaths()
{
    static std::vector<std::string> paths;
    return paths;
}

// 将 UTF-8 字符串转换为文件系统路径。
fs::path pathFromUtf8(const std::string& utf8)
{
#    if defined(__cpp_char8_t)
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
#    else
    return fs::u8path(utf8);
#    endif
}

// 归一化文件系统路径并转换为 UTF-8 正斜杠形式。
std::string pathToUtf8(const fs::path& path)
{
    const auto u8 = path.lexically_normal().generic_u8string();
    return std::string(reinterpret_cast<const char*>(u8.c_str()), u8.size());
}

// 按绝对路径、现有相对路径和搜索路径顺序解析文件。
std::string resolvePath(const std::string& path)
{
    fs::path input = pathFromUtf8(path);
    if (input.empty())
        return {};

    std::error_code ec;
    if (input.is_absolute() || fs::exists(input, ec))
        return pathToUtf8(input);

    for (const auto& searchPath : searchPaths())
    {
        fs::path candidate = pathFromUtf8(searchPath) / input;
        ec.clear();
        if (fs::exists(candidate, ec))
            return pathToUtf8(candidate);
    }

    return pathToUtf8(input);
}
}  // namespace
#endif

// 在无引擎运行时去重并按请求位置维护搜索路径。
void addSearchPath(const std::string& path, bool front)
{
#if RUNTIME_IN_AXMOL
    ax::FileUtils::getInstance()->addSearchPath(path, front);
#else
    if (path.empty())
        return;

    auto normalized = io::normalizePath(path);
    auto& paths     = searchPaths();
    paths.erase(std::remove(paths.begin(), paths.end(), normalized), paths.end());

    if (front)
        paths.insert(paths.begin(), std::move(normalized));
    else
        paths.emplace_back(std::move(normalized));
#endif
}

// 清理仅由无引擎实现持有的搜索路径。
void clearSearchPaths()
{
#if RUNTIME_IN_AXMOL
    // Axmol 搜索路径由引擎管理，此处不清理
#else
    searchPaths().clear();
#endif
}

// 委托当前运行时解析逻辑返回完整文件路径。
std::string fullFilePath(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->fullPathForFilename(path);
#else
    return resolvePath(path);
#endif
}

// 判断解析后路径是否为目录。
bool isDirectoryExist(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->isDirectoryExist(path);
#else
    std::error_code ec;
    return fs::is_directory(pathFromUtf8(fullFilePath(path)), ec);
#endif
}

// 判断解析后路径是否为普通文件。
bool isFileExist(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->isFileExist(path);
#else
    std::error_code ec;
    return fs::is_regular_file(pathFromUtf8(fullFilePath(path)), ec);
#endif
}

// 递归创建目标目录，空路径视为失败。
bool createDirectory(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->createDirectory(path);
#else
    if (path.empty())
        return false;
    std::error_code ec;
    fs::create_directories(pathFromUtf8(path), ec);
    return !ec;
#endif
}

// 创建父目录后以截断方式写入文本内容。
bool writeStringToFile(const std::string& content, const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->writeStringToFile(content, path);
#else
    fs::path filePath = pathFromUtf8(path);
    if (auto parent = filePath.parent_path(); !parent.empty())
    {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec)
            return false;
    }

    std::ofstream out(filePath, std::ios::out | std::ios::trunc);
    if (!out)
        return false;

    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
#endif
}

// 创建父目录后以二进制截断方式写入字节。
bool writeDataToFile(const char* data, size_t dataSize, const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::writeBinaryToFile(data, dataSize, path);
#else
    fs::path filePath = pathFromUtf8(path);
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

// 读取完整文件；无法打开时返回空字节数组。
std::vector<uint8_t> getDataFromFile(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    auto data = ax::FileUtils::getInstance()->getDataFromFile(path);
    if (data.isNull())
        return {};
    return std::vector<uint8_t>(data.getBytes(), data.getBytes() + data.getSize());
#else
    std::ifstream in(pathFromUtf8(fullFilePath(path)), std::ios::in | std::ios::binary);
    if (!in)
        return {};

    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
#endif
}

// 读取完整文件；无法打开时返回空字符串。
std::string getStringFromFile(const std::string& path)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->getStringFromFile(path);
#else
    std::ifstream in(pathFromUtf8(fullFilePath(path)), std::ios::in | std::ios::binary);
    if (!in)
        return {};

    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
#endif
}

// 返回 malloc 分配的文件缓冲区，失败时返回空并将大小置零。
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
    std::ifstream in(pathFromUtf8(fullFilePath(path)), std::ios::in | std::ios::binary);
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

// 列举一级逻辑路径，目录项保留末尾斜杠。
std::vector<std::string> listFiles(const std::string& dirPath)
{
#if RUNTIME_IN_AXMOL
    return ax::FileUtils::getInstance()->listFiles(dirPath);
#else
    // 返回相对内容根的逻辑路径，以保持搜索路径语义一致。
    const fs::path resolved = pathFromUtf8(fullFilePath(dirPath));
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

        std::string p = logicalRoot + "/" + pathToUtf8(rel);
        if (isDir && p.back() != '/')
            p += '/';
        files.emplace_back(std::move(p));
    }
    return files;
#endif
}

// 递归追加逻辑路径，目录项保留末尾斜杠。
void listFilesRecursively(const std::string& dirPath, std::vector<std::string>* files)
{
#if RUNTIME_IN_AXMOL
    ax::FileUtils::getInstance()->listFilesRecursively(dirPath, files);
#else
    if (!files)
        return;

    const fs::path resolved = pathFromUtf8(fullFilePath(dirPath));
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

        std::string p = logicalRoot + "/" + pathToUtf8(rel);
        if (isDir && p.back() != '/')
            p += '/';
        files->emplace_back(std::move(p));
    }
#endif
}

// 提取不含目录和扩展名的基础名称。
std::string getPathBaseNameNoExtension(std::string_view filePath)
{
    size_t pos = filePath.find_last_of("/\\");
    size_t dot = filePath.find_last_of('.');
    if (pos != std::string::npos)
        return std::string{filePath.substr(pos + 1, dot != std::string_view::npos ? dot - (pos + 1) : dot)};

    return std::string{filePath.substr(0, dot)};
}

// 提取小写扩展名，缺失时返回空字符串。
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

// 仅将反斜杠标准化为正斜杠。
std::string normalizePath(std::string path)
{
    for (char& c : path)
    {
        if (c == '\\')
            c = '/';
    }
    return path;
}

// 标准化目录分隔符并确保恰有一个末尾斜杠。
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

// 连接非空路径段；空段直接返回另一段。
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
