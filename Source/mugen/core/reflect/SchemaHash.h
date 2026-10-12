#pragma once

#include "Reflect.h"
#include "../serialize/Hash.h"

NS_MG_BEGIN

namespace reflect
{
// 将反射字段及类型结构编码为稳定字节串并计算哈希。
class SchemaHasher
{
public:
    template <class T>
    // 递归追加类型的规范化模式描述。
    void appendType()
    {
        using Value = Bare<T>;
        if constexpr (std::is_enum_v<Value>)
        {
            append("enum");
            appendType<std::underlying_type_t<Value>>();
        }
        else if constexpr (std::is_same_v<Value, std::monostate>)
            append("monostate");
        else if constexpr (std::is_same_v<Value, bool>)
            append("bool");
        else if constexpr (std::is_integral_v<Value>)
        {
            append(std::is_signed_v<Value> ? "i" : "u");
            appendNumber(sizeof(Value));
        }
        else if constexpr (std::is_same_v<Value, Fixed>)
            append("fixed");
        else if constexpr (std::is_same_v<Value, FixedVec2>)
            append("fixed_vec2");
        else if constexpr (std::is_same_v<Value, FixedVec3>)
            append("fixed_vec3");
        else if constexpr (std::is_same_v<Value, IntVec2>)
            append("int_vec2");
        else if constexpr (std::is_same_v<Value, IntVec3>)
            append("int_vec3");
        else if constexpr (std::is_same_v<Value, std::string>)
            append("string");
        else if constexpr (IsVector<Value>::value)
        {
            append("vector<");
            appendType<typename Value::value_type>();
            append(">");
        }
        else if constexpr (IsArray<Value>::value)
        {
            append("array<");
            appendType<typename Value::value_type>();
            appendNumber(std::tuple_size_v<Value>);
            append(">");
        }
        else if constexpr (IsOptional<Value>::value)
        {
            append("optional<");
            appendType<typename Value::value_type>();
            append(">");
        }
        else if constexpr (IsVariant<Value>::value)
        {
            append("variant<");
            appendVariant<Value>(std::make_index_sequence<std::variant_size_v<Value>>{});
            append(">");
        }
        else if constexpr (IsMap<Value>::value)
        {
            append("map<");
            appendType<typename Value::key_type>();
            appendType<typename Value::mapped_type>();
            append(">");
        }
        else if constexpr (isReflected<Value>)
        {
            append("struct");
            Value value{};
            Value::mgReflect(*this, value);
        }
    }

    template <class T>
    // 追加字段名及其递归类型描述。
    void field(std::string_view name, T&)
    {
        append(name);
        appendType<T>();
    }

    // 对累计模式字节计算 xxh64。
    uint64_t finish() const { return xxh64(reinterpret_cast<const uint8_t*>(m_schema.data()), m_schema.size()); }

private:
    // 按替代项声明次序追加 variant 的所有类型。
    template <class Variant, std::size_t... Index>
    void appendVariant(std::index_sequence<Index...>)
    {
        (appendType<std::variant_alternative_t<Index, Variant>>(), ...);
    }

    // 以长度前缀追加文本，避免拼接歧义。
    void append(std::string_view text)
    {
        appendNumber(text.size());
        m_schema.append(text);
    }

    // 以固定八字节小端形式追加数值。
    void appendNumber(std::size_t value)
    {
        for (uint32_t shift = 0; shift < 64; shift += 8)
            m_schema.push_back(static_cast<char>((value >> shift) & 0xff));
    }

    // 等待计算哈希的规范化模式字节。
    std::string m_schema;
};

// 返回受支持反射类型的稳定模式哈希。
template <class T>
uint64_t schemaHash()
{
    static_assert(isSupported<T>, "Schema type contains an unsupported reflected field");
    SchemaHasher hasher;
    hasher.appendType<T>();
    return hasher.finish();
}
}  // namespace reflect

NS_MG_END
