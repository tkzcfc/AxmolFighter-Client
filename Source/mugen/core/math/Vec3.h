#pragma once

#include "../Object.h"

NS_MG_BEGIN

class Vector3f : public Object
{
public:
    typedef Object Super;

public:
    Vector3f() : x(0.0f), y(0.0f), z(0.0f) {}
    Vector3f(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}

    float x;
    float y;
    float z;
    MG_DEFINE_SERIALIZABLE(x, y, z);
};

class Vector3i : public Object
{
public:
    typedef Object Super;

public:
    Vector3i() : x(0), y(0), z(0) {}
    Vector3i(int32_t xx, int32_t yy, int32_t zz) : x(xx), y(yy), z(zz) {}

    int32_t x;
    int32_t y;
    int32_t z;
    MG_DEFINE_SERIALIZABLE(x, y, z);
};

NS_MG_END
