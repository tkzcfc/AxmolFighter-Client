#include "NetCodec.h"

#include "mugen/core/serialize/BinaryArchive.h"

#include <string>
#include <utility>

namespace mugen::sim::net
{
namespace
{
// 将反射值编码为确定性字节串。
template <class T>
std::string encode(const T& value)
{
    ByteBuffer buffer;
    BinaryWriter writer(buffer);
    writer.write(value);
    writer.finish();
    return std::string(reinterpret_cast<const char*>(buffer.data()), buffer.len());
}
// 从完整字节串解码反射值；载荷耗尽且无残留时返回 true。
template <class T>
bool decode(const std::string& bytes, T& value)
{
    ByteBuffer buffer;
    for (uint8_t byte : bytes)
        buffer.writeByte(byte);
    buffer.writeFinish();
    BinaryReader reader(buffer);
    T result{};
    if (!reader.read(result))
        return false;
    if (buffer.getPosition() != buffer.getSize())
        return false;
    value = std::move(result);
    return true;
}
}  // namespace

std::string encodeWorldSetup(const WorldSetup& setup)
{
    return encode(setup);
}
bool decodeWorldSetup(const std::string& bytes, WorldSetup& setup)
{
    return decode(bytes, setup);
}
std::string encodePlayerInput(const PlayerInput& input)
{
    return encode(input);
}
bool decodePlayerInput(const std::string& bytes, PlayerInput& input)
{
    return decode(bytes, input);
}
std::string encodeFrameInput(const FrameInput& input)
{
    return encode(input);
}
bool decodeFrameInput(const std::string& bytes, FrameInput& input)
{
    return decode(bytes, input);
}
}  // namespace mugen::sim::net
