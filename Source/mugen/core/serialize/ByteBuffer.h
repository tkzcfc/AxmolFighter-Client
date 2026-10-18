#pragma once

#include "../StdC.h"

#include <limits>

NS_MG_BEGIN

// 管理可增长字节存储及其顺序读写位置。
class ByteBuffer
{
public:
    // 构造空的自有缓冲区。
    ByteBuffer();
    // 构造指定初始容量的自有缓冲区。
    ByteBuffer(uint32_t capacity);
    // 接管调用方提供的缓冲区及容量。
    ByteBuffer(uint8_t* buffer, uint32_t capacity);
    // 转移另一缓冲区的内存和游标所有权。
    ByteBuffer(ByteBuffer&& other) noexcept;

    // 释放当前由缓冲区拥有的内存。
    virtual ~ByteBuffer();

    // 将读写游标重置到有效数据范围内的指定位置。
    void resetPosition(uint32_t pos = 0U);
    // 完成读取并清空缓冲区。
    void readFinish();
    // 完成写入并将游标设为有效数据长度。
    void writeFinish();
    // 返回底层字节地址。
    uint8_t* data();
    // 返回有效数据字节数。
    uint32_t len();
    // 清空有效数据并将游标归零。
    void clear();

    /**
     * 直接接管外部 malloc 分配的内存指针，无拷贝。
     * 原有缓冲区会被 free。
     * @param data  必须是 malloc 分配的指针，生命周期交由 ByteBuffer 接管
     * @param size  有效数据字节数
     */
    void fastSet(uint8_t* data, uint32_t size);

    // 从当前游标读取布尔值，空间不足时返回 false。
    bool getBool(bool& value);
    void writeBool(const bool& value);

    bool getInt8(int8_t& value);
    void writeInt8(const int8_t& value);

    bool getByte(byte& value);
    void writeByte(const byte& value);

    bool getInt16(int16_t& value);
    void writeInt16(const int16_t& value);

    bool getUint16(uint16_t& value);
    void writeUint16(const uint16_t& value);

    bool getInt32(int32_t& value);
    void writeInt32(const int32_t& value);

    bool getUint32(uint32_t& value);
    void writeUint32(const uint32_t& value);

    bool getInt64(int64_t& value);
    void writeInt64(const int64_t& value);

    bool getUint64(uint64_t& value);
    void writeUint64(const uint64_t& value);

    // 读取带 uint32 长度前缀的字符串。
    bool getString(std::string& value);
    void writeString(const std::string& value);
    void writeString(const std::string_view value);
    static constexpr bool isStringLengthValid(std::size_t length)
    {
        return length <= std::numeric_limits<uint32_t>::max();
    }

    // 写入 uint32 长度前缀及随后的原始字节。
    void writeBinary(byte* data, uint32_t len);

protected:
    // 确保当前游标之后可写入指定数量的字节。
    void resize(uint32_t addLen);

    MG_FORCEINLINE uint32_t getWriteLimit()
    {
        MG_ASSERT(m_position <= m_capacity);
        return m_capacity - m_position;
    }

    MG_FORCEINLINE uint32_t getReadLimit()
    {
        MG_ASSERT(m_position <= m_size);
        return m_size - m_position;
    }

    MG_FORCEINLINE uint8_t* ptr() { return m_buffer + m_position; }

    MG_SYNTHESIZE_READONLY(uint32_t, m_position, Position);
    MG_SYNTHESIZE_READONLY(uint32_t, m_size, Size);
    MG_SYNTHESIZE_READONLY(uint32_t, m_capacity, Capacity);
    MG_SYNTHESIZE_READONLY(uint8_t*, m_buffer, Buffer);
};

NS_MG_END
