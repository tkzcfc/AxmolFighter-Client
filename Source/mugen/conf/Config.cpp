#include "Config.h"
#include "mugen/core/io/FileUtils.h"
#include "mugen/core/serialize/ByteBuffer.h"

NS_MG_BEGIN

namespace
{
static Config* s_instance = nullptr;

template <typename T>
const T* findByPath(const std::unordered_map<std::string, T>& map, const std::string& path, const char* typeName)
{
    auto iter = map.find(path);
    if (iter != map.end())
        return &iter->second;
    MG_LOG_E("Config miss [{}]: {}", typeName, path);
    return nullptr;
}

template <typename T>
void setSourcePath(T& configMap)
{
    for (auto& pair : configMap)
    {
        pair.second.sourcePath = pair.first;
    }
}

}  // namespace

Config::Config() {}
Config::~Config() {}

Config* Config::getInstance()
{
    if (!s_instance)
        s_instance = new (std::nothrow) Config();
    return s_instance;
}

void Config::destroyInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

void Config::clearConfig()
{
    mapConfigs.clear();
    mapIdMap.clear();
    townConfigs.clear();
    campConfigs.clear();
    stageConfigs.clear();
    copyConfigs.clear();
    chapterConfigs.clear();
    npcConfigs.clear();
    portalConfigs.clear();
    roomConfigs.clear();
    mapDataConfigs.clear();
    skillAttackConfigs.clear();
    actionAttackConfigs.clear();
    skillHitTableConfigs.clear();
    attributeTemplateConfigs.clear();
    resSpineConfigs.clear();
    equipConfigs.clear();
    fashionConfigs.clear();
    fashionSuitConfigs.clear();
    itemBaseConfigs.clear();
    resFashionConfigs.clear();
    roleConfigs.clear();
    behaviorTemplateConfigs.clear();
    displacementConfigs.clear();
    cameraConfigs.clear();
    effectConfigs.clear();
    buffConfigs.clear();
    aiConfigs.clear();
    skillHurtConfigs.clear();
    skillActivationOverlayConfigs.clear();
    resSoundConfigs.clear();
    soundUiConfigs.clear();
    soundSpineConfigs.clear();
    soundSpineBgmConfigs.clear();
    soundMapSpineConfigs.clear();
    soundSendMessageConfigs.clear();
    soundTalkConfigs.clear();
}

bool Config::loadConfig(const std::string& path)
{
    clearConfig();

    size_t size  = 0;
    uint8_t* raw = io::getFileData(path, size);
    if (raw == nullptr || size == 0)
    {
        MG_LOG_E("Failed to load config: {}", path);
        return false;
    }

    ByteBuffer buffer;
    buffer.fastSet(raw, static_cast<uint32_t>(size));
    if (!deserialize(buffer))
    {
        MG_LOG_E("Config deserialize failed: {}", path);
        clearConfig();
        return false;
    }

    // 设置配置的 sourcePath 字段为其在 map 中的 key
    setSourcePath(mapConfigs);
    MG_LOG_I(
        "Loaded config: map={} mapIds={} "
        "town={} camp={} stage={} copy={} chapter={} npc={} portal={} room={} mapData={} "
        "skillAtk={} actionAtk={} roles={} resSpine={} behavior={} displace={} effect={} buff={} ai={} "
        "resSound={} soundUi={}",
        mapConfigs.size(), mapIdMap.size(), townConfigs.size(), campConfigs.size(), stageConfigs.size(),
        copyConfigs.size(), chapterConfigs.size(), npcConfigs.size(), portalConfigs.size(), roomConfigs.size(),
        mapDataConfigs.size(), skillAttackConfigs.size(), actionAttackConfigs.size(), roleConfigs.size(),
        resSpineConfigs.size(), behaviorTemplateConfigs.size(), displacementConfigs.size(), effectConfigs.size(),
        buffConfigs.size(), aiConfigs.size(), resSoundConfigs.size(), soundUiConfigs.size());
    return true;
}

