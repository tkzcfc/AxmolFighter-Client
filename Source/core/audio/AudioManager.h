#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace game
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

    // 播放音效（支持 ui:// 路径）
    int playSFX(std::string_view path, bool loop = false, float volume = 1.0f);

    // 按 res_sound id 播放音效
    int playSound(int32_t soundId);

    // 停止音效
    void stopSFX(int id);

    // 设置音效循环
    void setSFXLoop(int audioId, bool loop);

    // 播放 UI 音效
    void playUISFX(std::string_view path, float volumeScale = 1.0f);

private:
    std::string resolvePath(std::string_view path) const;
    bool canPlayFile(const std::string& resolvedPath) const;
    void retainAudio(int audioId, const std::string& resolvedPath);
    void releaseAudio(int audioId);

    static AudioManager* s_instance;
    static constexpr int kMaxSameFileInstances = 3;

    int m_bgmId = -1;
    std::string m_bgmPath;
    std::unordered_map<std::string, int> m_filePlayCounts;
    std::unordered_map<int, std::string> m_audioIdToFile;
    std::unordered_set<int32_t> m_missingSoundWarned;
};

}  // namespace game
