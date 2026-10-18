
#include "ByteBuffer.h"
#include <cstdlib>

#define USE_ENDIAN 1

#if USE_ENDIAN
#    include "mugen/core/utils/EndianUtils.h"
#    define MG_USE_BIG_ENDIAN 1
#endif

#define READ_CHECK_RETURN_FALSE(len) \
    if (getReadLimit() < (len))      \
    return false
#define WRITE_CHECK_AND_RESIZE(len) \
    if (getWriteLimit() < (len))    \
    this->resize(len)

NS_MG_BEGIN

// 初始化最小可写的自有缓冲区。
ByteBuffer::ByteBuffer() : m_position(0U), m_size(0), m_capacity(0), m_buffer(nullptr)
{
    resize(0);
}

// 分配指定初始容量，初始不含有效数据。
ByteBuffer::ByteBuffer(uint32_t capacity)
{
    m_capacity = capacity;
    m_buffer   = static_cast<byte*>(malloc(m_capacity));
    m_position = 0U;
    m_size     = 0U;
}

// 复制给定字节并将其视为完整有效载荷。
ByteBuffer::ByteBuffer(uint8_t* buffer, uint32_t capacity)
    : m_position(0U), m_size(capacity), m_capacity(capacity), m_buffer(nullptr)
{
    if (m_capacity > 0)
    {
        m_buffer = static_cast<byte*>(malloc(m_capacity));
        ::memcpy(m_buffer, buffer, m_capacity);
    }
}

// 转移内存所有权并清空源对象。
ByteBuffer::ByteBuffer(ByteBuffer&& other) noexcept
    : m_position(other.m_position), m_size(other.m_size), m_capacity(other.m_capacity), m_buffer(other.m_buffer)
{
    other.m_buffer   = nullptr;
    other.m_capacity = 0;
    other.m_size     = 0;
    other.m_position = 0U;
}

// 释放由本对象拥有的 malloc 缓冲区。
ByteBuffer::~ByteBuffer()
{
    if (m_buffer)
    {
        free(m_buffer);
        m_buffer = nullptr;
    }
}

// 在已分配范围内移动顺序读写游标。
void ByteBuffer::resetPosition(uint32_t pos)
{
    MG_ASSERT(pos <= m_capacity);
    m_position = pos;
}

// 读取结束后将游标返回缓冲区起点。
void ByteBuffer::readFinish()
{
    resetPosition(0U);
}

// 写入结束后提交游标之前的字节为有效数据。
void ByteBuffer::writeFinish()
{
    m_size = m_position;
    resetPosition();
}

uint8_t* ByteBuffer::data()
{
    return m_buffer;
}

uint32_t ByteBuffer::len()
{
    return m_size;
}

// 保留容量但丢弃当前有效数据。
void ByteBuffer::clear()
{
    m_position = 0U;
    m_size     = 0U;
}

// 释放旧缓冲区并无拷贝接管 malloc 分配的字节。
void ByteBuffer::fastSet(uint8_t* data, uint32_t size)
{
    if (m_buffer)
        free(m_buffer);
    m_buffer   = data;
    m_size     = size;
    m_capacity = size;
    m_position = 0U;
}

// 按倍增策略扩容，并保留已写入或游标前的字节。
void ByteBuffer::resize(uint32_t addLen)
{
    auto oldCapacity = m_capacity;

    m_capacity *= 2;
    m_capacity = std::max(m_capacity, 128u);

    if (m_capacity < oldCapacity + addLen)
    {
        m_capacity += addLen;
    }

    if (m_buffer == nullptr)
    {
        m_buffer = static_cast<byte*>(malloc(m_capacity));
    }
    else
    {
        auto oldBuf = m_buffer;
        m_buffer    = static_cast<byte*>(malloc(m_capacity));

        ::memcpy(m_buffer, oldBuf, std::max(m_size, m_position));

        free(oldBuf);
    }
}

// 读取单字节布尔值，非零视为 true。
bool ByteBuffer::getBool(bool& value)
{
    byte bval;
    if (getByte(bval))
    {
        value = bval != 0;
        return true;
    }
    return false;
}

// 将布尔值规范编码为零或一字节。
void ByteBuffer::writeBool(const bool& value)
{
    writeByte(value ? 1 : 0);
}

// 读取单字节有符号整数。
bool ByteBuffer::getInt8(int8_t& value)
{
    byte tmp;
    if (getByte(tmp))
    {
        value = (int8_t)tmp;
        return true;
    }
    return false;
}

// 写入单字节有符号整数的位表示。
void ByteBuffer::writeInt8(const int8_t& value)
{
    writeByte((byte)value);
}

// 读取一个原始字节并推进游标。
bool ByteBuffer::getByte(byte& value)
{
    READ_CHECK_RETURN_FALSE(sizeof(byte));

    auto p = (byte*)ptr();
    value  = *p;

    m_position += sizeof(byte);
    return true;
}

// 追加一个原始字节并推进游标。
void ByteBuffer::writeByte(const byte& value)
{
    WRITE_CHECK_AND_RESIZE(sizeof(byte));

    (*(byte*)ptr()) = value;
    m_position += sizeof(byte);
}

// 读取网络字节序的有符号 16 位整数。
bool ByteBuffer::getInt16(int16_t& value)
{
    uint16_t tmp;
    if (getUint16(tmp))
    {
        value = (int16_t)tmp;
        return true;
    }
    return false;
}