bool Config::saveToFile(const std::string& path) const
{
    ByteBuffer buffer;
    serialize(buffer);
    buffer.writeFinish();
    return io::writeDataToFile(reinterpret_cast<const char*>(buffer.data()), buffer.len(), path);
}

const MapConfig* Config::getMapConfig(const std::string& path) const
{
    return findByPath(mapConfigs, path, "MapConfig");
}

const MapConfig* Config::getMapConfigById(int32_t id) const
{
    auto it = mapIdMap.find(id);
    if (it == mapIdMap.end())
        return nullptr;
    return getMapConfig(it->second);
}

namespace
{
template <typename T>
const T* findById(const std::unordered_map<int32_t, T>& map, int32_t id, const char* typeName)
{
    auto it = map.find(id);
    if (it == map.end())
    {
        MG_LOG_W("Config miss [{}] id={}", typeName, id);
        return nullptr;
    }
    return &it->second;
}
}  // namespace

const TownConfig* Config::getTownConfigById(int32_t id) const
{
    return findById(townConfigs, id, "TownConfig");
}

const CampConfig* Config::getCampConfigById(int32_t id) const
{
    return findById(campConfigs, id, "CampConfig");
}

const StageConfig* Config::getStageConfigById(int32_t id) const
{
    return findById(stageConfigs, id, "StageConfig");
}

const CopyConfig* Config::getCopyConfigById(int32_t id) const
{
    return findById(copyConfigs, id, "CopyConfig");
}

const ChapterConfig* Config::getChapterConfigById(int32_t id) const
{
    return findById(chapterConfigs, id, "ChapterConfig");
}

const NpcConfig* Config::getNpcConfigById(int32_t id) const
{
    return findById(npcConfigs, id, "NpcConfig");
}

const PortalConfig* Config::getPortalConfigById(int32_t id) const
{
    return findById(portalConfigs, id, "PortalConfig");
}

const RoomConfig* Config::getRoomConfigById(int32_t id) const
{
    return findById(roomConfigs, id, "RoomConfig");
}

const MapDataConfig* Config::getMapDataConfigById(int32_t id) const
{
    return findById(mapDataConfigs, id, "MapDataConfig");
}

const SkillAttackConfig* Config::getSkillAttackConfigById(int32_t id) const
{
    return findById(skillAttackConfigs, id, "SkillAttackConfig");
}

const ActionAttackConfig* Config::getActionAttackConfigById(int32_t id) const
{
    return findById(actionAttackConfigs, id, "ActionAttackConfig");
}

const SkillHitTableConfig* Config::getSkillHitTableConfigById(int32_t id) const
{
    return findById(skillHitTableConfigs, id, "SkillHitTableConfig");
}

const RoleConfig* Config::getRoleConfigById(int32_t id) const
{
    return findById(roleConfigs, id, "RoleConfig");
}

const BehaviorTemplateConfig* Config::getBehaviorTemplateConfigById(int32_t id) const
{
    return findById(behaviorTemplateConfigs, id, "BehaviorTemplateConfig");
}

const DisplacementConfig* Config::getDisplacementConfigById(int32_t id) const
{
    return findById(displacementConfigs, id, "DisplacementConfig");
}

const CameraConfig* Config::getCameraConfigById(int32_t id) const
{
    return findById(cameraConfigs, id, "CameraConfig");
}

const EffectConfig* Config::getEffectConfigById(int32_t id) const
{
    return findById(effectConfigs, id, "EffectConfig");
}

const BuffConfig* Config::getBuffConfigById(int32_t id) const
{
    return findById(buffConfigs, id, "BuffConfig");
}

const AiConfig* Config::getAiConfigById(int32_t id) const
{
    return findById(aiConfigs, id, "AiConfig");
}

const SkillHurtConfig* Config::getSkillHurtConfigById(int32_t id) const
{
    return findById(skillHurtConfigs, id, "SkillHurtConfig");
}

