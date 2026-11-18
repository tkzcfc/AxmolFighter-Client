#pragma once

#include "mugen/sim/WorldSetup.h"
#include "mugen/sim/input/FrameInput.h"
#include "mugen/core/serialize/BinaryArchive.h"

namespace mugen::sim
{
// 回放二进制版本：魔数 MGRP 后的 32 位版本号。
inline constexpr uint32_t kReplayVersion = 5;

// 回放二进制顺序：MGRP、版本、配置模式哈希、世界配置、帧输入、终局哈希。
struct Replay
{
    // 写入时使用的配置模式哈希。
    uint64_t configSchemaHash = 0;
    // 重建世界所需的初始配置。
    WorldSetup setup;
    // 从第一逻辑帧开始连续保存的输入。
    std::vector<FrameInput> frames;
    // 回放结束时的世界哈希，供对账。
    uint64_t finalWorldHash = 0;

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
