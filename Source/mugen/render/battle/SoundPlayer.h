#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim_legacy/World.h"

#ifdef RUNTIME_IN_AXMOL

#    include <map>
#    include <random>

NS_MG_BEGIN

class SoundPlayer
{
public:
    explicit SoundPlayer(uint32_t seed = 0x51A7u) : m_random(seed) {}

    void consume(const sim_legacy::PresentationEvent& event);
    void clear();

private:
    using ActionKey = std::pair<uint32_t, int32_t>;

    std::minstd_rand m_random;
    std::map<ActionKey, int> m_loopAudioByAction;
};

NS_MG_END

#endif