// 以网络字节序写入有符号 16 位整数。
void ByteBuffer::writeInt16(const int16_t& value)
{
    writeUint16((uint16_t)value);
}

// 读取网络字节序的无符号 16 位整数。
bool ByteBuffer::getUint16(uint16_t& value)
{
    READ_CHECK_RETURN_FALSE(sizeof(uint16_t));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    value = EndianUtils::readUint16InBigEndian(ptr());
#    else
    value = EndianUtils::readUint16InLittleEndian(ptr());
#    endif
#else
    auto p = (uint16_t*)ptr();
    value  = *p;
#endif

    m_position += sizeof(uint16_t);
    return true;
}

// 以网络字节序写入无符号 16 位整数。
void ByteBuffer::writeUint16(const uint16_t& value)
{
    WRITE_CHECK_AND_RESIZE(sizeof(uint16_t));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    EndianUtils::writeUint16InBigEndian(ptr(), value);
#    else
    EndianUtils::writeUint16InLittleEndian(ptr(), value);
#    endif
#else
    (*(uint16_t*)ptr()) = value;
#endif

    m_position += sizeof(uint16_t);
}

// 读取网络字节序的有符号 32 位整数。
bool ByteBuffer::getInt32(int32_t& value)
{
    uint32_t tmp;
    if (getUint32(tmp))
    {
        value = (int32_t)tmp;
        return true;
    }
    return false;
}

// 以网络字节序写入有符号 32 位整数。
void ByteBuffer::writeInt32(const int32_t& value)
{
    writeUint32((uint32_t)value);
}

// 读取网络字节序的无符号 32 位整数。
bool ByteBuffer::getUint32(uint32_t& value)
{
    READ_CHECK_RETURN_FALSE(sizeof(uint32_t));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    value = EndianUtils::readUint32InBigEndian(ptr());
#    else
    value = EndianUtils::readUint32InLittleEndian(ptr());
#    endif
#else
    auto p = (uint32_t*)ptr();
    value  = *p;
#endif

    m_position += sizeof(uint32_t);
    return true;
}
// 以网络字节序写入无符号 32 位整数。
void ByteBuffer::writeUint32(const uint32_t& value)
{
    WRITE_CHECK_AND_RESIZE(sizeof(uint32_t));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    EndianUtils::writeUint32InBigEndian(ptr(), value);
#    else
    EndianUtils::writeUint32InLittleEndian(ptr(), value);
#    endif
#else
    (*(uint32_t*)ptr()) = value;
#endif

    m_position += sizeof(uint32_t);
}

// 读取网络字节序的有符号 64 位整数。
bool ByteBuffer::getInt64(int64_t& value)
{
    uint64_t tmp;
    if (getUint64(tmp))
    {
        value = (int64_t)tmp;
        return true;
    }
    return false;
}

// 以网络字节序写入有符号 64 位整数。
void ByteBuffer::writeInt64(const int64_t& value)
{
    writeUint64((uint64_t)value);
}

// 读取网络字节序的无符号 64 位整数。
bool ByteBuffer::getUint64(uint64_t& value)
{
    READ_CHECK_RETURN_FALSE(sizeof(uint64_t));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    value = EndianUtils::readUint64InBigEndian(ptr());
#    else
    value = EndianUtils::readUint64InLittleEndian(ptr());
#    endif
#else
    auto p = (uint64_t*)ptr();
    value  = *p;
#endif

    m_position += sizeof(uint64_t);
    return true;
}

// 以网络字节序写入无符号 64 位整数。
void ByteBuffer::writeUint64(const uint64_t& value)
{
    WRITE_CHECK_AND_RESIZE(sizeof(uint64_t));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    EndianUtils::writeUint64InBigEndian(ptr(), value);
#    else
    EndianUtils::writeUint64InLittleEndian(ptr(), value);
#    endif
#else
    (*(uint64_t*)ptr()) = value;
#endif

    m_position += sizeof(uint64_t);
}

// 读取 uint32 长度前缀及紧随其后的字符串字节。
bool ByteBuffer::getString(std::string& value)
{
    value.clear();

    uint32_t length = 0;
    if (!getUint32(length))
    {
        return false;
    }

    if (length > 0)
    {
        READ_CHECK_RETURN_FALSE(length);
        value.append((char*)ptr(), length);
        m_position += length;
    }
    else
    {
        value = "";
    }
    return true;
}

// 写入 std::string 的长度前缀字节。
void ByteBuffer::writeString(const std::string& value)
{
    MG_ASSERT(isStringLengthValid(value.size()));
    writeBinary((byte*)value.c_str(), static_cast<uint32_t>(value.size()));
}

// 写入不要求空终止的字符串视图字节。
void ByteBuffer::writeString(const std::string_view value)
{
    MG_ASSERT(isStringLengthValid(value.size()));
    writeBinary((byte*)value.data(), static_cast<uint32_t>(value.size()));
}

// 写入 uint32 长度前缀及原始二进制数据。
void ByteBuffer::writeBinary(byte* data, uint32_t length)
{
    WRITE_CHECK_AND_RESIZE(length + 4);
    writeUint32(length);

    if (length > 0)
        ::memcpy(ptr(), data, length);

    m_position += length;
}

NS_MG_END
