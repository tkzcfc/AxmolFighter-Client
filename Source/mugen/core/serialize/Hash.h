#pragma once

#include "BinaryArchive.h"

#include <cstddef>
#include <cstdint>

NS_MG_BEGIN

// xxHash64 实现所需的固定常量和内部混合函数。
namespace hash_detail
{
inline constexpr uint64_t kPrime1 = 11400714785074694791ULL;
inline constexpr uint64_t kPrime2 = 14029467366897019727ULL;
inline constexpr uint64_t kPrime3 = 1609587929392839161ULL;
inline constexpr uint64_t kPrime4 = 9650029242287828579ULL;
inline constexpr uint64_t kPrime5 = 2870177450012600261ULL;

// 对 64 位字执行非零位数循环左移。
inline uint64_t rotateLeft(uint64_t value, uint32_t count)
{
    return (value << count) | (value >> (64 - count));
}

// 从字节序列读取小端 32 位无符号数。
inline uint32_t read32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

// 从字节序列读取小端 64 位无符号数。
inline uint64_t read64(const uint8_t* data)
{
    return static_cast<uint64_t>(read32(data)) | (static_cast<uint64_t>(read32(data + 4)) << 32);
}

// 对单个输入字执行 xxHash64 轮混合。
inline uint64_t round(uint64_t accumulator, uint64_t input)
{
    accumulator += input * kPrime2;
    accumulator = rotateLeft(accumulator, 31);
    return accumulator * kPrime1;
}
}  // namespace hash_detail

// 计算指定字节序列的确定性 xxHash64；空数据可传空指针。
inline uint64_t xxh64(const uint8_t* data, std::size_t size, uint64_t seed = 0)
{
    using namespace hash_detail;
    const uint8_t empty = 0;
    if (data == nullptr)
    {
        MG_ASSERT(size == 0);
        data = &empty;
    }
    const uint8_t* cursor = data;
    const uint8_t* end    = data + size;
    uint64_t hash;
    if (size >= 32)
    {
        uint64_t lane1       = seed + kPrime1 + kPrime2;
        uint64_t lane2       = seed + kPrime2;
        uint64_t lane3       = seed;
        uint64_t lane4       = seed - kPrime1;
        const uint8_t* limit = end - 32;
        do
        {
            lane1 = round(lane1, read64(cursor));
            lane2 = round(lane2, read64(cursor + 8));
            lane3 = round(lane3, read64(cursor + 16));
            lane4 = round(lane4, read64(cursor + 24));
            cursor += 32;
        } while (cursor <= limit);
        hash = rotateLeft(lane1, 1) + rotateLeft(lane2, 7) + rotateLeft(lane3, 12) + rotateLeft(lane4, 18);
        for (uint64_t lane : {lane1, lane2, lane3, lane4})
        {
            hash ^= round(0, lane);
            hash = hash * kPrime1 + kPrime4;
        }
    }
    else
        hash = seed + kPrime5;

    hash += size;
    while (cursor + 8 <= end)
    {
        hash ^= round(0, read64(cursor));
        hash = rotateLeft(hash, 27) * kPrime1 + kPrime4;
        cursor += 8;
    }
    if (cursor + 4 <= end)
    {
        hash ^= static_cast<uint64_t>(read32(cursor)) * kPrime1;
        hash = rotateLeft(hash, 23) * kPrime2 + kPrime3;
        cursor += 4;
    }
    while (cursor < end)
    {
        hash ^= static_cast<uint64_t>(*cursor++) * kPrime5;
        hash = rotateLeft(hash, 11) * kPrime1;
    }
    hash ^= hash >> 33;
    hash *= kPrime2;
    hash ^= hash >> 29;
    hash *= kPrime3;
    hash ^= hash >> 32;
    return hash;
}

// 归档值后计算其规范字节表示的哈希。
template <class T>
uint64_t hashCanonical(const T& value, uint64_t seed = 0)
{
    ByteBuffer buffer;
    BinaryWriter writer(buffer);
    writer.write(value);
    writer.finish();
    return xxh64(buffer.data(), buffer.len(), seed);
}

NS_MG_END
