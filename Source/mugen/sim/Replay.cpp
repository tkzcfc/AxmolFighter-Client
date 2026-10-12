#include "Replay.h"
#include <fstream>
#include <limits>

namespace mugen::sim
{
// 按固定字段顺序写出回放数据。
void Replay::serialize(ByteBuffer& buffer) const
{
    BinaryWriter writer(buffer);
    for (uint8_t byte : {'M', 'G', 'R', 'P'})
        writer.write(byte);
    writer.write(uint32_t{1});
    writer.write(schemaHash);
    writer.write(setup.seed);
    writer.write(setup);
    writer.write(frames);
    writer.finish();
}
// 仅在文件完整且模式匹配时替换当前回放。
bool Replay::deserialize(ByteBuffer& buffer, uint64_t loadedSchemaHash, std::string& error)
{
    error.clear();
    BinaryReader reader(buffer);
    for (uint8_t expected : {'M', 'G', 'R', 'P'})
    {
        uint8_t byte = 0;
        if (!reader.read(byte) || byte != expected)
        {
            error = "Invalid replay magic (expected MGRP)";
            return false;
        }
    }
    uint32_t version = 0;
    Replay result;
    uint64_t seed = 0;
    if (!reader.read(version) || version != 1 || !reader.read(result.schemaHash))
    {
        error = "Unsupported or truncated replay header";
        return false;
    }
    if (result.schemaHash != loadedSchemaHash)
    {
        error = "Replay config schemaHash mismatch: file=" + std::to_string(result.schemaHash) +
                " loaded=" + std::to_string(loadedSchemaHash);
        return false;
    }
    if (!reader.read(seed) || !reader.read(result.setup) || seed != result.setup.seed ||
        (result.setup.mode != WorldMode::Battle && result.setup.mode != WorldMode::Town) ||
        !reader.read(result.frames) || buffer.getPosition() != buffer.getSize())
    {
        error = "Invalid or truncated replay payload";
        return false;
    }
    uint64_t frame = 0;
    for (const auto& input : result.frames)
        if (input.frame != ++frame)
        {
            error = "Replay frames must be consecutive starting at 1";
            return false;
        }
    *this = std::move(result);
    return true;
}
// 将编码后的回放保存到二进制文件。
bool Replay::save(const std::string& path, std::string& error) const
{
    ByteBuffer buffer;
    serialize(buffer);
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.len());
    file.close();
    if (!file)
    {
        error = "Cannot write replay: " + path;
        return false;
    }
    error.clear();
    return true;
}
// 读取完整文件后复用反序列化校验流程。
bool Replay::load(const std::string& path, uint64_t loadedSchemaHash, std::string& error)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (!file || size < 0 || static_cast<uint64_t>(size) > std::numeric_limits<uint32_t>::max())
    {
        error = "Cannot read replay: " + path;
        return false;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
    {
        error = "Cannot read complete replay: " + path;
        return false;
    }
    ByteBuffer buffer;
    for (uint8_t byte : bytes)
        buffer.writeByte(byte);
    buffer.writeFinish();
    return deserialize(buffer, loadedSchemaHash, error);
}
}  // namespace mugen::sim
