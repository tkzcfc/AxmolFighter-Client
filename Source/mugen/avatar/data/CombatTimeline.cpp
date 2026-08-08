#include "CombatTimeline.h"

#include "JsonHelper.h"
#include "mugen/core/StdC.h"
#include "mugen/core/io/FileUtils.h"

#include <algorithm>

NS_MG_BEGIN

namespace
{

bool parseVec3i(JsonHelper& helper, const rapidjson::Value& object, const char* key, Vector3i& out)
{
    helper.enterKey(key);
    const rapidjson::Value* value = helper.member(object, key);
    if (!value)
    {
        helper.fail("missing required field");
        helper.leave();
        return false;
    }
    if (!value->IsObject())
    {
        helper.fail("expected object");
        helper.leave();
        return false;
    }
    if (!helper.requireInt(*value, "x", out.x) || !helper.requireInt(*value, "y", out.y) ||
        !helper.requireInt(*value, "z", out.z))
    {
        helper.leave();
        return false;
    }
    helper.leave();
    return true;
}

bool parseKey(JsonHelper& helper, const rapidjson::Value& keyValue, CombatKey& outKey)
{
    if (!helper.requireObject(keyValue))
        return false;

    int timeMs = 0;
    if (!helper.requireInt(keyValue, "timeMs", timeMs))
        return false;
    outKey.setTimeMs(timeMs);

    const rapidjson::Value* boxValue = helper.member(keyValue, "box");
    if (!boxValue)
    {
        helper.enterKey("box");
        helper.fail("missing required field");
        helper.leave();
        return false;
    }

    if (boxValue->IsNull())
    {
        outKey.setHasBox(false);
        return true;
    }

    helper.enterKey("box");
    if (!boxValue->IsObject())
    {
        helper.fail("expected object or null");
        helper.leave();
        return false;
    }

    DamageBox box;
    if (!parseVec3i(helper, *boxValue, "pos", box.pos) || !parseVec3i(helper, *boxValue, "size", box.size))
    {
        helper.leave();
        return false;
    }
    outKey.setBox(box);
    outKey.setHasBox(true);
    helper.leave();
    return helper.ok();
}

bool parseTrack(JsonHelper& helper, const rapidjson::Value& trackValue, CombatTrack& outTrack)
{
    if (!helper.requireObject(trackValue))
        return false;

    std::string name;
    if (!helper.requireString(trackValue, "name", name))
        return false;
    outTrack.setName(name);

    std::string kind;
    if (!helper.requireString(trackValue, "kind", kind))
        return false;
    if (kind == "attack")
        outTrack.setKind(CombatTrackKind::Attack);
    else if (kind == "damage")
        outTrack.setKind(CombatTrackKind::Damage);
    else if (kind == "hitbox")
        outTrack.setKind(CombatTrackKind::Hitbox);
    else
    {
        helper.enterKey("kind");
        helper.fail("expected \"attack\", \"damage\" or \"hitbox\"");
        helper.leave();
        return false;
    }

    const rapidjson::Value* keys = nullptr;
    if (!helper.requireMemberArray(trackValue, "keys", keys))
        return false;

    helper.enterKey("keys");
    std::vector<CombatKey> parsed;
    parsed.reserve(keys->Size());
    bool sorted  = true;
    int lastTime = -1;
    for (rapidjson::SizeType i = 0; i < keys->Size(); ++i)
    {
        helper.enterIndex(i);
        CombatKey key;
        if (!parseKey(helper, (*keys)[i], key))
            return false;
        if (i > 0 && key.getTimeMs() < lastTime)
            sorted = false;
        lastTime = key.getTimeMs();
        parsed.push_back(std::move(key));
        helper.leave();
    }
    helper.leave();

    if (!sorted)
    {
        // 正常情况下，编辑器导出的关键帧列表应该是按时间升序排列的，如果不是，说明编辑器有 bug或者用户手动修改了
        // JSON，这里先 log 警告一下，然后再排序
        MG_LOG_W("CombatTimeline: keys not sorted by timeMs on track '{}', sorting", name);
        std::sort(parsed.begin(), parsed.end(),
                  [](const CombatKey& a, const CombatKey& b) { return a.getTimeMs() < b.getTimeMs(); });
    }

    // 检查一下关键帧列表,同一个时间点上只允许有一个关键帧,如果有多个则打印错误并返回失败
    for (size_t i = 1; i < parsed.size(); ++i)
    {
        if (parsed[i].getTimeMs() == parsed[i - 1].getTimeMs())
        {
            MG_LOG_E("CombatTimeline: duplicate keys at timeMs={} on track '{}'", parsed[i].getTimeMs(), name);
            return false;
        }
    }

    outTrack.setKeys(parsed);
    return helper.ok();
}

bool parseEvent(JsonHelper& helper, const rapidjson::Value& eventValue, CombatEvent& outEvent)
{
    if (!helper.requireObject(eventValue))
        return false;

    int timeMs = 0;
    if (!helper.requireInt(eventValue, "timeMs", timeMs))
        return false;
    outEvent.setTimeMs(timeMs);

    std::string type;
    if (!helper.requireString(eventValue, "type", type))
        return false;
    outEvent.setType(type);
    outEvent.setValue(helper.getString(eventValue, "value"));
    return true;
}

}  // namespace

