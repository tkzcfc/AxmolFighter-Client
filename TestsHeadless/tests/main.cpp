#include "doctest.h"

#include "mugen/core/math/Fixed.h"
#include "BTTestTypes.h"
#include "EcsTestTypes.h"
#include "mugen/sim/spawn/RoleSpawn.h"

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
    if (argc == 2 && std::strcmp(argv[1], "--spawn-invalid-role-probe") == 0)
    {
#if defined(_WIN32)
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
        mugen::sim::World world;
        world.init({});
        mugen::sim::RoleSpawnParams params;
        params.roleId = -123456;
        mugen::sim::RoleSpawner(world).spawnRole(params);
        return 0;
    }
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
    if (argc == 3 && std::strcmp(argv[1], "--bt-invalid-probe") == 0)
    {
        const auto pool = bt_test::params();
        const int probe = std::atoi(argv[2]);
        switch (probe)
        {
        case 0:
            static_cast<void>(bt_test::Def({}, {}, pool));
            break;
        case 1:
            static_cast<void>(bt_test::Def({bt_test::action(0), bt_test::action(1)}, {}, pool));
            break;
        case 2:
            static_cast<void>(
                bt_test::Def({bt_test::composite(mugen::BTNodeType::Selector, 2), bt_test::action(0)}, {}, pool));
            break;
        case 3:
            static_cast<void>(bt_test::Def({bt_test::composite(mugen::BTNodeType::Sequence, 0)}, {}, pool));
            break;
        case 4:
            static_cast<void>(bt_test::Def({bt_test::action(8)}, {}, pool));
            break;
        case 5:
            static_cast<void>(
                bt_test::Def({bt_test::composite(mugen::BTNodeType::Action, 1), bt_test::action(0)}, {}, pool));
            break;
        case 6:
            static_cast<void>(
                bt_test::Def({bt_test::composite(mugen::BTNodeType::Parallel, 0, {0, 1})}, {{0, 8}}, pool));
            break;
        default:
            return 2;
        }
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--world-invalid-probe") == 0)
    {
        const int probe = std::atoi(argv[2]);
        if (probe == 0)
        {
            mugen::sim::LogicTimers timers;
            timers.add({}, mugen::Fixed::fromInt(-1), false, mugen::sim::DestroyEntity{});
        }
        else if (probe == 1)
        {
            mugen::sim::World world;
            world.init({});
            world.tick({1, {}, {}, {}});
            world.tick({1, {}, {}, {}});
        }
        else
            return 2;
        return 0;
    }
    return doctest::Context(argc, argv).run();
}
