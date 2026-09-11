#include "AniData.h"

NS_MG_BEGIN

int AniData::totalDurationMs() const
{
    int total = 0;
    for (const AniFrame& frame : frames)
        total += frame.delay;
    return total;
}

int AniData::frameIndexAtTime(int timeMs) const
{
    if (frames.empty())
        return -1;

    const int total = totalDurationMs();
    if (total <= 0)
        return 0;

    if (timeMs < 0)
        return 0;
    if (timeMs >= total)
        return static_cast<int>(frames.size()) - 1;

    int cursor = 0;
    for (size_t i = 0; i < frames.size(); ++i)
    {
        const int delay = frames[i].delay;
        if (timeMs < cursor + delay)
            return static_cast<int>(i);
        cursor += delay;
    }
    return static_cast<int>(frames.size()) - 1;
}

NS_MG_END
