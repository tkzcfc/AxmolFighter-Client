#include "MotionMap.h"

#include "JsonHelper.h"
#include "mugen/core/StdC.h"
#include "mugen/core/io/FileUtils.h"

NS_MG_BEGIN

namespace
{

bool parseEntryType(const std::string& typeStr, MotionEntryType& outType)
{
    if (typeStr == "ani")
    {
        outType = MotionEntryType::kAni;
        return true;
    }
    if (typeStr == "spine")
    {
        outType = MotionEntryType::kSpine;
        return true;
    }
    return false;
}

}  // namespace

bool MotionMap::load(const std::string& path)
{
    m_motions.clear();
    m_nameToIndex.clear();
    m_sourcePath = path;

    const std::string jsonText = io::getStringFromFile(path);
    if (jsonText.empty())
    {
        MG_LOG_W("MotionMap: failed to read '{}'", path);
        return false;
    }

    JsonHelper helper(path);
    rapidjson::Document doc;
    if (!helper.parse(jsonText, doc))
        return false;
    if (!helper.requireObject(doc))
        return false;

    const rapidjson::Value* motions = nullptr;
    if (!helper.requireMemberArray(doc, "motions", motions))
        return false;

    helper.enterKey("motions");
    m_motions.reserve(motions->Size());
    for (rapidjson::SizeType i = 0; i < motions->Size(); ++i)
    {
        helper.enterIndex(i);
        const rapidjson::Value& motionValue = (*motions)[i];
        if (!helper.requireObject(motionValue))
            return false;

        std::string name;
        if (!helper.requireString(motionValue, "name", name))
            return false;

        if (m_nameToIndex.find(name) != m_nameToIndex.end())
        {
            MG_LOG_W("MotionMap: duplicate motion name '{}', keeping first", name);
            helper.leave();
            continue;
        }

        MotionDefinition motion;
        motion.setName(name);

        const rapidjson::Value* animations = nullptr;
        if (!helper.requireMemberArray(motionValue, "animations", animations))
            return false;

        helper.enterKey("animations");
        std::vector<MotionEntry> entries;
        entries.reserve(animations->Size());
        std::unordered_map<std::string, size_t> entryIds;
        for (rapidjson::SizeType j = 0; j < animations->Size(); ++j)
        {
            helper.enterIndex(j);
            const rapidjson::Value& entryValue = (*animations)[j];
            if (!helper.requireObject(entryValue))
                return false;

            MotionEntry entry;
            std::string id;
            if (!helper.requireString(entryValue, "id", id))
                return false;

            if (entryIds.find(id) != entryIds.end())
            {
                MG_LOG_W("MotionMap: duplicate entry id '{}' in motion '{}', keeping first", id, name);
                helper.leave();
                continue;
            }

            std::string typeStr;
            if (!helper.requireString(entryValue, "type", typeStr))
                return false;

            MotionEntryType entryType = MotionEntryType::kAni;
            if (!parseEntryType(typeStr, entryType))
            {
                helper.enterKey("type");
                helper.fail("expected \"ani\" or \"spine\"");
                helper.leave();
                return false;
            }

            std::string source;
            if (!helper.requireString(entryValue, "source", source))
                return false;
            if (source.empty())
            {
                helper.enterKey("source");
                helper.fail("must be non-empty");
                helper.leave();
                return false;
            }

            entry.setId(id);
            entry.setType(entryType);
            entry.setSource(source);
            entry.setBoxPath(helper.getString(entryValue, "box"));
            entryIds[id] = entries.size();
            entries.push_back(std::move(entry));
            helper.leave();
        }
        helper.leave();

        motion.setEntries(entries);
        m_nameToIndex[name] = m_motions.size();
        m_motions.push_back(std::move(motion));
        helper.leave();
    }
    helper.leave();

    return helper.ok();
}

const MotionDefinition* MotionMap::findMotion(const std::string& name) const
{
    const auto it = m_nameToIndex.find(name);
    if (it == m_nameToIndex.end())
        return nullptr;
    return &m_motions[it->second];
}

const MotionEntry* MotionMap::findEntry(const std::string& motionName, const std::string& entryId) const
{
    const MotionDefinition* motion = findMotion(motionName);
    if (!motion)
        return nullptr;
    for (const MotionEntry& entry : motion->getEntries())
    {
        if (entry.getId() == entryId)
            return &entry;
    }
    return nullptr;
}

const MotionEntry* MotionMap::entryAt(const std::string& motionName, size_t index) const
{
    const MotionDefinition* motion = findMotion(motionName);
    if (!motion || index >= motion->getEntries().size())
        return nullptr;
    return &motion->getEntries()[index];
}

NS_MG_END
