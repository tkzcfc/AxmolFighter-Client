#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/conf/BuffRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/serialize/BinaryArchive.h"

namespace
{
// 按生产格式构造内存归档，测试真实加载入口。
std::vector<uint8_t> buffArchiveBytes(const mugen::conf::ConfigArchive& archive)
{
    mugen::ByteBuffer bytes;
    for (const auto value : mugen::conf::kConfigMagic)
        bytes.writeByte(value);
    bytes.writeUint32(mugen::conf::kConfigVersion);
    bytes.writeUint64(mugen::conf::configSchemaHash());
    mugen::BinaryWriter writer(bytes);
    writer.write(archive);
    bytes.writeFinish();
    return {bytes.getBuffer(), bytes.getBuffer() + bytes.getSize()};
}
}

// 所有源表规则必须有唯一的稠密标识，未知名称不能降级。
TEST_CASE("BuffConfig registry covers every production rule")
{
    using namespace mugen::conf;
    Config config;
    REQUIRE(mugen::test::ensureContentVfs());
    REQUIRE(config.load("/mugen/config/config.bin"));
    REQUIRE(config.table<BuffRuleConfig>().size() == 162);
    for (const auto& row : config.table<BuffRuleConfig>())
    {
        const auto rule = buffRuleIdFromName(row.className);
        REQUIRE(rule.has_value());
        CHECK(kBuffRuleNames[static_cast<size_t>(*rule)] == row.className);
    }
    for (size_t index = 0; index < kBuffRuleNames.size(); ++index)
        CHECK(buffRuleIdFromName(kBuffRuleNames[index]) == static_cast<BuffRuleId>(index));
    CHECK_FALSE(buffRuleIdFromName("kUnportedRule").has_value());
    CHECK_FALSE(buffRuleIdFromName("").has_value());
    CHECK_FALSE(buffRuleIdFromName("buffSuperArmor").has_value());
}

// 未知名称必须在加载时失败，并清除先前已加载配置。
TEST_CASE("BuffConfig loadBytes rejects unknown className before publication")
{
    using namespace mugen::conf;
    ConfigArchive archive;
    BuffRuleConfig row;
    row.id = 1;
    row.className = "BuffSuperArmor";
    archive.buff_rule.push_back(row);
    Config config;
    REQUIRE(config.loadBytes(buffArchiveBytes(archive)));
    REQUIRE(config.loaded());
    archive.buff_rule.front().className = "BuffUnknownRegressionProbe";
    CHECK_FALSE(config.loadBytes(buffArchiveBytes(archive)));
    CHECK_FALSE(config.loaded());
    CHECK(config.table<BuffRuleConfig>().empty());
    archive.buff_rule.front().className = "BuffDummyDeathRattle";
    CHECK(config.loadBytes(buffArchiveBytes(archive)));
    CHECK(config.loaded());
}
