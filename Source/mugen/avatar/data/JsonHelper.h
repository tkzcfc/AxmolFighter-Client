#pragma once

#include "mugen/core/MacroDefinition.h"
#include "rapidjson/document.h"
#include <string>
#include <vector>

NS_MG_BEGIN

/**
 * rapidjson 读取辅助
 */
class JsonHelper
{
public:
    explicit JsonHelper(std::string sourcePathForLog);

    bool parse(const std::string& jsonText, rapidjson::Document& outDoc);

    void enterKey(const char* key);
    void enterIndex(size_t index);
    void leave();

    /** 当前 JSON 路径，如 frames[3].delay */
    std::string path() const;

    void fail(const std::string& message);
    bool ok() const { return m_ok; }

    const rapidjson::Value* member(const rapidjson::Value& object, const char* key) const;

    bool requireObject(const rapidjson::Value& value);
    bool requireArray(const rapidjson::Value& value);

    bool requireMemberObject(const rapidjson::Value& object, const char* key, const rapidjson::Value*& out);
    bool requireMemberArray(const rapidjson::Value& object, const char* key, const rapidjson::Value*& out);

    int getInt(const rapidjson::Value& object, const char* key, int fallback);
    float getFloat(const rapidjson::Value& object, const char* key, float fallback);
    bool getBool(const rapidjson::Value& object, const char* key, bool fallback);
    std::string getString(const rapidjson::Value& object, const char* key, const std::string& fallback = {});

    bool requireInt(const rapidjson::Value& object, const char* key, int& out);
    bool requireString(const rapidjson::Value& object, const char* key, std::string& out);

    bool readIntValue(const rapidjson::Value& value, int& out);
    bool readFloatValue(const rapidjson::Value& value, float& out);

private:
    void logFail(const std::string& message) const;

    // 源文件路径，用于错误日志
    std::string m_sourcePath;
    // 当前 JSON 路径栈
    std::vector<std::string> m_pathStack;
    // 是否仍可读（遇错后为 false）
    bool m_ok = true;
};

NS_MG_END
