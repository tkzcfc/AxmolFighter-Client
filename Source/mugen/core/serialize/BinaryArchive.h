#pragma once

#include "../reflect/Reflect.h"
#include "ByteBuffer.h"

#include <limits>

NS_MG_BEGIN

// 按反射字段声明顺序写入确定性二进制归档。
class BinaryWriter
{
public:
    // 绑定承载归档字节的可写缓冲区。
    explicit BinaryWriter(ByteBuffer& buffer) : m_buffer(buffer) {}

    template <class T>
    // 写入受支持的反射值。
    bool write(const T& value)
    {
        static_assert(reflect::isSupported<T>, "Reflected field type is not supported");
        writeValue(value);
        return true;
    }

    template <class T>
    // 作为反射访问器写入字段值，字段名不进入载荷。
    void field(std::string_view, const T& value)
    {
        write(value);
    }

    // 固化当前写入位置为有效载荷长度。
    void finish() { m_buffer.writeFinish(); }

private:
    template <class T>
    // 按类型递归编码标量、容器及反射结构。
    void writeValue(const T& value)
    {
        using Value = reflect::Bare<T>;
        if constexpr (std::is_enum_v<Value>)
        {
            writeValue(static_cast<std::underlying_type_t<Value>>(value));
        }
        else if constexpr (std::is_same_v<Value, std::monostate>)
            return;
        else if constexpr (std::is_same_v<Value, bool>)
            m_buffer.writeBool(value);
        else if constexpr (std::is_same_v<Value, int8_t>)
            m_buffer.writeInt8(value);
        else if constexpr (std::is_same_v<Value, uint8_t>)
            m_buffer.writeByte(value);
        else if constexpr (std::is_same_v<Value, int16_t>)
            m_buffer.writeInt16(value);
        else if constexpr (std::is_same_v<Value, uint16_t>)
            m_buffer.writeUint16(value);
        else if constexpr (std::is_same_v<Value, int32_t>)
            m_buffer.writeInt32(value);
        else if constexpr (std::is_same_v<Value, uint32_t>)
            m_buffer.writeUint32(value);
        else if constexpr (std::is_same_v<Value, int64_t>)
            m_buffer.writeInt64(value);
        else if constexpr (std::is_same_v<Value, uint64_t>)
            m_buffer.writeUint64(value);
        else if constexpr (std::is_same_v<Value, Fixed>)
            m_buffer.writeInt64(value.raw);
        else if constexpr (std::is_same_v<Value, FixedVec2>)
        {
            writeValue(value.x);
            writeValue(value.y);
        }
        else if constexpr (std::is_same_v<Value, FixedVec3>)
        {
            writeValue(value.x);
            writeValue(value.y);
            writeValue(value.z);
        }
        else if constexpr (std::is_same_v<Value, IntVec2>)
        {
            writeValue(value.x);
            writeValue(value.y);
        }
        else if constexpr (std::is_same_v<Value, IntVec3>)
        {
            writeValue(value.x);
            writeValue(value.y);
            writeValue(value.z);
        }
        else if constexpr (std::is_same_v<Value, std::string>)
            m_buffer.writeString(value);
        else if constexpr (reflect::IsVector<Value>::value)
        {
            MG_ASSERT(value.size() <= std::numeric_limits<uint32_t>::max());
            m_buffer.writeUint32(static_cast<uint32_t>(value.size()));
            for (const auto& item : value)
                writeValue(item);
        }
        else if constexpr (reflect::IsArray<Value>::value)
        {
            m_buffer.writeUint32(static_cast<uint32_t>(value.size()));
            for (const auto& item : value)
                writeValue(item);
        }
        else if constexpr (reflect::IsOptional<Value>::value)
        {
            m_buffer.writeByte(value.has_value() ? uint8_t{1} : uint8_t{0});
            if (value)
                writeValue(*value);
        }
        else if constexpr (reflect::IsVariant<Value>::value)
        {
            MG_ASSERT(value.index() <= std::numeric_limits<uint8_t>::max());
            m_buffer.writeByte(static_cast<uint8_t>(value.index()));
            std::visit([this](const auto& item) { this->writeValue(item); }, value);
        }
        else if constexpr (reflect::IsMap<Value>::value)
        {
            MG_ASSERT(value.size() <= std::numeric_limits<uint32_t>::max());
            m_buffer.writeUint32(static_cast<uint32_t>(value.size()));
            for (const auto& [key, item] : value)
            {
                writeValue(key);
                writeValue(item);
            }
        }
        else if constexpr (reflect::isReflected<Value>)
            Value::mgReflect(*this, value);
    }

    // 归档输出目标。
    ByteBuffer& m_buffer;
};

// 按反射字段声明顺序读取确定性二进制归档。
class BinaryReader
{
public:
    // 绑定提供归档字节的读缓冲区。
    explicit BinaryReader(ByteBuffer& buffer) : m_buffer(buffer) {}

    template <class T>
    // 读取受支持的反射值；首次失败后保持失败状态。
    bool read(T& value)
    {
        static_assert(reflect::isSupported<T>, "Reflected field type is not supported");
        if (!m_ok)
            return false;
        m_ok = readValue(value);
        return m_ok;
    }