const SkillActivationOverlayConfig* Config::getSkillActivationOverlayById(int32_t skillId) const
{
    auto it = skillActivationOverlayConfigs.find(skillId);
    if (it == skillActivationOverlayConfigs.end())
        return nullptr;
    return &it->second;
}

const AttributeTemplateConfig* Config::getAttributeTemplateConfigById(int32_t id) const
{
    return findById(attributeTemplateConfigs, id, "AttributeTemplateConfig");
}

const ResSpineConfig* Config::getResSpineConfigById(int32_t id) const
{
    return findById(resSpineConfigs, id, "ResSpineConfig");
}

const EquipConfig* Config::getEquipConfigById(int32_t id) const
{
    return findById(equipConfigs, id, "EquipConfig");
}

const FashionConfig* Config::getFashionConfigById(int32_t id) const
{
    return findById(fashionConfigs, id, "FashionConfig");
}

const FashionSuitConfig* Config::getFashionSuitConfigById(int32_t id) const
{
    return findById(fashionSuitConfigs, id, "FashionSuitConfig");
}

const ItemBaseConfig* Config::getItemBaseConfigById(int32_t id) const
{
    return findById(itemBaseConfigs, id, "ItemBaseConfig");
}

const ResFashionConfig* Config::getResFashionConfigById(int32_t id) const
{
    return findById(resFashionConfigs, id, "ResFashionConfig");
}

const ResSoundConfig* Config::getResSoundById(int32_t id) const
{
    return findById(resSoundConfigs, id, "ResSoundConfig");
}

const SoundUiConfig* Config::getSoundUiByViewName(const std::string& viewName) const
{
    auto it = soundUiConfigs.find(viewName);
    if (it == soundUiConfigs.end())
    {
        MG_LOG_W("Config miss [SoundUiConfig] viewName={}", viewName);
        return nullptr;
    }
    return &it->second;
}

const SoundSpineConfig* Config::getSoundSpineById(int32_t id) const
{
    return findById(soundSpineConfigs, id, "SoundSpineConfig");
}

const SoundSpineBgmConfig* Config::getSoundSpineBgmById(int32_t id) const
{
    return findById(soundSpineBgmConfigs, id, "SoundSpineBgmConfig");
}

const SoundMapSpineConfig* Config::getSoundMapSpineById(int32_t id) const
{
    return findById(soundMapSpineConfigs, id, "SoundMapSpineConfig");
}

const SoundSendMessageConfig* Config::getSoundSendMessageById(int32_t id) const
{
    return findById(soundSendMessageConfigs, id, "SoundSendMessageConfig");
}

const SoundTalkConfig* Config::getSoundTalkById(int32_t id) const
{
    return findById(soundTalkConfigs, id, "SoundTalkConfig");
}

const MapConfig* Config::getOrCreateMapConfigByKey(const std::string& mapKey)
{
    if (mapKey.empty())
        return nullptr;

    const std::string runtimeKey = std::string("runtime/map/") + mapKey;
    auto it                      = mapConfigs.find(runtimeKey);
    if (it != mapConfigs.end())
        return &it->second;

    MapConfig cfg;
    cfg.sourcePath = runtimeKey;
    cfg.layerFile  = std::string("mugen/map/") + mapKey + ".layer";
    cfg.name       = mapKey;
    // 默认尺寸；客户端渲染时可被 layer 实际内容覆盖观感，物理范围用此 scope
    cfg.mapWidth     = 1920;
    cfg.mapHeight    = 1080;
    cfg.scope.x      = 0;
    cfg.scope.y      = 0;
    cfg.scope.width  = cfg.mapWidth;
    cfg.scope.height = cfg.mapHeight;
    cfg.spawnPoints.push_back(Vector2i{cfg.mapWidth / 2, cfg.mapHeight / 2});

    mapConfigs[runtimeKey]            = std::move(cfg);
    mapConfigs[runtimeKey].sourcePath = runtimeKey;
    return &mapConfigs[runtimeKey];
}

NS_MG_END
