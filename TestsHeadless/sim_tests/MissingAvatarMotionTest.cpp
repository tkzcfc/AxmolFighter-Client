#include "doctest.h"

#include "../support/ContentVfs.h"

#include "mugen/avatar/MissingAvatarMotion.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

namespace
{

const char* kYouliMotion = "/mugen/motion/effect/tx_youli_zidongsheji.motion";

const mugen::MotionMap* youliMotionMap()
{
    auto* cache = mugen::AvatarAssetCache::getInstance();
    if (!cache->isLoaded())
    {
        if (!mugen::test::ensureContentVfs())
            return nullptr;
        if (!cache->load("/mugen/config/avatar.bin"))
            return nullptr;
    }
    return cache->getMotionMap(kYouliMotion);
}

int countOf(const std::string& text, const std::string& needle)
{
    int count = 0;
    for (std::size_t pos = 0; (pos = text.find(needle, pos)) != std::string::npos; pos += needle.size())
        ++count;
    return count;
}

template <typename Action>
std::string captureCerr(Action&& action)
{
    std::stringstream buffer;
    auto* previous = std::cerr.rdbuf(buffer.rdbuf());
    action();
    std::cerr.rdbuf(previous);
    return buffer.str();
}

}  // namespace

TEST_CASE("Missing avatar motion warns once for an absent tx_youli name")
{
    const auto* map = youliMotionMap();
    REQUIRE(map != nullptr);
    REQUIRE(map->findMotion("18") == nullptr);

    const mugen::Motion* first  = nullptr;
    const mugen::Motion* second = nullptr;
    const auto text             = captureCerr([&] {
        first  = mugen::findAvatarMotion(*map, kYouliMotion, "18");
        second = mugen::findAvatarMotion(*map, kYouliMotion, "18");
    });

    CHECK(first == nullptr);
    CHECK(second == nullptr);
    CHECK(countOf(text, "Avatar: motion '18' not found in '/mugen/motion/effect/tx_youli_zidongsheji.motion'") == 1);
}

TEST_CASE("Missing avatar motion still returns a name present in tx_youli")
{
    const auto* map = youliMotionMap();
    REQUIRE(map != nullptr);
    REQUIRE(map->findMotion("0") != nullptr);

    const mugen::Motion* first  = nullptr;
    const mugen::Motion* second = nullptr;
    const auto text             = captureCerr([&] {
        first  = mugen::findAvatarMotion(*map, kYouliMotion, "0");
        second = mugen::findAvatarMotion(*map, kYouliMotion, "0");
    });

    CHECK(first != nullptr);
    CHECK(second == first);
    CHECK(countOf(text, "Avatar: motion '0' not found") == 0);
}

TEST_CASE("Missing avatar motion stays quiet for an empty name")
{
    const auto* map = youliMotionMap();
    REQUIRE(map != nullptr);

    const mugen::Motion* first  = nullptr;
    const mugen::Motion* second = nullptr;
    const auto text             = captureCerr([&] {
        first  = mugen::findAvatarMotion(*map, kYouliMotion, "");
        second = mugen::findAvatarMotion(*map, kYouliMotion, "");
    });

    CHECK(first == nullptr);
    CHECK(second == nullptr);
    CHECK(text.find("Avatar: motion") == std::string::npos);
}
