#include "AudioManager.h"
#include "audio/AudioEngine.h"
#include "FairyGUI.h"

USING_NS_FGUI;

namespace gameui
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

// 播放音效
int AudioManager::playFSX(std::string_view path, float volumeScale)
{
    if (path.empty())
    {
        return -1;
    }

    // 判断路径是否以 ui:// 开头
    if (path.starts_with("ui://"))
    {
        // 处理 UI 音效路径
        auto item = UIPackage::getItemByURL(path);
        if (item)
        {
            return ax::AudioEngine::play2d(item->file, false, volumeScale);
        }
        return -1;
    }
    else
    {
        return ax::AudioEngine::play2d(path, false, volumeScale);
    }
}

// 停止音效
void AudioManager::stopSFX(int id)
{
    if (id != -1)
    {
        ax::AudioEngine::stop(id);
    }
}

// 播放 UI 音效
void AudioManager::playUISFX(std::string_view path, float volumeScale)
{
    playFSX(path, volumeScale);
}

}  // namespace gameui
