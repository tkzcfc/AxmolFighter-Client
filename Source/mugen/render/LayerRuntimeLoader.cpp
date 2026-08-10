#include "LayerRuntimeLoader.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/RenderUtils.h"
#    include "mugen/render/SpineSkeletonLoader.h"

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
            return value->GetInt();
        if (value->IsNumber())
            return static_cast<int>(std::round(value->GetDouble()));
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

ax::Vec2 vec2Or(const JsonValue& object, const char* key, const ax::Vec2& fallback)
{
    const JsonValue* value = member(object, key);
    if (!value || !value->IsObject())
        return fallback;
    return {numberOr(*value, "x", fallback.x), numberOr(*value, "y", fallback.y)};
}

ax::Size sizeOr(const JsonValue& object, const char* key, const ax::Size& fallback)
{
    const JsonValue* value = member(object, key);
    if (!value || !value->IsObject())
        return fallback;
    return {numberOr(*value, "width", fallback.width), numberOr(*value, "height", fallback.height)};
}

uint8_t colorByte(float value)
{
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}

ax::Color3B color3Or(const JsonValue& object, const char* key, const ax::Color3B& fallback)
{
    const JsonValue* value = member(object, key);
    if (!value || !value->IsObject())
        return fallback;
    return {colorByte(numberOr(*value, "r", fallback.r / 255.0f)),
            colorByte(numberOr(*value, "g", fallback.g / 255.0f)),
            colorByte(numberOr(*value, "b", fallback.b / 255.0f))};
}

ax::Color4B color4Or(const JsonValue& object, const char* key, const ax::Color4B& fallback)
{
    const JsonValue* value = member(object, key);
    if (!value || !value->IsObject())
        return fallback;
    return {
        colorByte(numberOr(*value, "r", fallback.r / 255.0f)), colorByte(numberOr(*value, "g", fallback.g / 255.0f)),
        colorByte(numberOr(*value, "b", fallback.b / 255.0f)), colorByte(numberOr(*value, "a", fallback.a / 255.0f))};
}

std::string propertyString(const JsonValue& objectNode, const char* key, const std::string& fallback = {})
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
        if (prop.HasMember("value") && prop["value"].IsNumber())
            return std::to_string(prop["value"].GetFloat());
    }
    return fallback;
}

void applyPreviewVisual(ax::Node& previewNode, const JsonValue& node)
{
    previewNode.setAnchorPoint(ax::Vec2::ZERO);
    previewNode.setPosition(ax::Vec2::ZERO);
    previewNode.setColor(color3Or(node, "color", ax::Color3B::WHITE));
    previewNode.setOpacity(static_cast<uint8_t>(std::clamp(intOr(node, "opacity", 255), 0, 255)));
}

void setSpriteContentSize(ax::Sprite& sprite, const ax::Size& size)
{
    sprite.setContentSize(ax::Vec2(std::max(1.0f, size.width), std::max(1.0f, size.height)));
}

ax::Node* createSpriteNode(const JsonValue& node)
{
    const JsonValue* spriteData = member(node, "sprite");
    if (!spriteData || !spriteData->IsObject())
        return ax::Node::create();

    const std::string sourceType = stringOr(*spriteData, "sourceType", "Texture");
    ax::Sprite* sprite           = nullptr;
    if (sourceType == "SpriteFrame")
    {
        const std::string atlasPath = stringOr(*spriteData, "atlasPath");
        const std::string frameName = stringOr(*spriteData, "frameName");
        if (!atlasPath.empty() && !frameName.empty())
        {
            auto* cache = ax::SpriteFrameCache::getInstance();
            if (!cache->isSpriteFramesWithFileLoaded(atlasPath))
                cache->addSpriteFramesWithFile(atlasPath);
            sprite = ax::Sprite::createWithSpriteFrameName(frameName);
        }
    }
    else
    {
        const std::string imagePath = stringOr(*spriteData, "imagePath");
        if (!imagePath.empty())
            sprite = ax::Sprite::create(imagePath);
    }

    if (!sprite)
    {
        MG_LOG_W("LayerRuntimeLoader: failed to create Sprite '{}'", stringOr(node, "name"));
        return ax::Node::create();
    }

    setSpriteContentSize(*sprite, sizeOr(node, "size", sprite->getContentSize()));
    const std::string renderType = stringOr(*spriteData, "renderType", "Simple");
    if (renderType == "Tiled")
        RenderUtils::setupSpriteType(sprite, SpriteType::kSpriteType_Tiled);
    else if (renderType == "Sliced")
        RenderUtils::setupSpriteType(sprite, SpriteType::kSpriteType_Sliced);
    else
        RenderUtils::setupSpriteType(sprite, SpriteType::kSpriteType_Simple);
    return sprite;
}

