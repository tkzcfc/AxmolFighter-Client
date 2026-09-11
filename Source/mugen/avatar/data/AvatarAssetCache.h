#pragma once

#include "AniData.h"
#include "CombatTimeline.h"
#include "MotionMap.h"

NS_MG_BEGIN

class AvatarAssetCache : public Object
{
public:
    typedef Object Super;

public:
    AvatarAssetCache();
    virtual ~AvatarAssetCache();

    static AvatarAssetCache* getInstance();
    static void destroy();

    void clear();

    void addSearchPath(const std::string& path);
    void clearSearchPaths();

    bool load(const std::string& path);
    bool isLoaded() const;
    bool saveToFile(const std::string& path) const;

    const AniData* getAniData(const std::string& path) const;
    const CombatTimeline* getCombatTimeline(const std::string& path) const;
    const MotionMap* getMotionMap(const std::string& path) const;

    void putAniData(const std::string& key, const AniData& value);
    void putCombatTimeline(const std::string& key, const CombatTimeline& value);
    void putMotionMap(const std::string& key, const MotionMap& value);

private:
    template <typename T>
    const T* findByPath(const std::unordered_map<std::string, T>& map, const std::string& path) const;

    void serializeCustomImpl(ByteBuffer& byteBuffer) const;
    bool deserializeCustomImpl(ByteBuffer& byteBuffer);

    std::vector<std::string> m_searchPaths;
    bool m_isLoaded = false;

#if defined(OLUA_AUTOCONF)
public:
#else
private:
#endif
    std::unordered_map<std::string, AniData> aniDatas;
    std::unordered_map<std::string, CombatTimeline> combatTimelines;
    std::unordered_map<std::string, MotionMap> motionMaps;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl, aniDatas, combatTimelines, motionMaps)
};

NS_MG_END
