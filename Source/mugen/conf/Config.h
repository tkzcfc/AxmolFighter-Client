#pragma once
#include "mugen/conf/TableConfig.h"

NS_MG_BEGIN

class Config : public Object
{
public:
    typedef Object Super;

public:
    Config();
    virtual ~Config();

    static Config* getInstance();
    static void destroyInstance();

    void clearConfig();

    bool loadConfig(const std::string& path);

    bool saveToFile(const std::string& path) const;

    const TownConfig* getTownConfigById(int32_t id) const;
    const CampConfig* getCampConfigById(int32_t id) const;
    const StageConfig* getStageConfigById(int32_t id) const;
    const CopyConfig* getCopyConfigById(int32_t id) const;
    const ChapterConfig* getChapterConfigById(int32_t id) const;
    const NpcConfig* getNpcConfigById(int32_t id) const;
    const PortalConfig* getPortalConfigById(int32_t id) const;
    const RoomConfig* getRoomConfigById(int32_t id) const;

    const SkillAttackConfig* getSkillAttackConfigById(int32_t id) const;
    const ActionAttackConfig* getActionAttackConfigById(int32_t id) const;
    const SkillHitTableConfig* getSkillHitTableConfigById(int32_t id) const;
    const RoleConfig* getRoleConfigById(int32_t id) const;
    const ResSpineConfig* getResSpineConfigById(int32_t id) const;
    const EquipConfig* getEquipConfigById(int32_t id) const;
    const FashionConfig* getFashionConfigById(int32_t id) const;
    const FashionSuitConfig* getFashionSuitConfigById(int32_t id) const;
    const ItemBaseConfig* getItemBaseConfigById(int32_t id) const;
    const ResFashionConfig* getResFashionConfigById(int32_t id) const;

    const BehaviorTemplateConfig* getBehaviorTemplateConfigById(int32_t id) const;
    const DisplacementConfig* getDisplacementConfigById(int32_t id) const;
    const CameraConfig* getCameraConfigById(int32_t id) const;
    const EffectConfig* getEffectConfigById(int32_t id) const;
    const BuffConfig* getBuffConfigById(int32_t id) const;
    const BuffRuleConfig* getBuffRuleConfigById(int32_t id) const;
    const AiConfig* getAiConfigById(int32_t id) const;
    const SkillAiConfig* getSkillAiConfigById(int32_t id) const;
    const SkillHurtConfig* getSkillHurtConfigById(int32_t id) const;
    const SkillActivationOverlayConfig* getSkillActivationOverlayById(int32_t skillId) const;
    const AttributeTemplateConfig* getAttributeTemplateConfigById(int32_t id) const;

    const ResSoundConfig* getResSoundById(int32_t id) const;
    const SoundUiConfig* getSoundUiByViewName(const std::string& viewName) const;
    const SoundSpineConfig* getSoundSpineById(int32_t id) const;
    const SoundSpineBgmConfig* getSoundSpineBgmById(int32_t id) const;
    const SoundMapSpineConfig* getSoundMapSpineById(int32_t id) const;
    const SoundSendMessageConfig* getSoundSendMessageById(int32_t id) const;
    const SoundTalkConfig* getSoundTalkById(int32_t id) const;

    std::unordered_map<int32_t, TownConfig> townConfigs;
    std::unordered_map<int32_t, CampConfig> campConfigs;
    std::unordered_map<int32_t, StageConfig> stageConfigs;
    std::unordered_map<int32_t, CopyConfig> copyConfigs;
    std::unordered_map<int32_t, ChapterConfig> chapterConfigs;
    std::unordered_map<int32_t, NpcConfig> npcConfigs;
    std::unordered_map<int32_t, PortalConfig> portalConfigs;
    std::unordered_map<int32_t, RoomConfig> roomConfigs;

    std::unordered_map<int32_t, SkillAttackConfig> skillAttackConfigs;
    std::unordered_map<int32_t, ActionAttackConfig> actionAttackConfigs;
    std::unordered_map<int32_t, SkillHitTableConfig> skillHitTableConfigs;
    std::unordered_map<int32_t, AttributeTemplateConfig> attributeTemplateConfigs;
    std::unordered_map<int32_t, ResSpineConfig> resSpineConfigs;
    std::unordered_map<int32_t, EquipConfig> equipConfigs;
    std::unordered_map<int32_t, FashionConfig> fashionConfigs;
    std::unordered_map<int32_t, FashionSuitConfig> fashionSuitConfigs;
    std::unordered_map<int32_t, ItemBaseConfig> itemBaseConfigs;
    std::unordered_map<int32_t, ResFashionConfig> resFashionConfigs;
    std::unordered_map<int32_t, RoleConfig> roleConfigs;

    std::unordered_map<int32_t, BehaviorTemplateConfig> behaviorTemplateConfigs;
    std::unordered_map<int32_t, DisplacementConfig> displacementConfigs;
    std::unordered_map<int32_t, CameraConfig> cameraConfigs;
    std::unordered_map<int32_t, EffectConfig> effectConfigs;
    std::unordered_map<int32_t, BuffConfig> buffConfigs;
    std::unordered_map<int32_t, BuffRuleConfig> buffRuleConfigs;
    std::unordered_map<int32_t, AiConfig> aiConfigs;
    std::unordered_map<int32_t, SkillAiConfig> skillAiConfigs;
    std::unordered_map<int32_t, SkillHurtConfig> skillHurtConfigs;
    std::unordered_map<int32_t, SkillActivationOverlayConfig> skillActivationOverlayConfigs;

    std::unordered_map<int32_t, ResSoundConfig> resSoundConfigs;
    std::unordered_map<std::string, SoundUiConfig> soundUiConfigs;
    std::unordered_map<int32_t, SoundSpineConfig> soundSpineConfigs;
    std::unordered_map<int32_t, SoundSpineBgmConfig> soundSpineBgmConfigs;
    std::unordered_map<int32_t, SoundMapSpineConfig> soundMapSpineConfigs;
    std::unordered_map<int32_t, SoundSendMessageConfig> soundSendMessageConfigs;
    std::unordered_map<int32_t, SoundTalkConfig> soundTalkConfigs;

public:

    MG_DEFINE_SERIALIZABLE(townConfigs,
                           campConfigs,
                           stageConfigs,
                           copyConfigs,
                           chapterConfigs,
                           npcConfigs,
                           portalConfigs,
                           roomConfigs,
                           skillAttackConfigs,
                           actionAttackConfigs,
                           skillHitTableConfigs,
                           attributeTemplateConfigs,
                           resSpineConfigs,
                           equipConfigs,
                           fashionConfigs,
                           fashionSuitConfigs,
                           itemBaseConfigs,
                           resFashionConfigs,
                           roleConfigs,
                           behaviorTemplateConfigs,
                           displacementConfigs,
                           cameraConfigs,
                           effectConfigs,
                           buffConfigs,
                           buffRuleConfigs,
                           aiConfigs,
                           skillAiConfigs,
                           skillHurtConfigs,
                           skillActivationOverlayConfigs,
                           resSoundConfigs,
                           soundUiConfigs,
                           soundSpineConfigs,
                           soundSpineBgmConfigs,
                           soundMapSpineConfigs,
                           soundSendMessageConfigs,
                           soundTalkConfigs);
};

NS_MG_END
