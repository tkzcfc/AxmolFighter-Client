#pragma once

#include "serialize/ByteBuffer.h"
#include "serialize/SerializableHelper.h"

NS_MG_BEGIN

class Object
{
public:
    Object();

    virtual ~Object();

    virtual void serialize(ByteBuffer& byteBuffer) const;

    virtual bool deserialize(ByteBuffer& byteBuffer);

    virtual void copySpecialProperties(Object* other);
};

extern std::unordered_set<Object*> g_objectSet;

template <typename T>
static T* clone(T& other)
{
    ByteBuffer buffer(sizeof(T));
    other.serialize(buffer);

    buffer.writeFinish();

    T* ptr = new (std::nothrow) T();
    if (ptr == NULL)
        return NULL;

    if (!ptr->deserialize(buffer))
    {
        delete ptr;
        return NULL;
    }
    ptr->copySpecialProperties(&other);
    return ptr;
}

NS_MG_END
