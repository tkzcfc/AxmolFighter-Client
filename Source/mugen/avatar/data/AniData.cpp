#include "AniData.h"

NS_MG_BEGIN

// 累加所有帧持续时间。
int AniData::totalDurationMs() const
{
    int total = 0;
    for (const AniFrame& frame : frames)
        total += frame.delay;
    return total;
}

// 将时间映射为有效帧索引。
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
