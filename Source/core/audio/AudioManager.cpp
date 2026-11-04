#include "core/audio/AudioManager.h"

#include "audio/AudioEngine.h"
#include "FairyGUI.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/tables/SoundTables.h"

USING_NS_FGUI;

namespace game
{

AudioManager* AudioManager::s_instance = nullptr;

AudioManager::AudioManager() {}

AudioManager::~AudioManager() {}

AudioManager* AudioManager::getInstance()
{
    if (!s_instance)
    {
        s_instance = new AudioManager();
    }
    return s_instance;
}

void AudioManager::destroyInstance()
{
    delete s_instance;
    s_instance = nullptr;
}

void AudioManager::playBGM(std::string_view path)
{
    if (m_bgmPath == path)
    {
        return;
    }

    stopBGM();
    m_bgmId   = ax::AudioEngine::play2d(path, true, 1.0f);
    m_bgmPath = path;
}

void AudioManager::stopBGM()
{
    if (m_bgmId != -1)
    {
        ax::AudioEngine::stop(m_bgmId);
        m_bgmId = -1;
        m_bgmPath.clear();
    }
}

void AudioManager::pauseBGM()
{
    if (m_bgmId != -1)
    {
        ax::AudioEngine::pause(m_bgmId);
    }
}

void AudioManager::resumeBGM()
{
    if (m_bgmId != -1)
    {
        ax::AudioEngine::resume(m_bgmId);
    }
}

int AudioManager::playSFX(std::string_view path, bool loop, float volume)
{
    const std::string resolvedPath = resolvePath(path);
    if (resolvedPath.empty())
    {
        return -1;
    }
    if (!canPlayFile(resolvedPath))
    {
        return -1;
    }

    const int audioId = ax::AudioEngine::play2d(resolvedPath, loop, volume);
    if (audioId == -1)
    {
        return -1;
    }

    retainAudio(audioId, resolvedPath);
    ax::AudioEngine::setFinishCallback(audioId, [this](int id, std::string_view) { releaseAudio(id); });
    return audioId;
}

int AudioManager::playSound(int32_t soundId)
{
    if (soundId <= 0)
    {
        return -1;
    }
    const auto* row = mugen::conf::Config::instance().find<mugen::conf::ResSoundConfig>(soundId);
    if (!row)
    {
        if (m_missingSoundWarned.insert(soundId).second)
            AXLOGW("AudioManager: res_sound {} not found", soundId);
        return -1;
    }
    if (row->fileName.empty())
    {
        return -1;
    }
    return playSFX(row->fileName, row->loop != 0, row->volume.toFloatForRender());
}

void AudioManager::stopSFX(int id)
{
    if (id != -1)
    {
        releaseAudio(id);
        ax::AudioEngine::stop(id);
    }
}

void AudioManager::setSFXLoop(int audioId, bool loop)
{
    if (audioId != -1)
        ax::AudioEngine::setLoop(audioId, loop);
}

void AudioManager::playUISFX(std::string_view path, float volumeScale)
{
    playSFX(path, false, volumeScale);
}

std::string AudioManager::resolvePath(std::string_view path) const
{
    if (path.empty())
    {
        return {};
    }

    if (path.starts_with("ui://"))
    {
        auto item = UIPackage::getItemByURL(path);
        if (!item)
            return {};
        return item->file;
    }
    return std::string(path);
}

bool AudioManager::canPlayFile(const std::string& resolvedPath) const
{
    const auto it = m_filePlayCounts.find(resolvedPath);
    if (it == m_filePlayCounts.end())
        return true;
    return it->second < kMaxSameFileInstances;
}

void AudioManager::retainAudio(int audioId, const std::string& resolvedPath)
{
    m_audioIdToFile[audioId] = resolvedPath;
    ++m_filePlayCounts[resolvedPath];
}

void AudioManager::releaseAudio(int audioId)
{
    const auto it = m_audioIdToFile.find(audioId);
    if (it == m_audioIdToFile.end())
        return;
    const auto countIt = m_filePlayCounts.find(it->second);
    if (countIt != m_filePlayCounts.end())
    {
        --countIt->second;
        if (countIt->second <= 0)
            m_filePlayCounts.erase(countIt);
    }
    m_audioIdToFile.erase(it);
}

}  // namespace game
