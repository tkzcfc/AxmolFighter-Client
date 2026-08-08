#pragma once

#include "mugen/core/ecs/System.h"
#include "mugen/component/SoundComponent.h"
#include "mugen/core/math/Random.h"

NS_MG_BEGIN

class SoundSystem : public System
{
public:
    typedef System Super;

public:
    SoundSystem();
    virtual ~SoundSystem();

    virtual void init(ECSManager* ecs) override;
    virtual void onEntityAdded(Entity* entity) override;
    virtual void onEntityRemoved(Entity* entity) override;
    virtual void update() override;

    // 按 res_sound id 播放；id<=0 忽略
    void play(int32_t soundId, Entity* entity);

    // 兼容旧字符串键：仅当整串可解析为 int 时转发到 play(int)
    void play(const std::string& key, Entity* entity);

    // 按 res_sound id 播放 BGM
    void playBgmById(int32_t soundId);

    // 播放背景音乐，确保同一时间只有一个背景音乐在播放
    void playBGM(const std::string& filePath);

    // 停止背景音乐
    void stopBGM();

    // 按 soundId 停止
    void stop(int32_t soundId, Entity* entity);

    // 兼容旧字符串键
    void stop(const std::string& key, Entity* entity);

    // 停止指定实体上的所有音效
    void stopAllByEntity(Entity* entity);

    bool isPlaying(int32_t soundId, Entity* entity) const;

    bool isPlaying(const std::string& key, Entity* entity) const;

    // 设置随机数生成器种子
    void setRandomSeed(uint64_t seed);

private:
    // 释放所有音频资源,用于在游戏反序列化之前剥离音频资源，避免反序列化之前销毁实体调用 onEntityRemoved 时突然停止音频
    void releaseAudioResources(Entity* entity);

    void preloadAudio(const std::string& filePath, const std::function<void(bool)> callback);

    int32_t playAudio(const std::string& filePath, bool loop, float volume = 1.0f);

    void setVolume(int32_t audioId, float volume);

    int32_t getAudioState(int32_t audioId) const;

    void stopAudio(int32_t audioId);

    // 尝试同步背景音乐进度
    bool trySyncBGM();

    static int32_t parseSoundId(const std::string& key);

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);

private:
    // 随机数生成器
    Random m_random;
    // 正在播放的背景音乐文件路径,在serializeCustomImpl手动序列化这个字段，确保在反序列化时能正确恢复正在播放的背景音乐
    std::string m_currentBGMFilePath;
    // 正在播放的背景音乐时间
    float m_currentBGMTime = 0.0f;

    // 上一次播放的背景音乐时间,用于在切换背景音乐时尝试同步时间(不需要序列化)
    float m_lastBGMTime = 0.0f;
    // 标记是否等待同步背景音乐时间(不需要序列化)
    bool m_waitingSyncBGMTime = false;
    // 需要恢复音量的背景音乐id(不需要序列化)
    int32_t m_willSyncBGMVolumeAudioId = -1;

    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl, m_random, m_currentBGMTime)
};

NS_MG_END
