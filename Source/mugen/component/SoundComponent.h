#pragma once

#include "mugen/core/ecs/Component.h"

NS_MG_BEGIN

enum AudioEnginePlayState : int32_t
{
    kPlayState_Error,
    kPlayState_Initializing,
    kPlayState_Playing,
    kPlayState_Paused
};

enum SoundRuntimeState : uint8_t
{
    // 等待播放延迟
    kSoundRuntimeState_Delay,
    // 正在播放
    kSoundRuntimeState_Playing,
    // 播放完成
    kSoundRuntimeState_Finished
};

class SoundRuntimeData : public Object
{
public:
    typedef Object Super;

public:
    SoundRuntimeData() {}
    virtual ~SoundRuntimeData() {}

    // 音效 id（res_sound）；stop/isPlaying 也可用该值匹配
    int32_t soundId = 0;
    // 兼容旧调用方字符串键（可解析为 int 时写入 soundId）
    std::string key;
    // 是否循环播放（由 res_sound.loop 决定；引擎侧循环时此处为 false）
    bool loop = false;
    // 首次播放前的延迟（秒）
    float delay = 0.0f;
    // 每次播放完毕后等待的基准时间（秒）
    float loopDelay = 0.0f;
    // 等待时间的随机浮动范围（秒），实际等待 = loopDelay ± rand(loopDelayRange)
    float loopDelayRange = 0.0f;
    // 运行时状态
    SoundRuntimeState state = SoundRuntimeState::kSoundRuntimeState_Delay;
    // 当前状态的计时器（秒）
    float timer = 0.0f;
    // 如果正在播放，这里记录GameWorld返回的 audioId
    int audioId = -1;

    MG_DEFINE_SERIALIZABLE(soundId, key, loop, delay, loopDelay, loopDelayRange, state, timer);
};

class SoundComponent : public Component
{
public:
    typedef Component Super;

public:
    SoundComponent() {}
    virtual ~SoundComponent() {}

    std::vector<SoundRuntimeData> sounds;

    // MG_DEFINE_SERIALIZABLE(sounds);
};

NS_MG_END
