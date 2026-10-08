#include <cstring>
#include <iostream>
#include <string_view>

namespace
{

constexpr std::string_view kVersion = "mugen_sim_cli 0.1.0";

void printUsage()
{
    std::cerr << "usage: mugen_sim_cli --version\n";
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0)
    {
        std::cout << kVersion << '\n';
        return 0;
    }

    printUsage();
    return argc == 1 ? 0 : 1;
}
