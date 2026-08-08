#include "AniData.h"

#include "JsonHelper.h"
#include "mugen/core/io/FileUtils.h"

NS_MG_BEGIN

namespace
{

bool parseFrame(JsonHelper& helper, const rapidjson::Value& frameValue, AniFrame& outFrame)
{
    if (!helper.requireObject(frameValue))
        return false;

    // 持续时间
    int delay = 0;
    if (!helper.requireInt(frameValue, "delay", delay))
        return false;
    outFrame.setDelay(delay);

    // 图片路径
    std::string imagePath = helper.getString(frameValue, "image");
    if (imagePath.empty())
    {
        helper.enterKey("image");
        helper.fail("missing image");
        helper.leave();
        return false;
    }
    outFrame.setImagePath(imagePath);

    float offsetX  = 0.0f;
    float offsetY  = 0.0f;
    float anchorX  = 0.5f;
    float anchorY  = 0.5f;
    float scale    = 1.0f;
    float rotation = 0.0f;

    if (const rapidjson::Value* transform = helper.member(frameValue, "transform"))
    {
        helper.enterKey("transform");
        if (!transform->IsObject())
        {
            helper.fail("expected object");
            helper.leave();
            return false;
        }

        if (const rapidjson::Value* offset = helper.member(*transform, "offset"))
        {
            helper.enterKey("offset");
            if (!offset->IsObject())
            {
                helper.fail("expected object");
                helper.leave();
                helper.leave();
                return false;
            }
            offsetX = helper.getFloat(*offset, "x", 0.0f);
            offsetY = helper.getFloat(*offset, "y", 0.0f);
            helper.leave();
        }

        if (const rapidjson::Value* anchor = helper.member(*transform, "anchor"))
        {
            helper.enterKey("anchor");
            if (!anchor->IsObject())
            {
                helper.fail("expected object");
                helper.leave();
                helper.leave();
                return false;
            }
            anchorX = helper.getFloat(*anchor, "x", 0.5f);
            anchorY = helper.getFloat(*anchor, "y", 0.5f);
            helper.leave();
        }

        if (const rapidjson::Value* scaleValue = helper.member(*transform, "scale"))
        {
            helper.enterKey("scale");
            if (scaleValue->IsNumber())
            {
                scale = scaleValue->GetFloat();
            }
            else if (scaleValue->IsObject())
            {
                scale = helper.getFloat(*scaleValue, "x", 1.0f);
            }
            else
            {
                helper.fail("expected number or object");
                helper.leave();
                helper.leave();
                return false;
            }
            helper.leave();
        }

        rotation = helper.getFloat(*transform, "rotation", 0.0f);
        helper.leave();
    }

    outFrame.setOffsetX(offsetX);
    outFrame.setOffsetY(offsetY);
    outFrame.setAnchorX(anchorX);
    outFrame.setAnchorY(anchorY);
    outFrame.setScale(scale);
    outFrame.setRotation(rotation);
    return helper.ok();
}

}  // namespace

bool AniData::load(const std::string& path)
{
    m_frames.clear();
    m_sourcePath = path;

    const std::string jsonText = io::getStringFromFile(path);
    if (jsonText.empty())
    {
        MG_LOG_W("AniData: failed to read '{}'", path);
        return false;
    }

    JsonHelper helper(path);
    rapidjson::Document doc;
    if (!helper.parse(jsonText, doc))
        return false;
    if (!helper.requireObject(doc))
        return false;

    // 不解析 loop / previewVars：循环由播放入口传入，previewVars 仅编辑器预览用

    const rapidjson::Value* frames = nullptr;
    if (!helper.requireMemberArray(doc, "frames", frames))
        return false;

    helper.enterKey("frames");
    m_frames.reserve(frames->Size());
    for (rapidjson::SizeType i = 0; i < frames->Size(); ++i)
    {
        helper.enterIndex(i);
        AniFrame frame;
        if (!parseFrame(helper, (*frames)[i], frame))
            return false;
        m_frames.push_back(std::move(frame));
        helper.leave();
    }
    helper.leave();

    return helper.ok();
}

int AniData::totalDurationMs() const
{
    int total = 0;
    for (const AniFrame& frame : m_frames)
        total += frame.getDelay();
    return total;
}

int AniData::frameIndexAtTime(int timeMs) const
{
    if (m_frames.empty())
        return -1;

    const int total = totalDurationMs();
    if (total <= 0)
        return 0;

    if (timeMs < 0)
        return 0;
    if (timeMs >= total)
        return static_cast<int>(m_frames.size()) - 1;

    int cursor = 0;
    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        const int delay = m_frames[i].getDelay();
        if (timeMs < cursor + delay)
            return static_cast<int>(i);
        cursor += delay;
    }
    return static_cast<int>(m_frames.size()) - 1;
}

NS_MG_END
