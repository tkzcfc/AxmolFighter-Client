
#include "ByteBuffer.h"
#include "../Object.h"
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

ByteBuffer::ByteBuffer() : m_buffer(nullptr), m_capacity(0), m_size(0), m_position(0U)
{
    resize(0);
}

ByteBuffer::ByteBuffer(uint32_t capacity)
{
    m_capacity = capacity;
    m_buffer   = static_cast<byte*>(malloc(m_capacity));
    m_position = 0U;
    m_size     = 0U;
}

ByteBuffer::ByteBuffer(uint8_t* buffer, uint32_t capacity)
{
    m_size     = capacity;
    m_capacity = capacity;
    m_position = 0U;

    if (m_capacity > 0)
    {
        m_buffer = static_cast<byte*>(malloc(m_capacity));
        ::memcpy(m_buffer, buffer, m_capacity);
    }
}

ByteBuffer::ByteBuffer(ByteBuffer&& other) noexcept
    : m_buffer(other.m_buffer), m_capacity(other.m_capacity), m_size(other.m_size), m_position(other.m_position)
{
    other.m_buffer   = nullptr;
    other.m_capacity = 0;
    other.m_size     = 0;
    other.m_position = 0U;
}

ByteBuffer::~ByteBuffer()
{
    if (m_buffer)
    {
        free(m_buffer);
        m_buffer = nullptr;
    }
}

void ByteBuffer::resetPosition(uint32_t pos)
{
    MG_ASSERT(pos <= m_capacity);
    m_position = pos;
}

void ByteBuffer::readFinish()
{
    resetPosition(0U);
}

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

void ByteBuffer::clear()
{
    m_position = 0U;
    m_size     = 0U;
}

void ByteBuffer::fastSet(uint8_t* data, uint32_t size)
{
    if (m_buffer)
        free(m_buffer);
    m_buffer   = data;
    m_size     = size;
    m_capacity = size;
    m_position = 0U;
}

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

        ::memcpy(m_buffer, oldBuf, m_position);

        free(oldBuf);
    }
}

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

void ByteBuffer::writeBool(const bool& value)
{
    writeByte(value ? 1 : 0);
}

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

void ByteBuffer::writeInt8(const int8_t& value)
{
    writeByte((byte)value);
}

bool ByteBuffer::getByte(byte& value)
{
    READ_CHECK_RETURN_FALSE(sizeof(byte));

    auto p = (byte*)ptr();
    value  = *p;

    m_position += sizeof(byte);
    return true;
}

void ByteBuffer::writeByte(const byte& value)
{
    WRITE_CHECK_AND_RESIZE(sizeof(byte));

    (*(byte*)ptr()) = value;
    m_position += sizeof(byte);
}

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

void ByteBuffer::writeInt16(const int16_t& value)
{
    writeUint16((uint16_t)value);
}

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

void ByteBuffer::writeInt32(const int32_t& value)
{
    writeUint32((uint32_t)value);
}

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

void ByteBuffer::writeInt64(const int64_t& value)
{
    writeUint64((uint64_t)value);
}

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

bool ByteBuffer::getFloat32(float& value)
{
    READ_CHECK_RETURN_FALSE(sizeof(float));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    value = EndianUtils::readFloatInBigEndian(ptr());
#    else
    value = EndianUtils::readFloatInLittleEndian(ptr());
#    endif
#else
    auto p = (float*)ptr();
    value  = *p;
#endif

    m_position += sizeof(float);
    return true;
}

void ByteBuffer::writeFloat32(const float& value)
{
    WRITE_CHECK_AND_RESIZE(sizeof(float));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    EndianUtils::writeFloatInBigEndian(ptr(), value);
#    else
    EndianUtils::writeFloatInLittleEndian(ptr(), value);
#    endif
#else
    (*(float*)ptr()) = value;
#endif

    m_position += sizeof(float);
}

bool ByteBuffer::getFloat64(double& value)
{
    READ_CHECK_RETURN_FALSE(sizeof(double));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    value = EndianUtils::readDoubleInBigEndian(ptr());
#    else
    value = EndianUtils::readDoubleInLittleEndian(ptr());
#    endif
#else
    auto p = (double*)ptr();
    value  = *p;
#endif

    m_position += sizeof(double);
    return true;
}

void ByteBuffer::writeFloat64(const double& value)
{
    WRITE_CHECK_AND_RESIZE(sizeof(double));

#if USE_ENDIAN
#    if MG_USE_BIG_ENDIAN
    EndianUtils::writeDoubleInBigEndian(ptr(), value);
#    else
    EndianUtils::writeDoubleInLittleEndian(ptr(), value);
#    endif
#else
    (*(double*)ptr()) = value;
#endif

    m_position += sizeof(double);
}

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

void ByteBuffer::writeString(const std::string& value)
{
    writeBinary((byte*)value.c_str(), value.size());
}

void ByteBuffer::writeString(const std::string_view value)
{
    writeBinary((byte*)value.data(), value.size());
}

bool ByteBuffer::getObject(Object& value)
{
    return value.deserialize(*this);
}

void ByteBuffer::writeObject(const Object& value)
{
    value.serialize(*this);
}

void ByteBuffer::writeBinary(byte* data, uint32_t length)
{
    WRITE_CHECK_AND_RESIZE(length + 4);
    writeUint32(length);

    if (length > 0)
        ::memcpy(ptr(), data, length);

    m_position += length;
}

bool ByteBuffer::readBool()
{
    bool value = false;
    getBool(value);
    return value;
}

int8_t ByteBuffer::readInt8()
{
    int8_t value = 0;
    getInt8(value);
    return value;
}

byte ByteBuffer::readByte()
{
    byte value = 0;
    getByte(value);
    return value;
}

int16_t ByteBuffer::readInt16()
{
    int16_t value = 0;
    getInt16(value);
    return value;
}

uint16_t ByteBuffer::readUint16()
{
    uint16_t value = 0;
    getUint16(value);
    return value;
}

int32_t ByteBuffer::readInt32()
{
    int32_t value = 0;
    getInt32(value);
    return value;
}

uint32_t ByteBuffer::readUint32()
{
    uint32_t value = 0;
    getUint32(value);
    return value;
}

int64_t ByteBuffer::readInt64()
{
    int64_t value = 0;
    getInt64(value);
    return value;
}

uint64_t ByteBuffer::readUint64()
{
    uint64_t value = 0;
    getUint64(value);
    return value;
}

float ByteBuffer::readFloat32()
{
    float value = 0.0f;
    getFloat32(value);
    return value;
}

double ByteBuffer::readFloat64()
{
    double value = 0.0;
    getFloat64(value);
    return value;
}

std::string ByteBuffer::readString()
{
    std::string value = "";
    getString(value);
    return value;
}

NS_MG_END
