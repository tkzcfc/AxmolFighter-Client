#pragma once
#include "WorldSetup.h"
#include "input/FrameInput.h"
#include "mugen/core/serialize/BinaryArchive.h"

namespace mugen::sim
{
// 回放二进制顺序：MGRP、版本、配置模式哈希、种子、世界配置、帧输入。
struct Replay
{
    // 写入时使用的配置模式哈希。
    uint64_t schemaHash = 0;
    // 重建世界所需的初始配置。
    WorldSetup setup;
    // 从第一逻辑帧开始连续保存的输入。
    std::vector<FrameInput> frames;
    // 将回放编码为确定性二进制数据。
    void serialize(ByteBuffer& buffer) const;
    // 校验模式哈希和帧连续性后解码回放。
    bool deserialize(ByteBuffer& buffer, uint64_t loadedSchemaHash, std::string& error);
    // 编码回放并写入指定文件。
    bool save(const std::string& path, std::string& error) const;
    // 从指定文件读取并校验回放。
    bool load(const std::string& path, uint64_t loadedSchemaHash, std::string& error);
};
}  // namespace mugen::sim
