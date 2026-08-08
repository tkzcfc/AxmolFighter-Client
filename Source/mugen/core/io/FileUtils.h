#pragma once

#include "../StdC.h"
#include "../serialize/ByteBuffer.h"
#include <string>
#include <vector>

NS_MG_BEGIN

namespace io
{

void addSearchPath(const std::string& path, bool front = false);
void clearSearchPaths();
std::string fullFilePath(const std::string& path);

/**
 * 检查目录是否存在
 */
bool isDirectoryExist(const std::string& path);

/**
 * 检查文件是否存在
 */
bool isFileExist(const std::string& path);

/**
 * 创建目录（递归创建所有中间目录）
 */
bool createDirectory(const std::string& path);

/**
 * 将字符串内容写入文件（若文件已存在则覆盖，若目录不存在则自动创建）
 */
bool writeStringToFile(const std::string& content, const std::string& path);

/**
 * 将二进制数据写入文件（若文件已存在则覆盖，若目录不存在则自动创建）
 */
bool writeDataToFile(const char* data, size_t dataSize, const std::string& path);

/**
 * 读取文件内容为字节数组，文件不存在则返回空 vector
 */
std::vector<uint8_t> getDataFromFile(const std::string& path);

/**
 * 读取文件内容为字符串，文件不存在则返回空字符串
 */
std::string getStringFromFile(const std::string& path);

/**
 * 读取文件内容，返回 malloc 分配的缓冲区，大小写入 size。
 * 调用方负责用 free() 释放。失败时返回 nullptr 且 size == 0。
 */
uint8_t* getFileData(const std::string& path, size_t& size);

/**
 * 列出目录下所有文件和子目录（非递归），目录项以 '/' 结尾
 */
std::vector<std::string> listFiles(const std::string& dirPath);

/**
 * 递归列出目录下所有文件和子目录，结果追加到 files，目录项以 '/' 结尾
 */
void listFilesRecursively(const std::string& dirPath, std::vector<std::string>* files);

/**
 * 获取路径的基础名称（不含目录和扩展名）
 * 例如："/path/to/file.txt" -> "file"
 */
std::string getPathBaseNameNoExtension(std::string_view filePath);

/**
 * 获取路径的扩展名（含点号），若无扩展名则返回空字符串
 * 例如："/path/to/file.txt" ->
 * ".txt"

 */
std::string getPathExtension(std::string_view filePath);

/**
 * 规范化路径，将反斜杠替换为正斜杠，并去除多余的斜杠
 */
std::string normalizePath(std::string path);

/**
 * 规范化目录路径，将反斜杠替换为正斜杠，并去除多余的斜杠，确保以 '/' 结尾
 */
std::string normalizeDirPath(std::string path);

std::string joinPath(const std::string& a, const std::string& b);

}  // namespace io

NS_MG_END