ax::TextHAlignment textHAlignment(const std::string& value)
{
    if (value == "Left")
        return ax::TextHAlignment::LEFT;
    if (value == "Right")
        return ax::TextHAlignment::RIGHT;
    return ax::TextHAlignment::CENTER;
}

ax::TextVAlignment textVAlignment(const std::string& value)
{
    if (value == "Top")
        return ax::TextVAlignment::TOP;
    if (value == "Bottom")
        return ax::TextVAlignment::BOTTOM;
    return ax::TextVAlignment::CENTER;
}

ax::Node* createLabelNode(const JsonValue& node)
{
    const JsonValue* labelData = member(node, "label");
    if (!labelData || !labelData->IsObject())
        return ax::Node::create();

    const std::string text = stringOr(*labelData, "text");
    const float fontSize   = std::max(1.0f, numberOr(*labelData, "fontSize", 24.0f));
    ax::Label* label       = nullptr;

    const std::string fontPath = stringOr(*labelData, "fontPath");
    const std::string fontType = stringOr(*labelData, "fontType", "System");
    if (fontType == "BMFont" && !fontPath.empty())
        label = ax::Label::createWithBMFont(fontPath, text);
    else if (!fontPath.empty())
        label = ax::Label::createWithTTF(text, fontPath, fontSize);
    else
        label = ax::Label::createWithSystemFont(text, stringOr(*labelData, "fontName", "Arial"), fontSize);

    if (!label)
    {
        MG_LOG_W("LayerRuntimeLoader: failed to create Label '{}'", stringOr(node, "name"));
        return ax::Node::create();
    }

    const ax::Size size = sizeOr(node, "size", {0.0f, 0.0f});
    label->setDimensions(std::max(0.0f, size.width), std::max(0.0f, size.height));
    label->setAlignment(textHAlignment(stringOr(*labelData, "horizontalAlignment", "Center")),
                        textVAlignment(stringOr(*labelData, "verticalAlignment", "Center")));
    label->setTextColor(color4Or(*labelData, "color", ax::Color4B::WHITE));
    if (boolOr(*labelData, "outlineEnabled", false))
        label->enableOutline(color4Or(*labelData, "outlineColor", ax::Color4B::WHITE),
                             std::max(0.0f, numberOr(*labelData, "outlineSize", 1.0f)));
    if (boolOr(*labelData, "shadowEnabled", false))
    {
        label->enableShadow(color4Or(*labelData, "shadowColor", ax::Color4B::BLACK),
                            {numberOr(*labelData, "shadowOffsetX", 2.0f), numberOr(*labelData, "shadowOffsetY", -2.0f)},
                            0);
    }
    return label;
}

ax::Node* createSpineNode(const JsonValue& node)
{
    const JsonValue* spineData = member(node, "spine");
    if (!spineData || !spineData->IsObject())
    {
        MG_LOG_W("LayerRuntimeLoader: Spine node '{}' missing spine payload", stringOr(node, "name"));
        return ax::Node::create();
    }

    const std::string jsonPath  = stringOr(*spineData, "jsonPath");
    const std::string atlasPath = stringOr(*spineData, "atlasPath");
    if (jsonPath.empty() || atlasPath.empty())
    {
        MG_LOG_W("LayerRuntimeLoader: Spine node '{}' missing json/atlas path", stringOr(node, "name"));
        return ax::Node::create();
    }

    // path 常为 .json，内容可能是 Binary；走公共探测加载
    auto* skeleton = SpineSkeletonLoader::createSkeletonAnimation(jsonPath, atlasPath);
    if (!skeleton)
    {
        MG_LOG_W("LayerRuntimeLoader: failed to load Spine '{}' skeleton='{}' atlas='{}'", stringOr(node, "name"),
                 jsonPath, atlasPath);
        return ax::Node::create();
    }

    const std::string skinName = stringOr(*spineData, "skinName");
    if (!skinName.empty())
        skeleton->setSkin(skinName);

    const std::string animationName = stringOr(*spineData, "animationName");
    if (!animationName.empty())
        skeleton->setAnimation(0, animationName, boolOr(*spineData, "loop", true));

    skeleton->setTimeScale(numberOr(*spineData, "timeScale", 1.0f));
    return skeleton;
}

// map_data offset → ParallaxNode ratio（等价于层位移 -viewPos*offset）
ax::Vec2 parallaxRatioForLayer(const std::string& layerName, const MapDataConfig* mapData)
{
    ax::Vec2 offset(0.0f, 0.0f);
    if (!mapData)
        return {1.0f, 1.0f};

    if (layerName == "distant")
        offset = {mapData->distantOffset.x, mapData->distantOffset.y};
    else if (layerName == "middle")
        offset = {mapData->middleOffset.x, mapData->middleOffset.y};
    else if (layerName == "nearby")
        offset = {mapData->nearbyOffset.x, mapData->nearbyOffset.y};
    else if (layerName == "case")
        offset = {mapData->caseOffset.x, mapData->caseOffset.y};
    else if (layerName == "light")
        offset = {mapData->lightOffset.x, mapData->lightOffset.y};
    else
        return {1.0f, 1.0f};  // ground/region/trigger/entity

    return {1.0f - offset.x, 1.0f - offset.y};
}

