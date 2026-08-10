#pragma once

#include "mugen/core/Object.h"

#include <unordered_map>

NS_MG_BEGIN

/** 扩展属性通道（与参照 ExtendAttributeType 数值对齐） */
enum class ExtendAttributeType : int32_t
{
    AddHurt        = 100,
    AvoidHurt      = 101,
    AddCrit        = 102,
    AvoidCrit      = 103,
    AddEp          = 104,
    AvoidEp        = 105,
    AddMaxHp       = 106,
    AvoidMaxHp     = 107,
    DragonDrop     = 108,
    AddDodge       = 109,
    AvoidDodge     = 110,
    AddHit         = 111,
    AvoidHit       = 112,
    AddArtifactHit = 113,
};

class ExtendAttribute : public Object
{
public:
    typedef Object Super;

    std::unordered_map<int32_t, float> values;

    float get(ExtendAttributeType t) const
    {
        auto it = values.find(static_cast<int32_t>(t));
        return it == values.end() ? 0.0f : it->second;
    }

    void modify(ExtendAttributeType t, float delta)
    {
        const int32_t key = static_cast<int32_t>(t);
        values[key]       = get(t) + delta;
    }

    void set(ExtendAttributeType t, float v) { values[static_cast<int32_t>(t)] = v; }

    MG_DEFINE_SERIALIZABLE(values);
};

NS_MG_END
