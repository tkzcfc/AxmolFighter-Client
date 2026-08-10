#include "LayerLoader.h"

#    include "mugen/conf/TableConfig.h"
#    include "mugen/core/io/FileUtils.h"
#    include "rapidjson/document.h"
#    include "rapidjson/error/en.h"

NS_MG_BEGIN

namespace
{
using JsonValue = rapidjson::Value;

const JsonValue* member(const JsonValue& object, const char* key)
{
    if (!object.IsObject() || !object.HasMember(key))
        return nullptr;
    return &object[key];
}

float numberOr(const JsonValue& object, const char* key, float fallback)
{
    const JsonValue* value = member(object, key);
    return value && value->IsNumber() ? value->GetFloat() : fallback;
}

int intOr(const JsonValue& object, const char* key, int fallback)
{
    const JsonValue* value = member(object, key);
    if (value)
    {
        if (value->IsInt())
        {
            return value->GetInt();
        }
        // 兼容浮点数
        else if (value->IsNumber())
        {
            auto num = value->GetDouble();
            return static_cast<int>(std::round(num));
        }
    }
    return fallback;
}

bool boolOr(const JsonValue& object, const char* key, bool fallback)
{
    const JsonValue* value = member(object, key);
    return value && value->IsBool() ? value->GetBool() : fallback;
}

std::string stringOr(const JsonValue& object, const char* key, const std::string& fallback = {})
{
    const JsonValue* value = member(object, key);
    return value && value->IsString() ? value->GetString() : fallback;
}

//Vector2f vec2fOr(const JsonValue& object, const char* key, const Vector2f& fallback)
//{
//    const JsonValue* value = member(object, key);
//    if (!value || !value->IsObject())
//        return fallback;
//    return {numberOr(*value, "x", fallback.x), numberOr(*value, "y", fallback.y)};
//}

Vector2i vec2iOr(const JsonValue& object, const char* key, const Vector2i& fallback)
{
    const JsonValue* value = member(object, key);
    if (!value || !value->IsObject())
        return fallback;
    return {intOr(*value, "x", fallback.x), intOr(*value, "y", fallback.y)};
}

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

        const Vector2i center = vec2iOr(shape, "position", {0, 0});
        const Vector2i size   = vec2iOr(shape, "size", {0, 0});
        if (size.x <= 0 || size.y <= 0)
            continue;

        LayerMoveRange range;
        range.width  = size.x;
        range.height = size.y;
        range.x      = center.x - size.x * 0.5f;
        range.y      = center.y - size.y * 0.5f;
        out.push_back(range);
    }
}

}  // namespace

LayerLoadResult LayerLoader::load(const std::string& layerFile)
{
    LayerLoadResult result;

    const std::vector<uint8_t> data = io::getDataFromFile(layerFile);
    if (data.empty())
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

    result.size = vec2iOr(*root, "size", {0, 0});
    parseMoveRanges(document, result.moveRanges);

    return result;
}

NS_MG_END
