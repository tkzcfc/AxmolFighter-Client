#pragma once

#include "../math/Fixed.h"
#include "../math/FixedVec2.h"
#include "../math/FixedVec3.h"
#include "../math/IntVec2.h"
#include "../math/IntVec3.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

NS_MG_BEGIN

namespace reflect
{

// 移除类型的 cv 与引用限定，用于统一反射判断。
template <class T>
using Bare = std::remove_cv_t<std::remove_reference_t<T>>;

// 检测类型是否通过 MG_REFLECT 提供名称与字段访问协议。
template <class T, class = void>
struct IsReflected : std::false_type
{};

// 仅用于检测 mgReflect 是否可被字段访问器调用。
struct ProbeVisitor
{
    template <class T>
    void field(std::string_view, T&);
};

template <class T>
struct IsReflected<T,
                   std::void_t<decltype(Bare<T>::kMgTypeName),
                               decltype(Bare<T>::mgReflect(std::declval<ProbeVisitor&>(), std::declval<Bare<T>&>()))>>
    : std::true_type
{};

// 提供已去限定类型的反射判断结果。
template <class T>
inline constexpr bool isReflected = IsReflected<Bare<T>>::value;

// 检测标准 vector 容器。
template <class T>
struct IsVector : std::false_type
{};
template <class T, class Allocator>
struct IsVector<std::vector<T, Allocator>> : std::true_type
{};

// 检测固定长度 array 容器。
template <class T>
struct IsArray : std::false_type
{};
template <class T, std::size_t Size>
struct IsArray<std::array<T, Size>> : std::true_type
{};

// 检测可选值容器。
template <class T>
struct IsOptional : std::false_type
{};
template <class T>
struct IsOptional<std::optional<T>> : std::true_type
{};

// 检测可判别联合容器。
template <class T>
struct IsVariant : std::false_type
{};
template <class... T>
struct IsVariant<std::variant<T...>> : std::true_type
{};

// 检测按键有序的 map 容器。
template <class T>
struct IsMap : std::false_type
{};
template <class Key, class Value, class Compare, class Allocator>
struct IsMap<std::map<Key, Value, Compare, Allocator>> : std::true_type
{};

// 声明可归档字段类型的总判断。
template <class T>
struct IsSupported;

// 为容器递归判断其元素是否支持反射归档。
template <class T>
struct IsSupportedContainer : std::false_type
{};
template <class T, class Allocator>
struct IsSupportedContainer<std::vector<T, Allocator>> : std::bool_constant<IsSupported<T>::value>
{};
template <class T, std::size_t Size>
struct IsSupportedContainer<std::array<T, Size>> : std::bool_constant<IsSupported<T>::value>
{};
template <class T>
struct IsSupportedContainer<std::optional<T>> : std::bool_constant<IsSupported<T>::value>
{};
template <class... T>
struct IsSupportedContainer<std::variant<T...>> : std::bool_constant<(IsSupported<T>::value && ...)>
{};
template <class Key, class Value, class Compare, class Allocator>
struct IsSupportedContainer<std::map<Key, Value, Compare, Allocator>>
    : std::bool_constant<IsSupported<Key>::value && IsSupported<Value>::value>
{};

// 限制反射系统可处理的标量、结构体和容器类型集合。
template <class T>
struct IsSupported
    : std::bool_constant<
          std::is_same_v<Bare<T>, bool> || (std::is_integral_v<Bare<T>> && sizeof(Bare<T>) <= sizeof(uint64_t)) ||
          std::is_enum_v<Bare<T>> || std::is_same_v<Bare<T>, Fixed> || std::is_same_v<Bare<T>, FixedVec2> ||
          std::is_same_v<Bare<T>, FixedVec3> || std::is_same_v<Bare<T>, IntVec2> || std::is_same_v<Bare<T>, IntVec3> ||
          std::is_same_v<Bare<T>, std::string> || std::is_same_v<Bare<T>, std::monostate> || isReflected<Bare<T>> ||
          IsSupportedContainer<Bare<T>>::value>
{};

// 提供已去限定类型的支持判断结果。
template <class T>
inline constexpr bool isSupported = IsSupported<Bare<T>>::value;

}  // namespace reflect

