#include "LaunchOptions.h"

#ifdef _WIN32
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#    include <shellapi.h>
#endif

std::unordered_map<std::string, std::string> LaunchOptions::s_options;

void LaunchOptions::init(int argc, char** argv)
{
    s_options.clear();
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i])
            parseArg(argv[i]);
    }
}

#ifdef _WIN32
void LaunchOptions::initFromWin32CommandLine()
{
    s_options.clear();
    int argc     = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return;
    for (int i = 1; i < argc; ++i)
    {
        const int size = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1)
            continue;
        std::string arg(static_cast<size_t>(size - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, arg.data(), size, nullptr, nullptr);
        parseArg(arg);
    }
    LocalFree(argv);
}
#endif

std::string_view LaunchOptions::get(std::string_view key)
{
    const auto it = s_options.find(std::string(key));
    return it == s_options.end() ? std::string_view{} : std::string_view(it->second);
}

bool LaunchOptions::has(std::string_view key)
{
    return s_options.find(std::string(key)) != s_options.end();
}

void LaunchOptions::parseArg(std::string_view arg)
{
    if (arg.size() <= 2 || arg.substr(0, 2) != "--")
        return;
    arg.remove_prefix(2);
    const size_t eq = arg.find('=');
    if (eq == 0)
        return;
    if (eq == std::string_view::npos)
        s_options[std::string(arg)].clear();
    else
        s_options[std::string(arg.substr(0, eq))] = std::string(arg.substr(eq + 1));
}