void applyTransform(ax::Node& runtimeNode, const JsonValue& node)
{
    const std::string type = stringOr(node, "type", "Node");
    // Label/Spine 这儿不设置 contentSize，避免覆盖其内部尺寸（Label/Spine 内部会根据文本/骨骼自动计算尺寸）
    if (type != "Label" && type != "Spine")
    {
        runtimeNode.setContentSize(sizeOr(node, "size", {100.0f, 100.0f}));
    }

    runtimeNode.setName(stringOr(node, "name", "Node"));
    runtimeNode.setAnchorPoint(vec2Or(node, "anchor", {0.5f, 0.5f}));
    runtimeNode.setPosition(vec2Or(node, "position", ax::Vec2::ZERO));
    runtimeNode.setPositionZ(numberOr(node, "positionZ", 0.0f));
    const ax::Vec2 scale = vec2Or(node, "scale", {1.0f, 1.0f});
    runtimeNode.setScale(scale.x, scale.y);
    runtimeNode.setRotation(numberOr(node, "rotation", 0.0f));
    const ax::Vec2 skew = vec2Or(node, "skew", ax::Vec2::ZERO);
    runtimeNode.setSkewX(skew.x);
    runtimeNode.setSkewY(skew.y);
    runtimeNode.setColor(color3Or(node, "color", ax::Color3B::WHITE));
    runtimeNode.setOpacity(static_cast<uint8_t>(std::clamp(intOr(node, "opacity", 255), 0, 255)));
    runtimeNode.setVisible(boolOr(node, "visible", true));
}

ax::Node* createNode(const JsonValue& node)
{
    if (stringOr(node, "type") == "Object")
    {
        return nullptr;
    }

    const std::string type = stringOr(node, "type", "Node");
    ax::Node* runtimeNode  = nullptr;
    if (type == "Sprite")
        runtimeNode = createSpriteNode(node);
    else if (type == "Label")
        runtimeNode = createLabelNode(node);
    else if (type == "Spine")
        runtimeNode = createSpineNode(node);
    else
        runtimeNode = ax::Node::create();

    if (!runtimeNode)
        runtimeNode = ax::Node::create();

    applyTransform(*runtimeNode, node);

    const JsonValue* children = member(node, "children");
    if (children && children->IsArray())
    {
        int index = 0;
        for (const JsonValue& child : children->GetArray())
        {
            if (!child.IsObject())
                continue;
            ax::Node* childNode = createNode(child);
            if (childNode)
                runtimeNode->addChild(childNode, index++);
        }
    }
    return runtimeNode;
}
}  // namespace

ax::ParallaxNode* LayerRuntimeLoader::loadNode(const std::string& layerFile, const MapDataConfig* mapData)
{
    const std::vector<uint8_t> data = io::getDataFromFile(layerFile);
    if (data.empty())
    {
        MG_LOG_E("LayerRuntimeLoader: failed to read layer file '{}'", layerFile);
        return nullptr;
    }

    const std::string json(reinterpret_cast<const char*>(data.data()), data.size());
    rapidjson::Document document;
    document.Parse(json.c_str(), json.size());
    if (document.HasParseError())
    {
        MG_LOG_E("LayerRuntimeLoader: failed to parse '{}': {}", layerFile,
                 rapidjson::GetParseError_En(document.GetParseError()));
        return nullptr;
    }

    const JsonValue* root = member(document, "root");
    if (!root || !root->IsObject())
    {
        MG_LOG_E("LayerRuntimeLoader: '{}' does not contain root object", layerFile);
        return nullptr;
    }

    auto* mapRoot = ax::ParallaxNode::create();
    mapRoot->setName("mapRoot");
    applyTransform(*mapRoot, *root);

    const JsonValue* children = member(*root, "children");
    if (children && children->IsArray())
    {
        int index = 0;
        for (const JsonValue& child : children->GetArray())
        {
            if (!child.IsObject())
                continue;
            ax::Node* layerNode = createNode(child);
            if (!layerNode)
                continue;

            const std::string layerName(layerNode->getName());
            const ax::Vec2 ratio = parallaxRatioForLayer(layerName, mapData);
            mapRoot->addChild(layerNode, index++, ratio, ax::Vec2::ZERO);
            MG_LOG_I("LayerRuntimeLoader: layer '{}' parallaxRatio=({}, {})", layerName, ratio.x, ratio.y);
        }
    }

    return mapRoot;
}

NS_MG_END

#endif
