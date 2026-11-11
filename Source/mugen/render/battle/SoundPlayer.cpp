#include "SoundPlayer.h"

#ifdef RUNTIME_IN_AXMOL

#    include "core/audio/AudioManager.h"

NS_MG_BEGIN

namespace
{
int32_t chooseSoundId(const sim_legacy::Sound& sound, std::minstd_rand& random)
{
    if (sound.id > 0)
        return sound.id;

    std::vector<int32_t> candidates;
    candidates.reserve(sound.m_candidates.size());
    for (int32_t id : sound.m_candidates)
    {
        if (id > 0)
            candidates.push_back(id);
    }
    if (candidates.empty())
        return -1;
    std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
    return candidates[distribution(random)];
}
}  // namespace

void SoundPlayer::consume(const sim_legacy::PresentationEvent& event)
{
    const auto* sound = std::get_if<sim_legacy::Sound>(&event.payload);
    if (!sound)
        return;

    if (sound->m_stopLoop)
    {
        if (sound->m_actionId < 0)
            return;
        const ActionKey key{event.entity.value, sound->m_actionId};
        const auto it = m_loopAudioByAction.find(key);
        if (it == m_loopAudioByAction.end())
            return;
        game::AudioManager::getInstance()->setSFXLoop(it->second, false);
        m_loopAudioByAction.erase(it);
        return;
    }

    const int32_t chosenSoundId = chooseSoundId(*sound, m_random);
    if (chosenSoundId <= 0)
        return;

    const int audioId = game::AudioManager::getInstance()->playSound(chosenSoundId);
    if (sound->m_actionId < 0 || audioId == -1)
        return;

    const ActionKey key{event.entity.value, sound->m_actionId};
    if (const auto it = m_loopAudioByAction.find(key); it != m_loopAudioByAction.end())
        game::AudioManager::getInstance()->setSFXLoop(it->second, false);
    m_loopAudioByAction[key] = audioId;
}

void SoundPlayer::clear()
{
    for (const auto& entry : m_loopAudioByAction)
        game::AudioManager::getInstance()->setSFXLoop(entry.second, false);
    m_loopAudioByAction.clear();
}

NS_MG_END

#endif
