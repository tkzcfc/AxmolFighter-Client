#pragma once

#include "ByteBuffer.h"

NS_MG_BEGIN

template <typename T>
void serialize_impl(ByteBuffer& buf, const T& value)
{
    buf.writeValue(value);
}

template <typename T, typename... Args>
void serialize_impl(ByteBuffer& buf, const T& value, const Args&... args)
{
    buf.writeValue(value);
    serialize_impl(buf, args...);
}

template <typename T>
bool deserialize_impl(ByteBuffer& buf, T& value)
{
    return buf.getValue(value);
}

template <typename T, typename... Args>
bool deserialize_impl(ByteBuffer& buf, T& value, Args&... args)
{
    if (!buf.getValue(value))
        return false;
    return deserialize_impl(buf, args...);
}

#define MG_DEFINE_SERIALIZABLE(...)                               \
    virtual void serialize(ByteBuffer& byteBuffer) const override \
    {                                                             \
        Super::serialize(byteBuffer);                             \
        serialize_impl(byteBuffer, __VA_ARGS__);                  \
    }                                                             \
    virtual bool deserialize(ByteBuffer& byteBuffer) override     \
    {                                                             \
        if (!Super::deserialize(byteBuffer))                      \
            return false;                                         \
        return deserialize_impl(byteBuffer, __VA_ARGS__);         \
    }

#define MG_DEFINE_SERIALIZABLE_CUSTOM(serialize_custom, deserialize_custom, ...) \
    virtual void serialize(ByteBuffer& byteBuffer) const override                \
    {                                                                            \
        Super::serialize(byteBuffer);                                            \
        serialize_impl(byteBuffer, __VA_ARGS__);                                 \
        serialize_custom(byteBuffer);                                            \
    }                                                                            \
    virtual bool deserialize(ByteBuffer& byteBuffer) override                    \
    {                                                                            \
        if (!Super::deserialize(byteBuffer))                                     \
            return false;                                                        \
        if (!deserialize_impl(byteBuffer, __VA_ARGS__))                          \
            return false;                                                        \
        return deserialize_custom(byteBuffer);                                   \
    }

NS_MG_END
