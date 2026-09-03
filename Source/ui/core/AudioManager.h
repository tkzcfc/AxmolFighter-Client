#pragma once

#include <string>
#include <string_view>

namespace gameui
{
class AudioManager
{
public:
    AudioManager();

    virtual ~AudioManager();

    static AudioManager* getInstance();

    static void destroyInstance();

    // 播放背景音乐
    void playBGM(std::string_view path);

    // 停止背景音乐
    void stopBGM();

    // 暂停背景音乐
    void pauseBGM();

    // 恢复背景音乐
    void resumeBGM();

    // 播放音效
    int playFSX(std::string_view, float volumeScale = 1.0f);

    // 停止音效
    void stopSFX(int id);

    // 播放 UI 音效
    void playUISFX(std::string_view path, float volumeScale = 1.0f);

private:
    static AudioManager* s_instance;

    int m_bgmId = -1;
    std::string m_bgmPath;
};

}  // namespace gameui
