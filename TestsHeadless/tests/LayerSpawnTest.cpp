#include "doctest.h"
#include "mugen/conf/LayerLoader.h"
#include <filesystem>
#include <fstream>

TEST_CASE("Spawn layer bounds preserve half units and reject empty ranges")
{
    using namespace mugen;
    const auto path = std::filesystem::temp_directory_path() / "mugen-layer-spawn-test.layer";
    struct Cleanup
    {
        std::filesystem::path path;
        ~Cleanup() { std::filesystem::remove(path); }
    } cleanup{path};
    {
        std::ofstream file(path);
        REQUIRE(file.is_open());
        file << R"({"root":{"size":{"width":100,"height":100}},"plugins":{"shape":{"shapes":[
            {"name":"range0","position":{"x":10,"y":-10},"size":{"width":5,"height":7}},
            {"name":"range1","position":{"x":0,"y":0},"size":{"width":0,"height":4}}
        ]}}})";
        REQUIRE(file.good());
    }
    const auto layer = LayerLoader::load(path.string());
    REQUIRE(layer.moveRanges.size() == 1);
    CHECK(layer.moveRanges[0].x == "7.5"_fx);
    CHECK(layer.moveRanges[0].y == "-13.5"_fx);
    CHECK(layer.moveRanges[0].width == Fixed::fromInt(5));
    CHECK(layer.moveRanges[0].height == Fixed::fromInt(7));
    CHECK(layer.size.x == 100);
    CHECK(layer.size.y == 100);
}
