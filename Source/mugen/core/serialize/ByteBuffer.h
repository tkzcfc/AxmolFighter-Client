#pragma once

#include "../StdC.h"

NS_MG_BEGIN

class Object;

// Helper to trigger static assert for unsupported types
template <typename T>
struct always_false : std::false_type
{};

// Object
template <typename T>
constexpr bool is_object_v = std::is_base_of<Object, T>::value;

// std::array
template <typename T>
struct is_std_array : std::false_type
{};
template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type
{};
template <typename T>
inline constexpr bool is_std_array_v = is_std_array<T>::value;

// std::vector
template <typename T>
struct is_vector : std::false_type
{};
template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type
{};
template <typename T>
constexpr bool is_vector_v = is_vector<T>::value;

// std::map
template <typename T>
struct is_map : std::false_type
{};
template <typename K, typename V, typename Cmp, typename Alloc>
struct is_map<std::map<K, V, Cmp, Alloc>> : std::true_type
{};
template <typename T>
constexpr bool is_map_v = is_map<T>::value;
// unordered_map
template <typename T>
struct is_unordered_map : std::false_type
{};
template <typename K, typename V, typename Hash, typename KeyEqual, typename Alloc>
struct is_unordered_map<std::unordered_map<K, V, Hash, KeyEqual, Alloc>> : std::true_type
{};
template <typename T>
constexpr bool is_unordered_map_v = is_unordered_map<T>::value;

// set
template <typename T>
struct is_set : std::false_type
{};
template <typename K, typename Cmp, typename Alloc>
struct is_set<std::set<K, Cmp, Alloc>> : std::true_type
{};
template <typename T>
constexpr bool is_set_v = is_set<T>::value;

// unordered_set
template <typename T>
struct is_unordered_set : std::false_type
{};
template <typename K, typename Hash, typename KeyEqual, typename Alloc>
struct is_unordered_set<std::unordered_set<K, Hash, KeyEqual, Alloc>> : std::true_type
{};
template <typename T>
constexpr bool is_unordered_set_v = is_unordered_set<T>::value;

class ByteBuffer
{
public:
    ByteBuffer();
    ByteBuffer(uint32_t capacity);
    ByteBuffer(uint8_t* buffer, uint32_t capacity);
    ByteBuffer(ByteBuffer&& other) noexcept;

    virtual ~ByteBuffer();

    void resetPosition(uint32_t pos = 0U);
    void readFinish();
    void writeFinish();
    uint8_t* data();
    uint32_t len();
    void clear();

    /**
     * 直接接管外部 malloc 分配的内存指针，无拷贝。
     * 原有缓冲区会被 free。
     * @param data  必须是 malloc 分配的指针，生命周期交由 ByteBuffer 接管
     * @param size  有效数据字节数
     */
    void fastSet(uint8_t* data, uint32_t size);

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

    bool getFloat32(float& value);
    void writeFloat32(const float& value);

    bool getFloat64(double& value);
    void writeFloat64(const double& value);

    bool getString(std::string& value);
    void writeString(const std::string& value);
    void writeString(const std::string_view value);

    bool getObject(Object& value);
    void writeObject(const Object& value);

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
    float readFloat32();
    double readFloat64();
    std::string readString();

public:
    template <typename T>
    bool getValue(T& value)
    {
        if constexpr (is_object_v<T>)
        {
            return getObject(value);
        }
        else if constexpr (is_std_array_v<T>)
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

            // 检查剩余可读数据是否足够
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

            // 检查剩余可读数据是否足够
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
        else if constexpr (is_unordered_map_v<T>)
        {
            using KeyType    = typename T::key_type;
            using MappedType = typename T::mapped_type;
            uint32_t size    = 0;
            if (!getUint32(size))
                return false;

            // 检查剩余可读数据是否足够
            if (getReadLimit() < size)
            {
                return false;
            }

            value.clear();
            value.reserve(static_cast<size_t>(size * 1.5f));
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

            // 检查剩余可读数据是否足够
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
        else if constexpr (is_unordered_set_v<T>)
        {
            using KeyType = typename T::key_type;
            uint32_t size = 0;
            if (!getUint32(size))
                return false;

            // 检查剩余可读数据是否足够
            if (getReadLimit() < size)
            {
                return false;
            }

            value.clear();
            value.reserve(static_cast<size_t>(size * 1.5f));
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
        else if constexpr (std::is_same_v<T, float>)
        {
            return getFloat32(value);
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return getFloat64(value);
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
    void writeValue(const T& value)
    {
        if constexpr (is_object_v<T>)
        {
            writeObject(value);
        }
        else if constexpr (is_std_array_v<T>)
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
        else if constexpr (is_map_v<T> || is_unordered_map_v<T>)
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
        else if constexpr (is_unordered_set_v<T>)
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
        else if constexpr (std::is_same_v<T, float>)
        {
            writeFloat32(value);
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            writeFloat64(value);
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
        else if constexpr (std::is_same_v<T, float>)
        {
            return readFloat32();
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return readFloat64();
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
