#include "JsonHelper.h"

#include "mugen/core/StdC.h"

#include "rapidjson/error/en.h"

#include <sstream>

NS_MG_BEGIN

JsonHelper::JsonHelper(std::string sourcePathForLog) : m_sourcePath(std::move(sourcePathForLog)) {}

bool JsonHelper::parse(const std::string& jsonText, rapidjson::Document& outDoc)
{
    m_ok = true;
    m_pathStack.clear();
    outDoc.Parse(jsonText.c_str());
    if (outDoc.HasParseError())
    {
        fail(std::string("JSON parse error: ") + rapidjson::GetParseError_En(outDoc.GetParseError()) + " at offset " +
             std::to_string(outDoc.GetErrorOffset()));
        return false;
    }
    return true;
}

void JsonHelper::enterKey(const char* key)
{
    m_pathStack.emplace_back(key ? key : "");
}

void JsonHelper::enterIndex(size_t index)
{
    m_pathStack.push_back("[" + std::to_string(index) + "]");
}

void JsonHelper::leave()
{
    if (!m_pathStack.empty())
        m_pathStack.pop_back();
}

std::string JsonHelper::path() const
{
    std::ostringstream oss;
    for (size_t i = 0; i < m_pathStack.size(); ++i)
    {
        const std::string& seg = m_pathStack[i];
        if (!seg.empty() && seg.front() == '[')
        {
            oss << seg;
        }
        else
        {
            if (i > 0)
                oss << '.';
            oss << seg;
        }
    }
    return oss.str();
}

void JsonHelper::fail(const std::string& message)
{
    m_ok = false;
    logFail(message);
}

void JsonHelper::logFail(const std::string& message) const
{
    const std::string jsonPath = path();
    if (jsonPath.empty())
        MG_LOG_E("AvatarData JSON error in '{}': {}", m_sourcePath, message);
    else
        MG_LOG_E("AvatarData JSON error in '{}' at '{}': {}", m_sourcePath, jsonPath, message);
}

const rapidjson::Value* JsonHelper::member(const rapidjson::Value& object, const char* key) const
{
    if (!object.IsObject() || key == nullptr || !object.HasMember(key))
        return nullptr;
    return &object[key];
}

bool JsonHelper::requireObject(const rapidjson::Value& value)
{
    if (!value.IsObject())
    {
        fail("expected object");
        return false;
    }
    return true;
}

bool JsonHelper::requireArray(const rapidjson::Value& value)
{
    if (!value.IsArray())
    {
        fail("expected array");
        return false;
    }
    return true;
}

bool JsonHelper::requireMemberObject(const rapidjson::Value& object, const char* key, const rapidjson::Value*& out)
{
    enterKey(key);
    const rapidjson::Value* value = member(object, key);
    if (!value)
    {
        fail("missing required field");
        leave();
        return false;
    }
    if (!value->IsObject())
    {
        fail("expected object");
        leave();
        return false;
    }
    out = value;
    leave();
    return true;
}

bool JsonHelper::requireMemberArray(const rapidjson::Value& object, const char* key, const rapidjson::Value*& out)
{
    enterKey(key);
    const rapidjson::Value* value = member(object, key);
    if (!value)
    {
        fail("missing required field");
        leave();
        return false;
    }
    if (!value->IsArray())
    {
        fail("expected array");
        leave();
        return false;
    }
    out = value;
    leave();
    return true;
}

int JsonHelper::getInt(const rapidjson::Value& object, const char* key, int fallback)
{
    const rapidjson::Value* value = member(object, key);
    if (!value)
        return fallback;
    if (value->IsInt())
        return value->GetInt();
    if (value->IsUint())
        return static_cast<int>(value->GetUint());
    if (value->IsNumber())
        return static_cast<int>(value->GetDouble());
    return fallback;
}

float JsonHelper::getFloat(const rapidjson::Value& object, const char* key, float fallback)
{
    const rapidjson::Value* value = member(object, key);
    if (!value || !value->IsNumber())
        return fallback;
    return value->GetFloat();
}

bool JsonHelper::getBool(const rapidjson::Value& object, const char* key, bool fallback)
{
    const rapidjson::Value* value = member(object, key);
    if (!value || !value->IsBool())
        return fallback;
    return value->GetBool();
}

std::string JsonHelper::getString(const rapidjson::Value& object, const char* key, const std::string& fallback)
{
    const rapidjson::Value* value = member(object, key);
    if (!value || !value->IsString())
        return fallback;
    return value->GetString();
}

bool JsonHelper::requireInt(const rapidjson::Value& object, const char* key, int& out)
{
    enterKey(key);
    const rapidjson::Value* value = member(object, key);
    if (!value)
    {
        fail("missing required field");
        leave();
        return false;
    }
    if (!readIntValue(*value, out))
    {
        fail("expected integer");
        leave();
        return false;
    }
    leave();
    return true;
}

bool JsonHelper::requireString(const rapidjson::Value& object, const char* key, std::string& out)
{
    enterKey(key);
    const rapidjson::Value* value = member(object, key);
    if (!value)
    {
        fail("missing required field");
        leave();
        return false;
    }
    if (!value->IsString())
    {
        fail("expected string");
        leave();
        return false;
    }
    out = value->GetString();
    leave();
    return true;
}

bool JsonHelper::readIntValue(const rapidjson::Value& value, int& out)
{
    if (value.IsInt())
    {
        out = value.GetInt();
        return true;
    }
    if (value.IsUint())
    {
        out = static_cast<int>(value.GetUint());
        return true;
    }
    if (value.IsNumber())
    {
        out = static_cast<int>(value.GetDouble());
        return true;
    }
    return false;
}

bool JsonHelper::readFloatValue(const rapidjson::Value& value, float& out)
{
    if (!value.IsNumber())
        return false;
    out = value.GetFloat();
    return true;
}

NS_MG_END
