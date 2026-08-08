#include "SoundSystem.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"

#include <cstdlib>

#ifdef RUNTIME_IN_AXMOL
#    include "audio/AudioEngine.h"
#endif

NS_MG_BEGIN

// 正在播放的背景音乐audioId,和本地相关,不需要序列化
static int32_t currentBGMAudioId = -1;

SoundSystem::SoundSystem() {}

SoundSystem::~SoundSystem()
{
    if (!getECSManager()->isDeserialized())
    {
        stopBGM();
    }
}

void SoundSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, SoundComponent);
}

void SoundSystem::onEntityAdded(Entity* entity) {}

void SoundSystem::onEntityRemoved(Entity* entity)
{
    if (getECSManager()->isDeserialized())
    {
        releaseAudioResources(entity);
    }
    stopAllByEntity(entity);
}

void SoundSystem::update()
{
    float dtSec = getECSManager()->getLastUpdateTimeMs() / 1000.0f;

    if (!m_currentBGMFilePath.empty())
    {
        m_currentBGMTime += dtSec;
    }

    if (this->m_willSyncBGMVolumeAudioId != -1 && this->m_willSyncBGMVolumeAudioId == currentBGMAudioId)
    {
        this->m_willSyncBGMVolumeAudioId = -1;
        setVolume(currentBGMAudioId, 1.0f);
    }

    if (this->m_waitingSyncBGMTime && this->trySyncBGM())
    {
        this->m_waitingSyncBGMTime       = false;
        this->m_willSyncBGMVolumeAudioId = currentBGMAudioId;
    }

    for (auto entity : entities)
    {
        auto soundComp = MG_GET_COMPONENT(entity, SoundComponent);
        auto& sounds   = soundComp->sounds;

        for (auto it = sounds.begin(); it != sounds.end();)
        {
            auto& data = *it;
            data.timer += dtSec;

            bool isDeleted = false;

            if (data.state == kSoundRuntimeState_Delay)
            {
                if (data.timer >= data.delay)
                {
                    data.state = kSoundRuntimeState_Playing;
                    data.timer = 0.0f;
                    auto* cfg  = Config::getInstance()->getResSoundById(data.soundId);
                    if (cfg && !cfg->fileName.empty())
                    {
                        data.audioId = playAudio(cfg->fileName, cfg->loop != 0, cfg->volume);
                        if (data.audioId == -1)
                        {
                            MG_LOG_W("Failed to play audio file '{}'", cfg->fileName);
                        }
                    }
                }
            }
            else if (data.state == kSoundRuntimeState_Playing)
            {
                if (data.audioId == -1)
                {
                    if (data.timer >= 0.5f)
                    {
                        data.state = kSoundRuntimeState_Finished;
                        data.timer = 0.0f;
                    }
                }
                else
                {
                    auto state = getAudioState(data.audioId);
                    if (state == AudioEnginePlayState::kPlayState_Error ||
                        state == AudioEnginePlayState::kPlayState_Paused)
                    {
                        data.state = kSoundRuntimeState_Finished;
                        data.timer = 0.0f;
                    }
                }
            }
            else if (data.state == kSoundRuntimeState_Finished)
            {
                if (data.audioId != -1)
                {
                    stopAudio(data.audioId);
                    data.audioId = -1;
                }

                if (data.loop)
                {
                    float wait = data.loopDelay;
                    if (data.loopDelayRange > 0.0001f)
                    {
                        float r = m_random.nextFloat(-1.0f, 1.0f);
                        wait += r * data.loopDelayRange;
                        if (wait < 0.0f)
                        {
                            wait = 0.0f;
                        }
                    }
                    data.delay = wait;
                    data.state = kSoundRuntimeState_Delay;
                    data.timer = 0.0f;
                }
                else
                {
                    it        = sounds.erase(it);
                    isDeleted = true;
                }
            }
            else
            {
                MG_ASSERT(false && "Invalid sound runtime state");
            }

            if (!isDeleted)
            {
                ++it;
            }
        }
    }
}

int32_t SoundSystem::parseSoundId(const std::string& key)
{
    if (key.empty())
    {
        return 0;
    }
    char* end         = nullptr;
    const long parsed = std::strtol(key.c_str(), &end, 10);
    if (end == key.c_str() || (end && *end != '\0'))
    {
        return 0;
    }
    return static_cast<int32_t>(parsed);
}

