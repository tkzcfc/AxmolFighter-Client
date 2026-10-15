#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

/// 进程启动参数（命令行）。只识别 `--key=value` 和 `--flag` 两种形式，其他参数（如 macOS 的 `-psn_...`）忽略。
/// 各平台的 main 在启动引擎前调用 init()；未调用时视为没有任何参数（正常启动）。
class LaunchOptions
{
public:
    static void init(int argc, char** argv);
#ifdef _WIN32
    /// 从 GetCommandLineW() 解析参数并转为 UTF-8，用于没有 argv 的 WinMain 入口。
    static void initFromWin32CommandLine();
#endif

    /// 返回 `--key=value` 的 value；参数不存在或为 `--flag` 形式时返回空串。key 不带 `--` 前缀。
    static std::string_view get(std::string_view key);
    static bool has(std::string_view key);

private:
    static void parseArg(std::string_view arg);

    static std::unordered_map<std::string, std::string> s_options;
};
