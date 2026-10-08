#include "doctest.h"

#include "mugen/core/math/Fixed.h"
#include "EcsTestTypes.h"

#include <cstring>
#include <filesystem>
#include <limits>
#include <cstdlib>
#if defined(_WIN32)
#    include <crtdbg.h>
#endif

int main(int argc, char** argv)
{
    ecs_test::executablePath = std::filesystem::absolute(argv[0]).string();
    if (argc == 2 && std::strcmp(argv[1], "--ecs-probe-ok") == 0)
        return 0;
    if (argc == 2 && std::strcmp(argv[1], "--ecs-missing-component-probe") == 0)
    {
#if defined(_WIN32)
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
        ecs_test::Manager ecs;
        const mugen::Entity entity = ecs.create();
        static_cast<void>(ecs.get<ecs_test::Position>(entity));
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--fixed-overflow-probe") == 0)
    {
        const mugen::Fixed maximum = mugen::Fixed::fromRaw(std::numeric_limits<int64_t>::max());
        static_cast<void>(maximum * mugen::Fixed::fromInt(2));
        return 0;
    }
    return doctest::Context(argc, argv).run();
}
