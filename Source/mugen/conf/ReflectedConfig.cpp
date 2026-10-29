#include "ReflectedConfig.h"
#include "BuffRules.h"

#include "mugen/core/serialize/BinaryArchive.h"

#include "mugen/core/io/FileUtils.h"
#if !defined(MG_NO_VFS)
#include "vfs/VirtualFileSystem.h"
#endif

#include <vector>

namespace mugen::conf
{
// 发布配置前检查规则类名，不依赖模拟层的移植进度。
bool validateConfigArchive(const ConfigArchive& archive)
{
    for (const auto& row : archive.buff_rule)
    {
        if (!buffRuleIdFromName(row.className))
        {
            MG_LOG_E("config.buff_rule: unknown className '{}' (id={})", row.className, row.id);
            return false;
        }
    }
    return true;
}

// 返回进程共享的只读配置容器。
Config& Config::instance()
{
    static Config config;
    return config;
}

// 验证文件头和反射归档后替换当前配置。
bool Config::loadBytes(std::vector<uint8_t> bytes)
{
    m_loaded  = false;
    m_archive = {};
    if (bytes.empty() || bytes.size() > std::numeric_limits<uint32_t>::max())
        return false;

    ByteBuffer buffer(bytes.data(), static_cast<uint32_t>(bytes.size()));
    for (uint8_t expected : kConfigMagic)
    {
        uint8_t actual = 0;
        if (!buffer.getByte(actual) || actual != expected)
            return false;
    }

    uint32_t version    = 0;
    uint64_t schemaHash = 0;
    if (!buffer.getUint32(version) || !buffer.getUint64(schemaHash) || version != kConfigVersion)
        return false;
    if (schemaHash != configSchemaHash())
    {
        MG_LOG_E("config.bin schema mismatch, re-run config_converter");
        return false;
    }

    ConfigArchive archive;
    BinaryReader reader(buffer);
    if (!reader.read(archive) || !reader.ok() || buffer.getPosition() != buffer.getSize())
        return false;

    if (!validateConfigArchive(archive))
        return false;

    m_archive = std::move(archive);
    m_loaded  = true;
    return true;
}

// 读取文件内容后委托给内存加载流程。
bool Config::load(const std::string& path)
{
#if defined(MG_NO_VFS)
    (void)path;
    return false;
#else
    std::vector<uint8_t> bytes;
    if (io::vfs()->readFile(path, bytes) != vfs::Error::OK)
        return false;
    return loadBytes(std::move(bytes));
#endif
}
}  // namespace mugen::conf
