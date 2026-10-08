#pragma once

#include "mugen/core/Object.h"
#include "Fixed.h"

NS_MG_BEGIN

class Random : public Object
{
public:
    typedef Object Super;

    Random();
    explicit Random(uint64_t seed);

    void seed(uint64_t s);
    uint64_t next();
    Fixed randomF(Fixed min, Fixed max);
    int32_t randomN(int32_t min, int32_t max);

    MG_DEFINE_SERIALIZABLE(m_s0, m_s1, m_s2, m_s3);

private:
    static uint64_t _splitmix64(uint64_t& x);
    static uint64_t _rotl(uint64_t v, int k);

    uint64_t m_s0 = 0;
    uint64_t m_s1 = 0;
    uint64_t m_s2 = 0;
    uint64_t m_s3 = 0;
};

NS_MG_END
