#pragma once
#include "mugen/sim_legacy/WorldSetup.h"
#include "mugen/sim_legacy/input/FrameInput.h"
#include <string>

namespace mugen::sim_legacy::net
{
// 将世界配置编码为确定性字节串，供 protobuf bytes 字段携带。
std::string encodeWorldSetup(const WorldSetup& setup);
// 从字节串解码世界配置；完整且有效时返回 true。
bool decodeWorldSetup(const std::string& bytes, WorldSetup& setup);

// 将单个玩家槽位的输入编码为确定性字节串。
std::string encodePlayerInput(const PlayerInput& input);
// 从字节串解码玩家输入；完整且有效时返回 true。
bool decodePlayerInput(const std::string& bytes, PlayerInput& input);

// 将整帧确认输入编码为确定性字节串。
std::string encodeFrameInput(const FrameInput& input);
// 从字节串解码整帧确认输入；完整且有效时返回 true。
bool decodeFrameInput(const std::string& bytes, FrameInput& input);
}  // namespace mugen::sim_legacy::net
