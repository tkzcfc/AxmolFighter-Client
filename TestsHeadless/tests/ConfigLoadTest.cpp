#include "doctest.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/avatar/FashionResolver.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

#include <filesystem>
#include <fstream>

TEST_CASE("ConfigLoad finds real skill 920000")
{
    namespace fs = std::filesystem;
    const fs::path configPath =
        fs::path(__FILE__).parent_path().parent_path().parent_path() / "Content" / "mugen" / "config" / "config.bin";
    mugen::conf::Config config;
    REQUIRE(config.load(configPath.string()));
    const auto* skill = config.find<mugen::conf::SkillAttackConfig>(920000);
    REQUIRE(skill != nullptr);
    CHECK(skill->actionIds == std::vector<std::vector<int32_t>>{{920000, 920010, 920020}});
    CHECK(config.find<mugen::conf::SkillAttackConfig>(-123456) == nullptr);
}

TEST_CASE("AvatarAssetLoad finds lilimu motion and box timeline")
{
    const auto content = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Content";
    auto* cache = mugen::AvatarAssetCache::getInstance();
    cache->clearSearchPaths();
    cache->addSearchPath("res_zhcn");
    REQUIRE(cache->load((content / "mugen/config/avatar.bin").string()));
    const auto* motionMap = cache->getMotionMap("mugen/motion/hero/rw_newzhujue_lilimu.motion");
    REQUIRE(motionMap != nullptr);
    const auto* motion = motionMap->findMotion("101");
    REQUIRE(motion != nullptr);
    REQUIRE(motion->clipCount() == 1);
    CHECK(motion->clipAtIndex(0)->timeline != nullptr);
}

TEST_CASE("AvatarAssetLoad refuses a schema-mismatched archive")
{
    const auto content = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Content";
    const auto source = content / "mugen/config/avatar.bin";
    const auto invalid = std::filesystem::temp_directory_path() / "mugen-avatar-schema-mismatch.bin";
    std::filesystem::copy_file(source, invalid, std::filesystem::copy_options::overwrite_existing);
    std::fstream file(invalid, std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(8);
    const char changed = 0;
    file.write(&changed, 1);
    file.close();

    auto* cache = mugen::AvatarAssetCache::getInstance();
    cache->clearSearchPaths();
    CHECK_FALSE(cache->load(invalid.string()));
    std::filesystem::remove(invalid);
}

TEST_CASE("ConfigLoad resolves every real row by its registered key policy")
{
    // Given the production archive.
    const auto path =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Content/mugen/config/config.bin";
    mugen::conf::Config config;
    REQUIRE(config.load(path.string()));

    // When each table is addressed using its declared identity.
    const auto checkTable = [&config]<class T>() {
        using namespace mugen::conf;
        using Policy     = typename ConfigTablePolicy<T>::Policy;
        const auto& rows = config.table<T>();
        REQUIRE_FALSE(rows.empty());
        for (size_t index = 0; index < rows.size(); ++index)
        {
            const auto key = [&]() {
                if constexpr (std::is_same_v<Policy, KeyByOrdinal>)
                    return static_cast<int32_t>(index + 1);
                else
                    return Policy::get(rows[index]);
            }();
            // Then the lookup identifies that exact row, including duplicate ids.
            CHECK(config.find<T>(key) == &rows[index]);
            CHECK(&config.get<T>(key) == &rows[index]);
        }
        if constexpr (std::is_same_v<Policy, KeyByOrdinal>)
        {
            CHECK(config.find<T>(0) == nullptr);
            CHECK(config.find<T>(static_cast<int32_t>(rows.size() + 1)) == nullptr);
        }
    };
#define MG_CHECK_CONFIG_TABLE(kind, Type, Key) checkTable.operator()<mugen::conf::Type>();
    MG_CONFIG_TABLES(MG_CHECK_CONFIG_TABLE)
#undef MG_CHECK_CONFIG_TABLE
}

TEST_CASE("ConfigLoad selects occupation specific fashion when ids overlap")
{
    // Given the production archive with shared fashion ids.
    const auto path =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Content/mugen/config/config.bin";
    mugen::conf::Config config;
    REQUIRE(config.load(path.string()));
    for (int32_t occupation : {1, 2, 3, 4})
    {
        // When the full composite key is requested.
        const auto* fashion = config.find<mugen::conf::FashionConfig>({6001, occupation});
        // Then both key components match, rather than the first duplicate id.
        REQUIRE(fashion != nullptr);
        CHECK(fashion->id == 6001);
        CHECK(fashion->occupation == occupation);
    }
    CHECK(config.find<mugen::conf::FashionConfig>({6001, 99}) == nullptr);
}

TEST_CASE("ConfigLoad returns null when real reference chains dangle")
{
    // Given known source rows whose targets are absent.
    const auto path =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Content/mugen/config/config.bin";
    mugen::conf::Config config;
    REQUIRE(config.load(path.string()));
    const auto* action = config.findById<mugen::conf::ActionConfig>(501750);
    const auto* role   = config.findById<mugen::conf::RoleConfig>(41211);
    REQUIRE(action != nullptr);
    REQUIRE(role != nullptr);
    REQUIRE_FALSE(action->effectIds.empty());
    REQUIRE_FALSE(role->aiId.empty());
    // When traversing the actual reference fields, then misses remain nullable.
    CHECK(config.find<mugen::conf::EffectConfig>(std::get<int32_t>(action->effectIds.front())) == nullptr);
    CHECK(config.find<mugen::conf::AiConfig>(role->aiId.front()) == nullptr);
}

TEST_CASE("FashionResolver uses occupation and skips missing referenced fashion")
{
    // Given the same production configuration used by avatar callers.
    const auto path =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Content/mugen/config/config.bin";
    REQUIRE(mugen::conf::Config::instance().load(path.string()));
    for (int32_t role : {101, 102, 104})
    {
        mugen::FashionAppearance appearance;
        appearance.roleId                                      = role;
        appearance.equipFashion[mugen::FashionPosition::kHat]  = 6001;
        appearance.equipFashion[mugen::FashionPosition::kWing] = -123456;
        mugen::AvatarDesc desc;
        // When resolving a shared id and an absent reference together.
        REQUIRE(mugen::FashionResolver::resolve(appearance, desc));
        const auto* expected = mugen::conf::Config::instance().find<mugen::conf::ResFashionConfig>((role * 1000) + 121);
        REQUIRE(expected != nullptr);
        // Then the occupation-specific atlas survives the nullable miss.
        CHECK(std::find(desc.atlases.begin(), desc.atlases.end(), std::get<std::string>(expected->spine)) !=
              desc.atlases.end());
    }
}
