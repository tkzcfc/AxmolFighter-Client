#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/conf/LayerLoader.h"
#include "mugen/core/io/FileUtils.h"

TEST_CASE("Spawn layer bounds preserve half units and reject empty ranges")
{
    using namespace mugen;
    // LayerLoader 经 VFS 读取；把夹具写入可写挂载 /user 后按虚拟路径加载。
    REQUIRE(test::ensureContentVfs());
    constexpr std::string_view layerPath = "/user/mugen-layer-spawn-test.layer";
    const std::string json = R"({"root":{"size":{"width":100,"height":100}},"plugins":{"shape":{"shapes":[
        {"name":"range0","position":{"x":10,"y":-10},"size":{"width":5,"height":7}},
        {"name":"range1","position":{"x":0,"y":0},"size":{"width":0,"height":4}}
    ]}}})";
    REQUIRE(io::vfs()->writeFile(layerPath, json) == vfs::Error::OK);
    const auto layer = LayerLoader::load(std::string(layerPath));
    REQUIRE(layer.moveRanges.size() == 1);
    CHECK(layer.moveRanges[0].x == "7.5"_fx);
    CHECK(layer.moveRanges[0].y == "-13.5"_fx);
    CHECK(layer.moveRanges[0].width == Fixed::fromInt(5));
    CHECK(layer.moveRanges[0].height == Fixed::fromInt(7));
    CHECK(layer.size.x == 100);
    CHECK(layer.size.y == 100);
}
