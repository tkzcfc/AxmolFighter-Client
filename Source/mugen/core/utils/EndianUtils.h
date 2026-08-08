#pragma once

#include "../StdC.h"

NS_MG_BEGIN

class EndianUtils
{
public:
    /* Big-Endian */
    static uint16_t readUint16InBigEndian(void* memory);
    static uint32_t readUint32InBigEndian(void* memory);
    static uint64_t readUint64InBigEndian(void* memory);

    static void writeUint16InBigEndian(void* memory, uint16_t value);
    static void writeUint32InBigEndian(void* memory, uint32_t value);
    static void writeUint64InBigEndian(void* memory, uint64_t value);

    static int16_t readInt16InBigEndian(void* memory);
    static int32_t readInt32InBigEndian(void* memory);
    static int64_t readInt64InBigEndian(void* memory);
    static float readFloatInBigEndian(void* memory);
    static double readDoubleInBigEndian(void* memory);

    static void writeInt16InBigEndian(void* memory, int16_t value);
    static void writeInt32InBigEndian(void* memory, int32_t value);
    static void writeInt64InBigEndian(void* memory, int64_t value);
    static void writeFloatInBigEndian(void* memory, float value);
    static void writeDoubleInBigEndian(void* memory, double value);

    /* Little-Endian */
    static uint16_t readUint16InLittleEndian(void* memory);
    static uint32_t readUint32InLittleEndian(void* memory);
    static uint64_t readUint64InLittleEndian(void* memory);

    static void writeUint16InLittleEndian(void* memory, uint16_t value);
    static void writeUint32InLittleEndian(void* memory, uint32_t value);
    static void writeUint64InLittleEndian(void* memory, uint64_t value);

    static int16_t readInt16InLittleEndian(void* memory);
    static int32_t readInt32InLittleEndian(void* memory);
    static int64_t readInt64InLittleEndian(void* memory);
    static float readFloatInLittleEndian(void* memory);
    static double readDoubleInLittleEndian(void* memory);

    static void writeInt16InLittleEndian(void* memory, int16_t value);
    static void writeInt32InLittleEndian(void* memory, int32_t value);
    static void writeInt64InLittleEndian(void* memory, int64_t value);
    static void writeFloatInLittleEndian(void* memory, float value);
    static void writeDoubleInLittleEndian(void* memory, double value);
};

NS_MG_END
