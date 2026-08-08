#include "Object.h"
#include <mutex>

NS_MG_BEGIN

namespace
{
std::mutex g_objectSetMutex;
}
std::unordered_set<Object*> g_objectSet;

Object::Object()
{
    g_objectSetMutex.lock();
    g_objectSet.insert(this);
    g_objectSetMutex.unlock();
}

Object::~Object()
{
    g_objectSetMutex.lock();
    g_objectSet.erase(this);
    g_objectSetMutex.unlock();
}

void Object::serialize(ByteBuffer& byteBuffer) const {}

bool Object::deserialize(ByteBuffer& byteBuffer)
{
    return true;
}

void Object::copySpecialProperties(Object* other) {}

NS_MG_END