NS_MG_END

// 将字段名和类型名转换为稳定文本，供访问器和模式哈希使用。
#define MG_REFLECT_STRINGIZE_INNER(value)       #value
#define MG_REFLECT_STRINGIZE(value)             MG_REFLECT_STRINGIZE_INNER(value)
#define MG_REFLECT_FIELD(visitor, self, member) (visitor).field(MG_REFLECT_STRINGIZE(member), (self).member);

#define MG_REFLECT_PARENS                       ()
#define MG_REFLECT_EXPAND1(...)                 __VA_ARGS__
#define MG_REFLECT_EXPAND2(...)                 MG_REFLECT_EXPAND1(MG_REFLECT_EXPAND1(MG_REFLECT_EXPAND1(__VA_ARGS__)))
#define MG_REFLECT_EXPAND3(...)                 MG_REFLECT_EXPAND2(MG_REFLECT_EXPAND2(MG_REFLECT_EXPAND2(__VA_ARGS__)))
#define MG_REFLECT_EXPAND4(...)                 MG_REFLECT_EXPAND3(MG_REFLECT_EXPAND3(MG_REFLECT_EXPAND3(__VA_ARGS__)))
#define MG_REFLECT_EXPAND(...)                  MG_REFLECT_EXPAND4(MG_REFLECT_EXPAND4(MG_REFLECT_EXPAND4(__VA_ARGS__)))
#define MG_REFLECT_FOR_EACH_HELPER(visitor, self, first, ...) \
    MG_REFLECT_FIELD(visitor, self, first)                    \
    __VA_OPT__(MG_REFLECT_FOR_EACH_AGAIN MG_REFLECT_PARENS(visitor, self, __VA_ARGS__))
#define MG_REFLECT_FOR_EACH_AGAIN() MG_REFLECT_FOR_EACH_HELPER
#define MG_REFLECT_FIELDS(visitor, self, ...) \
    __VA_OPT__(MG_REFLECT_EXPAND(MG_REFLECT_FOR_EACH_HELPER(visitor, self, __VA_ARGS__)))

// 在结构体内生成类型名及全部直接字段的可变/只读访问入口。
#define MG_REFLECT(Type, ...)                                                                    \
    [[maybe_unused]] static constexpr std::string_view kMgTypeName = MG_REFLECT_STRINGIZE(Type); \
    template <class V>                                                                           \
    static constexpr void mgReflect(V& visitor, Type& self)                                      \
    {                                                                                            \
        MG_REFLECT_FIELDS(visitor, self, __VA_ARGS__)                                            \
    }                                                                                            \
    template <class V>                                                                           \
    static constexpr void mgReflect(V& visitor, const Type& self)                                \
    {                                                                                            \
        MG_REFLECT_FIELDS(visitor, self, __VA_ARGS__)                                            \
    }

// 在派生结构体内先访问基类字段，再访问直接字段。
#define MG_REFLECT_DERIVED(Type, Base, ...)                                                      \
    [[maybe_unused]] static constexpr std::string_view kMgTypeName = MG_REFLECT_STRINGIZE(Type); \
    template <class V>                                                                           \
    static constexpr void mgReflect(V& visitor, Type& self)                                      \
    {                                                                                            \
        Base::mgReflect(visitor, static_cast<Base&>(self));                                      \
        MG_REFLECT_FIELDS(visitor, self, __VA_ARGS__)                                            \
    }                                                                                            \
    template <class V>                                                                           \
    static constexpr void mgReflect(V& visitor, const Type& self)                                \
    {                                                                                            \
        Base::mgReflect(visitor, static_cast<const Base&>(self));                                \
        MG_REFLECT_FIELDS(visitor, self, __VA_ARGS__)                                            \
    }
