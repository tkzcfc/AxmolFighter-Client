#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 场景清单：data/golden_summaries/scenarios.txt。
namespace mugen::legacy_cli
{
// 清单中的一个场景。
struct ManifestEntry
{
    // 场景名，也是摘要文件名前缀。
    std::string name;
    // 脚本绝对（或相对工作目录）路径，已按清单所在目录解析。
    std::string script;
    // 房间配置 id。
    int32_t room = 0;
    // 职业 1-7。
    int32_t occupation = 1;
    // 角色等级。
    int32_t level = 1;
    // 随机种子。
    uint64_t seed = 31;
    // 导出帧数。
    uint64_t frames = 700;
    // 副本配置 id（决定复活次数上限与怪物等级加成）；0 表示不指定。
    int32_t copy = 0;
};

// 解析清单：每行 `name script room class level seed frames [copy]`，`#` 为注释，脚本路径相对清单目录。
// `copy` 列可省略，缺省为 0。失败时抛出带英文原因的 std::runtime_error。
std::vector<ManifestEntry> loadManifest(const std::string& path);
}  // namespace mugen::legacy_cli
