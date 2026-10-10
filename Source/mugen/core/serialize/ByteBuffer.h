#pragma once

#include "../StdC.h"

NS_MG_BEGIN

// 用于在模板分支中触发不支持类型的静态断言。
template <typename T>
struct always_false : std::false_type
{};

// 检测 std::array。
template <typename T>
struct is_std_array : std::false_type
{};
template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type
{};
template <typename T>
inline constexpr bool is_std_array_v = is_std_array<T>::value;

// 检测 std::vector。
template <typename T>
struct is_vector : std::false_type
{};
template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type
{};
template <typename T>
constexpr bool is_vector_v = is_vector<T>::value;

// 检测 std::map。
template <typename T>
struct is_map : std::false_type
{};
template <typename K, typename V, typename Cmp, typename Alloc>
struct is_map<std::map<K, V, Cmp, Alloc>> : std::true_type
{};
template <typename T>
constexpr bool is_map_v = is_map<T>::value;
// 检测 std::set。
template <typename T>
struct is_set : std::false_type
{};
template <typename K, typename Cmp, typename Alloc>
struct is_set<std::set<K, Cmp, Alloc>> : std::true_type
{};
template <typename T>
constexpr bool is_set_v = is_set<T>::value;

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

    bool readBool();
    int8_t readInt8();
    byte readByte();
    int16_t readInt16();
    uint16_t readUint16();
    int32_t readInt32();
    uint32_t readUint32();
    int64_t readInt64();
    uint64_t readUint64();
    std::string readString();

