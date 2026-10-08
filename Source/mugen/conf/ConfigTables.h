#pragma once

#include "tables/SkillTables.h"

#define MG_CONFIG_TABLES(X)              \
    X(skill, SkillAttackConfig, KeyById) \
    X(skill_hit, SkillHitConfig, KeyById)

namespace mugen::conf
{
struct KeyById
{
    template <class T>
    static int32_t get(const T& row)
    {
        return row.id;
    }
};
}  // namespace mugen::conf
