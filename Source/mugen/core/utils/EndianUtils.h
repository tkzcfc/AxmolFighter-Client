#pragma once

#include "../StdC.h"

NS_MG_BEGIN

// 在原始内存与显式大小端数值之间转换的工具集合。
class EndianUtils
{
public:
    // 大端字节序读写函数；调用方保证内存至少容纳目标类型。
    static uint16_t readUint16InBigEndian(void* memory);
    static uint32_t readUint32InBigEndian(void* memory);
    static uint64_t readUint64InBigEndian(void* memory);

    static void writeUint16InBigEndian(void* memory, uint16_t value);
    static void writeUint32InBigEndian(void* memory, uint32_t value);
    static void writeUint64InBigEndian(void* memory, uint64_t value);

    static int16_t readInt16InBigEndian(void* memory);
    static int32_t readInt32InBigEndian(void* memory);
    static int64_t readInt64InBigEndian(void* memory);

    static void writeInt16InBigEndian(void* memory, int16_t value);
    static void writeInt32InBigEndian(void* memory, int32_t value);
    static void writeInt64InBigEndian(void* memory, int64_t value);

    // 小端字节序读写函数；调用方保证内存至少容纳目标类型。
    static uint16_t readUint16InLittleEndian(void* memory);
    static uint32_t readUint32InLittleEndian(void* memory);
    static uint64_t readUint64InLittleEndian(void* memory);

    static void writeUint16InLittleEndian(void* memory, uint16_t value);
    static void writeUint32InLittleEndian(void* memory, uint32_t value);
    static void writeUint64InLittleEndian(void* memory, uint64_t value);

    static int16_t readInt16InLittleEndian(void* memory);
    static int32_t readInt32InLittleEndian(void* memory);
    static int64_t readInt64InLittleEndian(void* memory);

    static void writeInt16InLittleEndian(void* memory, int16_t value);
    static void writeInt32InLittleEndian(void* memory, int32_t value);
    static void writeInt64InLittleEndian(void* memory, int64_t value);
};

NS_MG_END