void SoundSystem::play(int32_t soundId, Entity* entity)
{
    if (soundId <= 0)
    {
        return;
    }
    MG_ASSERT(entity != nullptr);
    auto soundComp = MG_GET_COMPONENT(entity, SoundComponent);
    MG_ASSERT(soundComp != nullptr);

    auto* cfg = Config::getInstance()->getResSoundById(soundId);
    if (!cfg || cfg->fileName.empty())
    {
        MG_LOG_W("ResSound missing or empty file for id={}", soundId);
        return;
    }

    SoundRuntimeData data;
    data.soundId = soundId;
    data.key     = std::to_string(soundId);
    data.loop    = false;
    data.state   = kSoundRuntimeState_Playing;
    data.audioId = playAudio(cfg->fileName, cfg->loop != 0, cfg->volume);
    if (data.audioId == -1)
    {
        MG_LOG_W("Failed to play audio file '{}' id={}", cfg->fileName, soundId);
    }
    soundComp->sounds.push_back(data);
}

void SoundSystem::play(const std::string& key, Entity* entity)
{
    const int32_t soundId = parseSoundId(key);
    if (soundId <= 0)
    {
        if (!key.empty())
        {
            MG_LOG_W("SoundSystem::play ignored non-int key '{}'", key);
        }
        return;
    }
    play(soundId, entity);
}

void SoundSystem::playBgmById(int32_t soundId)
{
    if (soundId <= 0)
    {
        return;
    }
    auto* cfg = Config::getInstance()->getResSoundById(soundId);
    if (!cfg || cfg->fileName.empty())
    {
        MG_LOG_W("ResSound BGM missing or empty file for id={}", soundId);
        return;
    }
    playBGM(cfg->fileName);
}

void SoundSystem::playBGM(const std::string& filePath)
{
    if (filePath.empty())
    {
        return;
    }

    if (m_currentBGMFilePath != filePath)
    {
        if (currentBGMAudioId != -1)
        {
            stopAudio(currentBGMAudioId);
            currentBGMAudioId = -1;
        }

        currentBGMAudioId    = playAudio(filePath, true, 0.0f);
        m_currentBGMFilePath = filePath;

        this->m_waitingSyncBGMTime       = this->m_lastBGMTime > 0.0f;
        this->m_willSyncBGMVolumeAudioId = this->m_waitingSyncBGMTime ? -1 : currentBGMAudioId;
        this->m_currentBGMTime           = this->m_waitingSyncBGMTime ? this->m_lastBGMTime : 0.0f;

        this->m_lastBGMTime = 0.0f;
    }
}

void SoundSystem::stopBGM()
{
    if (currentBGMAudioId != -1)
    {
        stopAudio(currentBGMAudioId);
        currentBGMAudioId = -1;
    }
    m_currentBGMTime = 0.0f;
    m_currentBGMFilePath.clear();
}

void SoundSystem::stop(int32_t soundId, Entity* entity)
{
    if (soundId <= 0)
    {
        return;
    }
    stop(std::to_string(soundId), entity);
}

