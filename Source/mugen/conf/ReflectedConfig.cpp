#include "ReflectedConfig.h"

#include "mugen/core/io/FileUtils.h"
#include "mugen/core/serialize/BinaryArchive.h"

namespace mugen::conf
{
bool Config::load(const std::string& path)
{
    return loadBytes(io::getDataFromFile(path));
}

bool Config::loadBytes(std::vector<uint8_t> bytes)
{
    m_loaded = false;
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

    m_archive = std::move(archive);
    m_loaded  = true;
    return true;
}
}  // namespace mugen::conf