bool CombatTimeline::load(const std::string& path)
{
    m_duration = 0;
    m_tracks.clear();
    m_events.clear();
    m_sourcePath = path;

    const std::string jsonText = io::getStringFromFile(path);
    if (jsonText.empty())
    {
        MG_LOG_W("CombatTimeline: failed to read '{}'", path);
        return false;
    }

    JsonHelper helper(path);
    rapidjson::Document doc;
    if (!helper.parse(jsonText, doc))
        return false;
    if (!helper.requireObject(doc))
        return false;

    // preview字段,编辑器专用,这儿直接忽略
    // auto preview = helper.member(doc, "preview");

    if (!helper.requireInt(doc, "duration", m_duration))
        return false;

    const rapidjson::Value* tracks = nullptr;
    if (!helper.requireMemberArray(doc, "tracks", tracks))
        return false;

    helper.enterKey("tracks");
    m_tracks.reserve(tracks->Size());
    for (rapidjson::SizeType i = 0; i < tracks->Size(); ++i)
    {
        helper.enterIndex(i);
        CombatTrack track;
        if (!parseTrack(helper, (*tracks)[i], track))
            return false;
        m_tracks.push_back(std::move(track));
        helper.leave();
    }
    helper.leave();

    if (const rapidjson::Value* events = helper.member(doc, "events"))
    {
        helper.enterKey("events");
        if (!events->IsArray())
        {
            helper.fail("expected array");
            return false;
        }
        m_events.reserve(events->Size());
        for (rapidjson::SizeType i = 0; i < events->Size(); ++i)
        {
            helper.enterIndex(i);
            CombatEvent event;
            if (!parseEvent(helper, (*events)[i], event))
                return false;
            m_events.push_back(std::move(event));
            helper.leave();
        }
        helper.leave();

        // 这儿必须使用稳定排序，因为事件列表可能有多个事件在同一时间点触发
        // 编辑器导出的事件的顺序是有意义的，不能被打乱
        std::stable_sort(m_events.begin(), m_events.end(),
                         [](const CombatEvent& a, const CombatEvent& b) { return a.getTimeMs() < b.getTimeMs(); });
    }

    return helper.ok();
}

const CombatKey* CombatTimeline::keyAtOrBefore(const CombatTrack& track, int timeMs)
{
    const std::vector<CombatKey>& keys = track.getKeys();
    const CombatKey* best              = nullptr;
    for (const CombatKey& key : keys)
    {
        if (key.getTimeMs() > timeMs)
            break;
        best = &key;
    }
    return best;
}

void CombatTimeline::boxesAt(int timeMs,
                             std::vector<const DamageBox*>& outAttack,
                             std::vector<const DamageBox*>& outDamage) const
{
    for (const CombatTrack& track : m_tracks)
    {
        const CombatKey* key = keyAtOrBefore(track, timeMs);
        if (!key || !key->isHasBox())
            continue;

        const DamageBox* box = &key->getBox();
        if (track.getKind() == CombatTrackKind::Attack)
        {
            outAttack.push_back(box);
        }
        else if (track.getKind() == CombatTrackKind::Damage)
        {
            outDamage.push_back(box);
        }
        else  // Hitbox：配置侧动态区分，采样时同时提供攻/受
        {
            outAttack.push_back(box);
            outDamage.push_back(box);
        }
    }
}

void CombatTimeline::eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const
{
    if (t1 <= t0)
        return;

    for (const CombatEvent& event : m_events)
    {
        const int t = event.getTimeMs();
        if (t >= t1)
            break;
        if (t >= t0)
            out.push_back(&event);
    }
}

NS_MG_END