void SoundSystem::stop(const std::string& key, Entity* entity)
{
    MG_ASSERT(entity != nullptr);
    auto soundComp = MG_GET_COMPONENT(entity, SoundComponent);
    MG_ASSERT(soundComp != nullptr);

    const int32_t soundId = parseSoundId(key);
    auto& sounds          = soundComp->sounds;
    for (auto it = sounds.begin(); it != sounds.end();)
    {
        if (it->key == key || (soundId > 0 && it->soundId == soundId))
        {
            if (it->audioId != -1)
            {
                stopAudio(it->audioId);
                it->audioId = -1;
            }
            it = sounds.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SoundSystem::stopAllByEntity(Entity* entity)
{
    MG_ASSERT(entity != nullptr);
    auto soundComp = MG_GET_COMPONENT(entity, SoundComponent);
    MG_ASSERT(soundComp != nullptr);
    auto& sounds = soundComp->sounds;
    for (auto& data : sounds)
    {
        if (data.audioId != -1)
        {
            stopAudio(data.audioId);
            data.audioId = -1;
        }
    }
    sounds.clear();
}

bool SoundSystem::isPlaying(int32_t soundId, Entity* entity) const
{
    if (soundId <= 0)
    {
        return false;
    }
    return isPlaying(std::to_string(soundId), entity);
}

bool SoundSystem::isPlaying(const std::string& key, Entity* entity) const
{
    MG_ASSERT(entity != nullptr);
    auto soundComp = MG_GET_COMPONENT(entity, SoundComponent);
    MG_ASSERT(soundComp != nullptr);
    const int32_t soundId = parseSoundId(key);
    const auto& sounds    = soundComp->sounds;
    for (const auto& data : sounds)
    {
        if (data.key == key || (soundId > 0 && data.soundId == soundId))
        {
            return data.state == kSoundRuntimeState_Playing || data.state == kSoundRuntimeState_Delay;
        }
    }
    return false;
}

void SoundSystem::setRandomSeed(uint64_t seed)
{
    m_random.seed(seed);
}

void SoundSystem::releaseAudioResources(Entity* entity)
{
    auto soundComp = MG_GET_COMPONENT(entity, SoundComponent);

    for (auto& data : soundComp->sounds)
    {
        if (data.audioId != -1)
        {
#ifdef RUNTIME_IN_AXMOL
            if (data.timer <= 0.5f)
            {
                auto duration = ax::AudioEngine::getDuration(data.audioId);
                if (duration > 5.0f)
                {
                    ax::AudioEngine::stop(data.audioId);
                }
            }
#endif
            data.audioId = -1;
        }
    }
}

void SoundSystem::preloadAudio(const std::string& filePath, const std::function<void(bool)> callback)
{
#ifdef RUNTIME_IN_AXMOL
    ax::AudioEngine::preload(filePath, callback);
#else
    if (callback)
    {
        callback(false);
    }
#endif
}

int32_t SoundSystem::playAudio(const std::string& filePath, bool loop, float volume)
{
#ifdef RUNTIME_IN_AXMOL
    return ax::AudioEngine::play2d(filePath, loop, volume);
#else
    (void)filePath;
    (void)loop;
    (void)volume;
    return -1;
#endif
}

void SoundSystem::setVolume(int32_t audioId, float volume)
{
#ifdef RUNTIME_IN_AXMOL
    ax::AudioEngine::setVolume(audioId, volume);
#else
    (void)audioId;
    (void)volume;
#endif
}

int32_t SoundSystem::getAudioState(int32_t audioId) const
{
#ifdef RUNTIME_IN_AXMOL
    switch (ax::AudioEngine::getState(audioId))
    {
    case ax::AudioEngine::AudioState::ERROR:
        return AudioEnginePlayState::kPlayState_Error;
    case ax::AudioEngine::AudioState::INITIALIZING:
        return AudioEnginePlayState::kPlayState_Initializing;
    case ax::AudioEngine::AudioState::PLAYING:
        return AudioEnginePlayState::kPlayState_Playing;
    case ax::AudioEngine::AudioState::PAUSED:
        return AudioEnginePlayState::kPlayState_Paused;
    }
#endif
    (void)audioId;
    return AudioEnginePlayState::kPlayState_Error;
}

void SoundSystem::stopAudio(int32_t audioId)
{
#ifdef RUNTIME_IN_AXMOL
    ax::AudioEngine::stop(audioId);
#else
    (void)audioId;
#endif
}

bool SoundSystem::trySyncBGM()
{
    if (currentBGMAudioId == -1)
    {
        return true;
    }

#ifdef RUNTIME_IN_AXMOL
    float duration = ax::AudioEngine::getDuration(currentBGMAudioId);
    if (duration > 0.0f)
    {
        while (m_currentBGMTime > duration)
        {
            m_currentBGMTime -= duration;
        }
        ax::AudioEngine::setCurrentTime(currentBGMAudioId, m_currentBGMTime);
        return true;
    }
    return false;
#else
    return true;
#endif
}

void SoundSystem::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeValue(m_currentBGMFilePath);
}

bool SoundSystem::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    std::string bgmFilePath;
    if (!byteBuffer.getValue(bgmFilePath))
    {
        MG_LOG_E("Failed to deserialize SoundSystem: unable to read BGM file path");
        return false;
    }

    m_lastBGMTime = m_currentBGMTime;
    this->playBGM(bgmFilePath);

    return true;
}

NS_MG_END
