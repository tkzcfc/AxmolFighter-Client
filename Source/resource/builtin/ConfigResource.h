#pragma once

#include "resource/Resource.h"

#include <string>

namespace gameres
{

// 加载游戏的二进制配置表。已经加载过的直接算完成。
// 文件在主线程解析成全路径后交给工作线程读取和反序列化，避免 worker 线程使用 FileUtils 的相对路径缓存。
class ConfigResource : public Resource
{
public:
    enum class Kind : uint8_t
    {
        Mugen,         // mugen::Config（config.bin）
        AvatarAssets,  // mugen::AvatarAssetCache（avatar.bin）
    };

    ConfigResource(Kind kind, std::string path) : Resource(ResourceType::Config), m_kind(kind), m_path(std::move(path))
    {}

    std::string getKey() const override { return m_path; }

    Kind getKind() const { return m_kind; }
    const std::string& getPath() const { return m_path; }

private:
    Kind m_kind;
    std::string m_path;
};

}  // namespace gameres
