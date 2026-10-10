#include "doctest.h"
#include "mugen/avatar/data/MotionMap.h"

using namespace mugen;

TEST_CASE("Terminal clip duration follows a zero spine animation and playback length stays")
{
    Motion motion;
    motion.duration = 33;
    MotionClip clip;
    clip.durationMs = 33;
    motion.clips.push_back(clip);

    REQUIRE(motion.alignClipToSpine(0, 0));
    CHECK(motion.clips[0].durationMs == 0);
    CHECK(motion.durationMs() == 33);
}

TEST_CASE("Non-terminal clip duration stays when it disagrees with the spine animation")
{
    Motion motion;
    motion.duration = 66;
    MotionClip first;
    first.durationMs = 33;
    MotionClip second;
    second.durationMs = 33;
    motion.clips.push_back(first);
    motion.clips.push_back(second);

    CHECK_FALSE(motion.alignClipToSpine(0, 0));
    CHECK(motion.clips[0].durationMs == 33);
    CHECK(motion.durationMs() == 66);
}

TEST_CASE("Terminal clip with a later key keeps its stored duration")
{
    CombatTimeline timeline;
    timeline.duration = 33;
    CombatTrack track;
    CombatKey key;
    key.timeMs = 10;
    track.keys.push_back(key);
    timeline.tracks.push_back(track);

    Motion motion;
    motion.duration = 33;
    MotionClip clip;
    clip.durationMs = 33;
    clip.timeline   = &timeline;
    motion.clips.push_back(clip);

    CHECK_FALSE(motion.alignClipToSpine(0, 0));
    CHECK(motion.clips[0].durationMs == 33);
    CHECK(motion.durationMs() == 33);
}
