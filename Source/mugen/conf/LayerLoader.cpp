#include "LayerLoader.h"

#include "mugen/core/io/FileUtils.h"
#include "vfs/VirtualFileSystem.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <cmath>
#include <cstring>

NS_MG_BEGIN

namespace
{
// RapidJSON 节点类型别名。
using JsonValue = rapidjson::Value;

// 查找对象成员，缺失或非对象时返回空。
const JsonValue* member(const JsonValue& object, const char* key)
{
    if (!object.IsObject() || !object.HasMember(key))
        return nullptr;
    return &object[key];
}

// 读取数值成员，缺失或类型不符时返回默认值。
float numberOr(const JsonValue& object, const char* key, float fallback)
{
    const JsonValue* value = member(object, key);
    return value && value->IsNumber() ? value->GetFloat() : fallback;
}

// 读取整数成员，数值成员按四舍五入转换。
int intOr(const JsonValue& object, const char* key, int fallback)
{
    const JsonValue* value = member(object, key);
    if (value)
    {
        if (value->IsInt())
            return value->GetInt();
        if (value->IsNumber())
            return static_cast<int>(std::round(value->GetDouble()));
    }
    return fallback;
}

// 读取字符串成员，缺失或类型不符时返回默认值。
std::string stringOr(const JsonValue& object, const char* key, const std::string& fallback = {})
{
    const JsonValue* value = member(object, key);
    return value && value->IsString() ? value->GetString() : fallback;
}

// 读取 x、y 格式的整数坐标。
IntVec2 vec2iOr(const JsonValue& object, const char* key, const IntVec2& fallback)
{
    const JsonValue* value = member(object, key);
    if (!value || !value->IsObject())
        return fallback;
    return {intOr(*value, "x", fallback.x), intOr(*value, "y", fallback.y)};
}

// 读取 width、height 格式的整数尺寸。
IntVec2 size2iOr(const JsonValue& object, const char* key, const IntVec2& fallback)
{
    const JsonValue* value = member(object, key);
    if (!value || !value->IsObject())
        return fallback;
    return {intOr(*value, "width", fallback.x), intOr(*value, "height", fallback.y)};
}

// 判断图形属性中的 kind 是否匹配。
bool shapeKindIs(const JsonValue& shape, const char* kind)
{
    const JsonValue* props = member(shape, "properties");
    if (!props || !props->IsArray())
        return false;
    for (const JsonValue& prop : props->GetArray())
    {
        if (!prop.IsObject())
            continue;
        if (stringOr(prop, "key") != "kind")
            continue;
        if (prop.HasMember("value") && prop["value"].IsString())
            return std::strcmp(prop["value"].GetString(), kind) == 0;
    }
    return false;
}

// 从图形插件数据收集可移动矩形。
void parseMoveRanges(const JsonValue& document, std::vector<LayerMoveRange>& out)
{
    const JsonValue* plugins = member(document, "plugins");
    if (!plugins || !plugins->IsObject())
        return;
    const JsonValue* shapePlugin = member(*plugins, "shape");
    if (!shapePlugin || !shapePlugin->IsObject())
        return;
    const JsonValue* shapes = member(*shapePlugin, "shapes");
    if (!shapes || !shapes->IsArray())
        return;

    for (const JsonValue& shape : shapes->GetArray())
    {
        if (!shape.IsObject())
            continue;
        const std::string shapeName = stringOr(shape, "name");
        const bool nameLooksRange   = shapeName.rfind("range", 0) == 0;
        if (!shapeKindIs(shape, "moveRange") && !nameLooksRange)
            continue;

        const IntVec2 center = vec2iOr(shape, "position", {0, 0});
        const IntVec2 size   = size2iOr(shape, "size", {0, 0});
        if (size.x <= 0 || size.y <= 0)
            continue;

        LayerMoveRange range;
        range.width  = Fixed::fromInt(size.x);
        range.height = Fixed::fromInt(size.y);
        range.x      = Fixed::fromInt(center.x) - Fixed::ratio(size.x, 2);
        range.y      = Fixed::fromInt(center.y) - Fixed::ratio(size.y, 2);
        out.push_back(range);
    }
}

// 从对象属性数组读取数值。
float propNumber(const JsonValue& objectNode, const char* key, float fallback)
{
    const JsonValue* objectData = member(objectNode, "object");
    if (!objectData || !objectData->IsObject())
        return fallback;
    const JsonValue* props = member(*objectData, "properties");
    if (!props || !props->IsArray())
        return fallback;
    for (const JsonValue& prop : props->GetArray())
    {
        if (!prop.IsObject())
            continue;
        if (stringOr(prop, "key") != key)
            continue;
        if (prop.HasMember("value") && prop["value"].IsNumber())
            return prop["value"].GetFloat();
    }
    return fallback;
}

// 从对象属性数组读取整数。
int propInt(const JsonValue& objectNode, const char* key, int fallback)
{
    return static_cast<int>(std::round(propNumber(objectNode, key, static_cast<float>(fallback))));
}

// 从对象属性数组读取字符串。
std::string propString(const JsonValue& objectNode, const char* key, const std::string& fallback = {})
{
    const JsonValue* objectData = member(objectNode, "object");
    if (!objectData || !objectData->IsObject())
        return fallback;
    const JsonValue* props = member(*objectData, "properties");
    if (!props || !props->IsArray())
        return fallback;
    for (const JsonValue& prop : props->GetArray())
    {
        if (!prop.IsObject())
            continue;
        if (stringOr(prop, "key") != key)
            continue;
        if (prop.HasMember("value") && prop["value"].IsString())
            return prop["value"].GetString();
    }
    return fallback;
}

// 从根节点的元数据对象读取音效 id。
void parseRootMeta(const JsonValue& root, LayerLoadResult& result)
{
    const JsonValue* children = member(root, "children");
    if (!children || !children->IsArray())
        return;

    for (const JsonValue& child : children->GetArray())
    {
        if (!child.IsObject())
            continue;
        if (stringOr(child, "type") != "Object")
            continue;

        const std::string name = stringOr(child, "name");
        const std::string kind = propString(child, "kind");
        if (name != "meta" && kind != "meta")
            continue;

        result.soundId = propInt(child, "soundId", 0);
        return;
    }
}
}  // namespace

