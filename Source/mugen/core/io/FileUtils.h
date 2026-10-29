#pragma once

#include "../StdC.h"

#include <string>
#include <string_view>

namespace vfs
{
class VirtualFileSystem;
}

NS_MG_BEGIN

namespace io
{

// 进程入口设置的 VFS。整个进程只有一个实例，客户端 FileUtils 代理持有它。
void setVfs(vfs::VirtualFileSystem* fs);

// 返回入口已设置的 VFS。调用前必须已经 setVfs。
vfs::VirtualFileSystem* vfs();

/**
 * 获取路径的基础名称（不含目录和扩展名）
 * 例如："/path/to/file.txt" -> "file"
 */
std::string getPathBaseNameNoExtension(std::string_view filePath);

/**
 * 获取路径的扩展名（含点号），若无扩展名则返回空字符串
 * 例如："/path/to/file.txt" -> ".txt"
 */
std::string getPathExtension(std::string_view filePath);

/**
 * 规范化路径，将反斜杠替换为正斜杠
 */
std::string normalizePath(std::string path);

/**
 * 规范化目录路径，将反斜杠替换为正斜杠，并去除多余的斜杠，确保以 '/' 结尾
 */
std::string normalizeDirPath(std::string path);

// 连接两段路径并统一目录分隔符。
std::string joinPath(const std::string& a, const std::string& b);

}  // namespace io

NS_MG_END
