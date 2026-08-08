#pragma once

#include "../Object.h"

NS_MG_BEGIN

class Vector2f : public Object
{
public:
    typedef Object Super;

public:
    Vector2f() : x(0.0f), y(0.0f) {}
    Vector2f(float xx, float yy) : x(xx), y(yy) {}

    float x;
    float y;
    MG_DEFINE_SERIALIZABLE(x, y);
};

class Vector2i : public Object
{
public:
    typedef Object Super;

public:
    Vector2i() : x(0), y(0) {}
    Vector2i(int32_t xx, int32_t yy) : x(xx), y(yy) {}

    int32_t x;
    int32_t y;
    MG_DEFINE_SERIALIZABLE(x, y);
};

NS_MG_END