// 读取并校验层文件后返回可用的逻辑元数据。
LayerLoadResult LayerLoader::load(const std::string& layerFile)
{
    LayerLoadResult result;

    std::vector<uint8_t> data;
    if (io::vfs()->readFile(layerFile, data) != vfs::Error::OK || data.empty())
    {
        MG_LOG_E("LayerLoader: failed to read layer file '{}'", layerFile);
        return result;
    }

    const std::string json(reinterpret_cast<const char*>(data.data()), data.size());
    rapidjson::Document document;
    document.Parse(json.c_str(), json.size());
    if (document.HasParseError())
    {
        MG_LOG_E("LayerLoader: failed to parse '{}': {}", layerFile,
                 rapidjson::GetParseError_En(document.GetParseError()));
        return result;
    }

    const JsonValue* root = member(document, "root");
    if (!root || !root->IsObject())
    {
        MG_LOG_E("LayerLoader: '{}' does not contain root object", layerFile);
        return result;
    }

    const JsonValue* sizeObj = member(*root, "size");
    if (sizeObj && sizeObj->IsObject())
    {
        if (sizeObj->HasMember("width") || sizeObj->HasMember("height"))
        {
            result.size.x = intOr(*sizeObj, "width", 0);
            result.size.y = intOr(*sizeObj, "height", 0);
        }
        else
        {
            MG_LOG_W("LayerLoader: '{}' root.size object does not contain width/height", layerFile);
        }
    }
    else
    {
        MG_LOG_W("LayerLoader: '{}' root object does not contain size object", layerFile);
    }

    parseMoveRanges(document, result.moveRanges);
    parseRootMeta(*root, result);
    return result;
}

NS_MG_END
