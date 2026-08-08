#include "EndianUtils.h"

NS_MG_BEGIN

/* Big-Endian */

uint16_t EndianUtils::readUint16InBigEndian(void* memory)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    return (((uint16_t)p[0]) << 8) | (((uint16_t)p[1]));
}

uint32_t EndianUtils::readUint32InBigEndian(void* memory)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    return (((uint32_t)p[0]) << 24) | (((uint32_t)p[1]) << 16) | (((uint32_t)p[2]) << 8) | (((uint32_t)p[3]));
}

uint64_t EndianUtils::readUint64InBigEndian(void* memory)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    return (((uint64_t)p[0]) << 56) | (((uint64_t)p[1]) << 48) | (((uint64_t)p[2]) << 40) | (((uint64_t)p[3]) << 32) |
           (((uint64_t)p[4]) << 24) | (((uint64_t)p[5]) << 16) | (((uint64_t)p[6]) << 8) | (((uint64_t)p[7]));
}

void EndianUtils::writeUint16InBigEndian(void* memory, uint16_t value)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    p[0]       = (uint8_t)(value >> 8);
    p[1]       = (uint8_t)(value);
}

void EndianUtils::writeUint32InBigEndian(void* memory, uint32_t value)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    p[0]       = (uint8_t)(value >> 24);
    p[1]       = (uint8_t)(value >> 16);
    p[2]       = (uint8_t)(value >> 8);
    p[3]       = (uint8_t)(value);
}

void EndianUtils::writeUint64InBigEndian(void* memory, uint64_t value)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    p[0]       = (uint8_t)(value >> 56);
    p[1]       = (uint8_t)(value >> 48);
    p[2]       = (uint8_t)(value >> 40);
    p[3]       = (uint8_t)(value >> 32);
    p[4]       = (uint8_t)(value >> 24);
    p[5]       = (uint8_t)(value >> 16);
    p[6]       = (uint8_t)(value >> 8);
    p[7]       = (uint8_t)(value);
}

int16_t EndianUtils::readInt16InBigEndian(void* memory)
{
    return (int16_t)readUint16InBigEndian(memory);
}

int32_t EndianUtils::readInt32InBigEndian(void* memory)
{
    return (int32_t)readUint32InBigEndian(memory);
}

int64_t EndianUtils::readInt64InBigEndian(void* memory)
{
    return (int64_t)readUint64InBigEndian(memory);
}

float EndianUtils::readFloatInBigEndian(void* memory)
{
    union
    {
        uint32_t i;
        float f;
    } u;
    u.i = readUint32InBigEndian(memory);
    return u.f;
}

double EndianUtils::readDoubleInBigEndian(void* memory)
{
    union
    {
        uint64_t i;
        double f;
    } u;
    u.i = readUint64InBigEndian(memory);
    return u.f;
}

void EndianUtils::writeInt16InBigEndian(void* memory, int16_t value)
{
    writeUint16InBigEndian(memory, (uint16_t)value);
}

void EndianUtils::writeInt32InBigEndian(void* memory, int32_t value)
{
    writeUint32InBigEndian(memory, (uint32_t)value);
}

void EndianUtils::writeInt64InBigEndian(void* memory, int64_t value)
{
    writeUint64InBigEndian(memory, (uint64_t)value);
}

void EndianUtils::writeFloatInBigEndian(void* memory, float value)
{
    union
    {
        uint32_t i;
        float f;
    } u;
    u.f = value;
    writeUint32InBigEndian(memory, u.i);
}

void EndianUtils::writeDoubleInBigEndian(void* memory, double value)
{
    union
    {
        uint64_t i;
        double f;
    } u;
    u.f = value;
    writeUint64InBigEndian(memory, u.i);
}

/* Little-Endian */

uint16_t EndianUtils::readUint16InLittleEndian(void* memory)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    return (((uint16_t)p[1]) << 8) | (((uint16_t)p[0]));
}

uint32_t EndianUtils::readUint32InLittleEndian(void* memory)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    return (((uint32_t)p[3]) << 24) | (((uint32_t)p[2]) << 16) | (((uint32_t)p[1]) << 8) | (((uint32_t)p[0]));
}

uint64_t EndianUtils::readUint64InLittleEndian(void* memory)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    return (((uint64_t)p[7]) << 56) | (((uint64_t)p[6]) << 48) | (((uint64_t)p[5]) << 40) | (((uint64_t)p[4]) << 32) |
           (((uint64_t)p[3]) << 24) | (((uint64_t)p[2]) << 16) | (((uint64_t)p[1]) << 8) | (((uint64_t)p[0]));
}

void EndianUtils::writeUint16InLittleEndian(void* memory, uint16_t value)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    p[1]       = (uint8_t)(value >> 8);
    p[0]       = (uint8_t)(value);
}

void EndianUtils::writeUint32InLittleEndian(void* memory, uint32_t value)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    p[3]       = (uint8_t)(value >> 24);
    p[2]       = (uint8_t)(value >> 16);
    p[1]       = (uint8_t)(value >> 8);
    p[0]       = (uint8_t)(value);
}

void EndianUtils::writeUint64InLittleEndian(void* memory, uint64_t value)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(memory);
    p[7]       = (uint8_t)(value >> 56);
    p[6]       = (uint8_t)(value >> 48);
    p[5]       = (uint8_t)(value >> 40);
    p[4]       = (uint8_t)(value >> 32);
    p[3]       = (uint8_t)(value >> 24);
    p[2]       = (uint8_t)(value >> 16);
    p[1]       = (uint8_t)(value >> 8);
    p[0]       = (uint8_t)(value);
}

int16_t EndianUtils::readInt16InLittleEndian(void* memory)
{
    return (int16_t)readUint16InLittleEndian(memory);
}

int32_t EndianUtils::readInt32InLittleEndian(void* memory)
{
    return (int32_t)readUint32InLittleEndian(memory);
}

int64_t EndianUtils::readInt64InLittleEndian(void* memory)
{
    return (int64_t)readUint64InLittleEndian(memory);
}

float EndianUtils::readFloatInLittleEndian(void* memory)
{
    union
    {
        uint32_t i;
        float f;
    } u;
    u.i = readUint32InLittleEndian(memory);
    return u.f;
}

double EndianUtils::readDoubleInLittleEndian(void* memory)
{
    union
    {
        uint64_t i;
        double f;
    } u;
    u.i = readUint64InLittleEndian(memory);
    return u.f;
}

void EndianUtils::writeInt16InLittleEndian(void* memory, int16_t value)
{
    writeUint16InLittleEndian(memory, (uint16_t)value);
}

void EndianUtils::writeInt32InLittleEndian(void* memory, int32_t value)
{
    writeUint32InLittleEndian(memory, (uint32_t)value);
}

void EndianUtils::writeInt64InLittleEndian(void* memory, int64_t value)
{
    writeUint64InLittleEndian(memory, (uint64_t)value);
}

void EndianUtils::writeFloatInLittleEndian(void* memory, float value)
{
    union
    {
        uint32_t i;
        float f;
    } u;
    u.f = value;
    writeUint32InLittleEndian(memory, u.i);
}

void EndianUtils::writeDoubleInLittleEndian(void* memory, double value)
{
    union
    {
        uint64_t i;
        double f;
    } u;
    u.f = value;
    writeUint64InLittleEndian(memory, u.i);
}

NS_MG_END