    template <class T>
    // 作为反射访问器读取字段值，字段名不参与匹配。
    void field(std::string_view, T& value)
    {
        read(value);
    }

    // 返回累计读取状态。
    bool ok() const { return m_ok; }

private:
    // 返回尚未消费的载荷字节数。
    uint32_t remaining() const { return m_buffer.getSize() - m_buffer.getPosition(); }

    template <std::size_t Index = 0, class... T>
    // 递归按标签构造对应的 variant 替代项。
    bool readVariant(uint8_t index, std::variant<T...>& value)
    {
        if constexpr (Index == sizeof...(T))
            return false;
        else if (index == Index)
        {
            using Item = std::variant_alternative_t<Index, std::variant<T...>>;
            Item item{};
            if (!readValue(item))
                return false;
            value = std::move(item);
            return true;
        }
        else
            return readVariant<Index + 1>(index, value);
    }

    template <class T>
    // 按类型递归解码标量、容器及反射结构。
    bool readValue(T& value)
    {
        using Value = reflect::Bare<T>;
        if constexpr (std::is_enum_v<Value>)
        {
            std::underlying_type_t<Value> raw{};
            if (!readValue(raw))
                return false;
            value = static_cast<Value>(raw);
            return true;
        }
        else if constexpr (std::is_same_v<Value, std::monostate>)
            return true;
        else if constexpr (std::is_same_v<Value, bool>)
            return m_buffer.getBool(value);
        else if constexpr (std::is_same_v<Value, int8_t>)
            return m_buffer.getInt8(value);
        else if constexpr (std::is_same_v<Value, uint8_t>)
            return m_buffer.getByte(value);
        else if constexpr (std::is_same_v<Value, int16_t>)
            return m_buffer.getInt16(value);
        else if constexpr (std::is_same_v<Value, uint16_t>)
            return m_buffer.getUint16(value);
        else if constexpr (std::is_same_v<Value, int32_t>)
            return m_buffer.getInt32(value);
        else if constexpr (std::is_same_v<Value, uint32_t>)
            return m_buffer.getUint32(value);
        else if constexpr (std::is_same_v<Value, int64_t>)
            return m_buffer.getInt64(value);
        else if constexpr (std::is_same_v<Value, uint64_t>)
            return m_buffer.getUint64(value);
        else if constexpr (std::is_same_v<Value, Fixed>)
            return m_buffer.getInt64(value.raw);
        else if constexpr (std::is_same_v<Value, FixedVec2>)
            return readValue(value.x) && readValue(value.y);
        else if constexpr (std::is_same_v<Value, FixedVec3>)
            return readValue(value.x) && readValue(value.y) && readValue(value.z);
        else if constexpr (std::is_same_v<Value, IntVec2>)
            return readValue(value.x) && readValue(value.y);
        else if constexpr (std::is_same_v<Value, IntVec3>)
            return readValue(value.x) && readValue(value.y) && readValue(value.z);
        else if constexpr (std::is_same_v<Value, std::string>)
            return m_buffer.getString(value);
        else if constexpr (reflect::IsVector<Value>::value)
        {
            uint32_t count = 0;
            if (!m_buffer.getUint32(count) || count > remaining())
                return false;
            Value result;
            result.resize(count);
            for (auto& item : result)
                if (!readValue(item))
                    return false;
            value = std::move(result);
            return true;
        }
        else if constexpr (reflect::IsArray<Value>::value)
        {
            uint32_t count = 0;
            if (!m_buffer.getUint32(count) || count != value.size())
                return false;
            for (auto& item : value)
                if (!readValue(item))
                    return false;
            return true;
        }
        else if constexpr (reflect::IsOptional<Value>::value)
        {
            uint8_t present = 0;
            if (!m_buffer.getByte(present) || present > 1)
                return false;
            if (present == 0)
            {
                value.reset();
                return true;
            }
            typename Value::value_type item{};
            if (!readValue(item))
                return false;
            value = std::move(item);
            return true;
        }
        else if constexpr (reflect::IsVariant<Value>::value)
        {
            uint8_t index = 0;
            return m_buffer.getByte(index) && readVariant(index, value);
        }
        else if constexpr (reflect::IsMap<Value>::value)
        {
            uint32_t count = 0;
            if (!m_buffer.getUint32(count) || count > remaining())
                return false;
            Value result;
            for (uint32_t index = 0; index < count; ++index)
            {
                typename Value::key_type key{};
                typename Value::mapped_type item{};
                if (!readValue(key) || !readValue(item) || !result.emplace(std::move(key), std::move(item)).second)
                    return false;
            }
            value = std::move(result);
            return true;
        }
        else if constexpr (reflect::isReflected<Value>)
        {
            Value::mgReflect(*this, value);
            return m_ok;
        }
    }

    // 归档输入来源。
    ByteBuffer& m_buffer;
    // 任一字段失败后保持的累计读取结果。
    bool m_ok = true;
};

NS_MG_END