public:
    template <typename T>
    // 按类型递归读取旧式基础容器和值类型。
    bool getValue(T& value)
    {
        if constexpr (is_std_array_v<T>)
        {
            constexpr size_t N = std::tuple_size_v<T>;
            for (size_t i = 0; i < N; ++i)
            {
                if (!getValue(value[i]))
                    return false;
            }
            return true;
        }
        else if constexpr (is_vector_v<T>)
        {
            uint32_t size = 0;
            if (!getUint32(size))
                return false;

            // 元素至少占一个字节，先阻止明显越界的长度声明。
            if (getReadLimit() < size)
            {
                return false;
            }

            value.resize(size);
            for (uint32_t i = 0; i < size; ++i)
            {
                if (!getValue(value[i]))
                    return false;
            }
            return true;
        }
        else if constexpr (is_map_v<T>)
        {
            using KeyType    = typename T::key_type;
            using MappedType = typename T::mapped_type;
            uint32_t size    = 0;
            if (!getUint32(size))
                return false;

            // 元素至少占一个字节，先阻止明显越界的长度声明。
            if (getReadLimit() < size)
            {
                return false;
            }

            value.clear();
            for (uint32_t i = 0; i < size; ++i)
            {
                KeyType key;
                MappedType mapped;
                if (!getValue(key))
                    return false;
                if (!getValue(mapped))
                    return false;
                value.emplace(std::move(key), std::move(mapped));
            }
            return true;
        }
        else if constexpr (is_set_v<T>)
        {
            using KeyType = typename T::key_type;
            uint32_t size = 0;
            if (!getUint32(size))
                return false;

            // 元素至少占一个字节，先阻止明显越界的长度声明。
            if (getReadLimit() < size)
            {
                return false;
            }

            value.clear();
            for (uint32_t i = 0; i < size; ++i)
            {
                KeyType key;
                if (!getValue(key))
                    return false;
                value.emplace(std::move(key));
            }
            return true;
        }
        else if constexpr (std::is_enum_v<T>)
        {
            using UnderlyingType = std::underlying_type_t<T>;
            if constexpr (std::is_same_v<UnderlyingType, int8_t>)
            {
                UnderlyingType temp;
                bool result = getInt8(temp);
                value       = static_cast<T>(temp);
                return result;
            }
            else if constexpr (std::is_same_v<UnderlyingType, uint8_t>)
            {
                UnderlyingType temp;
                bool result = getByte(temp);
                value       = static_cast<T>(temp);
                return result;
            }
            else if constexpr (std::is_same_v<UnderlyingType, int16_t>)
            {
                UnderlyingType temp;
                bool result = getInt16(temp);
                value       = static_cast<T>(temp);
                return result;
            }
            else if constexpr (std::is_same_v<UnderlyingType, uint16_t>)
            {
                UnderlyingType temp;
                bool result = getUint16(temp);
                value       = static_cast<T>(temp);
                return result;
            }
            else if constexpr (std::is_same_v<UnderlyingType, int32_t>)
            {
                UnderlyingType temp;
                bool result = getInt32(temp);
                value       = static_cast<T>(temp);
                return result;
            }
            else if constexpr (std::is_same_v<UnderlyingType, uint32_t>)
            {
                UnderlyingType temp;
                bool result = getUint32(temp);
                value       = static_cast<T>(temp);
                return result;
            }
            else if constexpr (std::is_same_v<UnderlyingType, int64_t>)
            {
                UnderlyingType temp;
                bool result = getInt64(temp);
                value       = static_cast<T>(temp);
                return result;
            }
            else if constexpr (std::is_same_v<UnderlyingType, uint64_t>)
            {
                UnderlyingType temp;
                bool result = getUint64(temp);
                value       = static_cast<T>(temp);
                return result;
            }
            else
            {
                static_assert(always_false<T>::value, "Unsupported enum underlying type for getValue");
                return false;
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            return getBool(value);
        }
        else if constexpr (std::is_same_v<T, int8_t>)
        {
            return getInt8(value);
        }
        else if constexpr (std::is_same_v<T, uint8_t>)
        {
            return getByte(value);
        }
        else if constexpr (std::is_same_v<T, int16_t>)
        {
            return getInt16(value);
        }
        else if constexpr (std::is_same_v<T, uint16_t>)
        {
            return getUint16(value);
        }
        else if constexpr (std::is_same_v<T, int32_t>)
        {
            return getInt32(value);
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            return getUint32(value);
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            return getInt64(value);
        }
        else if constexpr (std::is_same_v<T, uint64_t>)
        {
            return getUint64(value);
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            return getString(value);
        }
        else
        {
            static_assert(always_false<T>::value, "Unsupported enum underlying type for getValue");
            return false;
        }
    }

    template <typename T>
    // 按类型递归写入旧式基础容器和值类型。
    void writeValue(const T& value)
    {
        if constexpr (is_std_array_v<T>)
        {
            constexpr size_t N = std::tuple_size_v<T>;
            for (size_t i = 0; i < N; ++i)
            {
                writeValue(value[i]);
            }
        }
        else if constexpr (is_vector_v<T>)
        {
            writeUint32(static_cast<uint32_t>(value.size()));
            for (const auto& elem : value)
            {
                writeValue(elem);
            }
        }
        else if constexpr (is_map_v<T>)
        {
            writeUint32(static_cast<uint32_t>(value.size()));
            for (const auto& kv : value)
            {
                writeValue(kv.first);
                writeValue(kv.second);
            }
        }
        else if constexpr (is_set_v<T>)
        {
            writeUint32(static_cast<uint32_t>(value.size()));
            for (const auto& key : value)
            {
                writeValue(key);
            }
        }
        else if constexpr (std::is_enum_v<T>)
        {
            using UnderlyingType = std::underlying_type_t<T>;
            if constexpr (std::is_same_v<UnderlyingType, int8_t>)
            {
                writeInt8(static_cast<int8_t>(value));
            }
            else if constexpr (std::is_same_v<UnderlyingType, uint8_t>)
            {
                writeByte(static_cast<uint8_t>(value));
            }
            else if constexpr (std::is_same_v<UnderlyingType, int16_t>)
            {
                writeInt16(static_cast<int16_t>(value));
            }
            else if constexpr (std::is_same_v<UnderlyingType, uint16_t>)
            {
                writeUint16(static_cast<uint16_t>(value));
            }
            else if constexpr (std::is_same_v<UnderlyingType, int32_t>)
            {
                writeInt32(static_cast<int32_t>(value));
            }
            else if constexpr (std::is_same_v<UnderlyingType, uint32_t>)
            {
                writeUint32(static_cast<uint32_t>(value));
            }
            else if constexpr (std::is_same_v<UnderlyingType, int64_t>)
            {
                writeInt64(static_cast<int64_t>(value));
            }
            else if constexpr (std::is_same_v<UnderlyingType, uint64_t>)
            {
                writeUint64(static_cast<uint64_t>(value));
            }
            else
            {
                static_assert(always_false<T>::value, "Unsupported enum underlying type for writeValue");
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            writeBool(value);
        }
        else if constexpr (std::is_same_v<T, int8_t>)
        {
            writeInt8(value);
        }
        else if constexpr (std::is_same_v<T, uint8_t>)
        {
            writeByte(value);
        }
        else if constexpr (std::is_same_v<T, int16_t>)
        {
            writeInt16(value);
        }
        else if constexpr (std::is_same_v<T, uint16_t>)
        {
            writeUint16(value);
        }
        else if constexpr (std::is_same_v<T, int32_t>)
        {
            writeInt32(value);
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            writeUint32(value);
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            writeInt64(value);
        }
        else if constexpr (std::is_same_v<T, uint64_t>)
        {
            writeUint64(value);
        }
        else if constexpr (std::is_same_v<T, std::string_view>)
        {
            writeString(value);
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            writeString(value);
        }
        else
        {
            static_assert(always_false<T>::value, "Unsupported type for writeValue");
        }
    }

    template <typename T>
    // 读取基础类型，失败时使用各基础读取函数的默认返回值。
    T readValue()
    {
        if constexpr (std::is_same_v<T, bool>)
        {
            return readBool();
        }
        else if constexpr (std::is_same_v<T, int8_t>)
        {
            return readInt8();
        }
        else if constexpr (std::is_same_v<T, uint8_t>)
        {
            return readByte();
        }
        else if constexpr (std::is_same_v<T, int16_t>)
        {
            return readInt16();
        }
        else if constexpr (std::is_same_v<T, uint16_t>)
        {
            return readUint16();
        }
        else if constexpr (std::is_same_v<T, int32_t>)
        {
            return readInt32();
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            return readUint32();
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            return readInt64();
        }
        else if constexpr (std::is_same_v<T, uint64_t>)
        {
            return readUint64();
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            return readString();
        }
        else
        {
            static_assert(always_false<T>::value, "Unsupported type");
        }
    }

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
