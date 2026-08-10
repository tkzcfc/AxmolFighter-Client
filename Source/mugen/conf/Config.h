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

    // 根据 id 获取城镇配置
    const TownConfig* getTownConfigById(int32_t id) const;
    // 根据 id 获取营地配置
    const CampConfig* getCampConfigById(int32_t id) const;
    // 根据 id 获取关卡配置
    const StageConfig* getStageConfigById(int32_t id) const;
    // 根据 id 获取副本配置
    const CopyConfig* getCopyConfigById(int32_t id) const;
    // 根据 id 获取章节配置
    const ChapterConfig* getChapterConfigById(int32_t id) const;
    // 获取全部章节配置（只读）
    const std::unordered_map<int32_t, ChapterConfig>& getChapterConfigs() const;
    // 根据 id 获取 NPC 配置
    const NpcConfig* getNpcConfigById(int32_t id) const;
    // 根据 id 获取传送门配置
    const PortalConfig* getPortalConfigById(int32_t id) const;
    // 根据 id 获取房间配置
    const RoomConfig* getRoomConfigById(int32_t id) const;

    // 根据 id 获取技能攻击配置
    const SkillAttackConfig* getSkillAttackConfigById(int32_t id) const;
    // 根据 id 获取动作攻击配置
    const ActionAttackConfig* getActionAttackConfigById(int32_t id) const;
    // 根据 id 获取技能受击表配置
    const SkillHitTableConfig* getSkillHitTableConfigById(int32_t id) const;
    // 根据 id 获取角色配置
    const RoleConfig* getRoleConfigById(int32_t id) const;
    // 根据 id 获取 Spine 资源配置
    const ResSpineConfig* getResSpineConfigById(int32_t id) const;
    // 根据 id 获取装备配置
    const EquipConfig* getEquipConfigById(int32_t id) const;
    // 根据 id 获取时装配置
    const FashionConfig* getFashionConfigById(int32_t id) const;
    // 根据 id 获取时装套装配置
    const FashionSuitConfig* getFashionSuitConfigById(int32_t id) const;
    // 根据 id 获取道具基础配置
    const ItemBaseConfig* getItemBaseConfigById(int32_t id) const;
    // 根据 id 获取时装资源配置
    const ResFashionConfig* getResFashionConfigById(int32_t id) const;

    // 根据 id 获取行为模板配置
    const BehaviorTemplateConfig* getBehaviorTemplateConfigById(int32_t id) const;
    // 根据 id 获取位移配置
    const DisplacementConfig* getDisplacementConfigById(int32_t id) const;
    // 根据 id 获取镜头配置
    const CameraConfig* getCameraConfigById(int32_t id) const;
    // 根据 id 获取特效配置
    const EffectConfig* getEffectConfigById(int32_t id) const;
    // 根据 id 获取 Buff 配置
    const BuffConfig* getBuffConfigById(int32_t id) const;
    // 根据 id 获取 Buff 规则配置
    const BuffRuleConfig* getBuffRuleConfigById(int32_t id) const;
    // 根据 id 获取 AI 配置
    const AiConfig* getAiConfigById(int32_t id) const;
    // 根据 id 获取技能 AI 配置
    const SkillAiConfig* getSkillAiConfigById(int32_t id) const;
    // 根据 id 获取技能伤害配置
    const SkillHurtConfig* getSkillHurtConfigById(int32_t id) const;
    // 根据 skillId 获取技能激活覆盖配置
    const SkillActivationOverlayConfig* getSkillActivationOverlayById(int32_t skillId) const;
    // 根据 id 获取属性模板配置
    const AttributeTemplateConfig* getAttributeTemplateConfigById(int32_t id) const;

    // 根据 id 获取音效资源配置
    const ResSoundConfig* getResSoundById(int32_t id) const;
    // 根据 viewName 获取 UI 音效配置
    const SoundUiConfig* getSoundUiByViewName(const std::string& viewName) const;
    // 根据 id 获取 Spine 音效配置
    const SoundSpineConfig* getSoundSpineById(int32_t id) const;
    // 根据 id 获取 Spine BGM 配置
    const SoundSpineBgmConfig* getSoundSpineBgmById(int32_t id) const;
    // 根据 id 获取地图 Spine 音效配置
    const SoundMapSpineConfig* getSoundMapSpineById(int32_t id) const;
    // 根据 id 获取发送消息音效配置
    const SoundSendMessageConfig* getSoundSendMessageById(int32_t id) const;
    // 根据 id 获取对话音效配置
    const SoundTalkConfig* getSoundTalkById(int32_t id) const;

#if defined(OLUA_AUTOCONF)
    // 允许 config_convert 工具直接访问私有成员修改配置
public:
#else
    // 运行时禁止直接访问私有成员，避免误操作
private:
#endif
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
