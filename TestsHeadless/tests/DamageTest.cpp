#include "doctest.h"
#include "mugen/sim/hit/Damage.h"
#include "mugen/sim/World.h"
#include <fstream>
#include <sstream>
#include <cmath>

namespace
{
using namespace mugen;
using namespace mugen::sim;
// 注入顺序固定的抽样并记录消费次数。
struct Sequence
{
    // 三次抽样分别供闪避、暴击与浮动使用。
    std::array<Fixed, 3> m_values{};
    // 下次读取位置。
    size_t m_cursor = 0;
    // 构造不预取的随机接口。
    hit::DamageDraw draw()
    {
        return {this, [](void* context) -> Fixed {
            auto& sequence = *static_cast<Sequence*>(context);
            REQUIRE(sequence.m_cursor < sequence.m_values.size());
            return sequence.m_values[sequence.m_cursor++];
        }};
    }
};
// 转换精确可表示的测试输入，不参与生产仿真。
Fixed fixed(double value) { return Fixed::fromRaw(static_cast<int64_t>(std::llround(value * Fixed::kOne))); }
}

// 对照独立 Lua 结果并严格限制取整误差比例。
TEST_CASE("Damage golden same inputs and ordered draws")
{
    std::ifstream file(std::string(MUGEN_CONTENT_DIR) + "/../TestsHeadless/golden/damage_golden.csv");
    REQUIRE(file.good());
    std::string line;
    REQUIRE(static_cast<bool>(std::getline(file, line)));
    size_t groups = 0, offByOne = 0;
    while (std::getline(file, line))
    {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream stream(line);
        std::array<double, 33> row{};
        for (auto& value : row) REQUIRE(static_cast<bool>(stream >> value));
        CAPTURE(row[0]);
        hit::DamageInput input;
        input.m_hit.hurtType = static_cast<int>(row[1]);
        input.m_hit.hitMust = static_cast<int>(row[2]);
        input.m_artifact = row[3] != 0;
        input.m_standardHurt = fixed(row[4]);
        input.m_addition = fixed(row[5]);
        auto& a = input.m_attacker;
        auto& d = input.m_target;
        a.basic[5] = fixed(row[6]); a.basic[7] = fixed(row[7]);
        d.basic[6] = fixed(row[8]); d.basic[8] = fixed(row[9]);
        a.basic[9] = fixed(row[10]); d.basic[10] = fixed(row[11]);
        a.basic[11] = fixed(row[12]); d.basic[12] = fixed(row[13]);
        d.basic[13] = fixed(row[14]); a.basic[14] = fixed(row[15]);
        a.extend[0] = fixed(row[16]); d.extend[1] = fixed(row[17]);
        a.extend[2] = fixed(row[18]); d.extend[3] = fixed(row[19]);
        d.extend[9] = fixed(row[20]); a.extend[11] = fixed(row[21]);
        a.extend[12] = fixed(row[22]); d.extend[13] = fixed(row[23]);
        a.extend[14] = fixed(row[24]); input.m_hit.hurtRate = fixed(row[25]);
        Sequence sequence{{fixed(row[26]), fixed(row[27]), fixed(row[28])}};
        const bool miss = hit::attributeDodge(input, sequence.draw());
        hit::DamageResult result;
        if (!miss) result = hit::calculateDamage(input, sequence.draw());
        const auto difference = std::abs(result.m_damage.toInt() - static_cast<int64_t>(row[29]));
        CHECK(difference <= 1);
        CHECK(result.m_critical == (row[30] != 0));
        CHECK(miss == (row[31] != 0));
        CHECK(sequence.m_cursor == static_cast<size_t>(row[32]));
        offByOne += difference == 1;
        ++groups;
    }
    CHECK(groups >= 50);
    CHECK(offByOne * 100 <= groups * 2);
    std::cout << "Damage golden: groups=" << groups << " off_by_one=" << offByOne << '\n';
}

// 检查零概率端点、必中短路、无最小伤害以及唯一 RNG 消费量。
TEST_CASE("Damage boundary comparisons and world RNG")
{
    hit::DamageInput input;
    Sequence sequence;
    CHECK(hit::attributeDodge(input, sequence.draw()));
    CHECK(sequence.m_cursor == 1);
    input.m_hit.hitMust = 1;
    CHECK_FALSE(hit::attributeDodge(input, sequence.draw()));
    CHECK(sequence.m_cursor == 1);
    auto result = hit::calculateDamage(input, sequence.draw());
    CHECK(result.m_damage == "0"_fx);
    CHECK(result.m_critical);
    CHECK(sequence.m_cursor == 3);

    World world;
    world.init({});
    auto expected = world.clone();
    auto& random = world.ecs().singleton<RngState>().random;
    hit::calculateDamage(input, hit::damageDraw(random));
    auto& expectedRandom = expected.ecs().singleton<RngState>().random;
    expectedRandom.randomF("0"_fx, "100"_fx);
    expectedRandom.randomF("0"_fx, "100"_fx);
    CHECK(random.next() == expectedRandom.next());
}
