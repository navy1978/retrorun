#include "config/flycast_game_catalog.h"

#include <cassert>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace rr::flycast_profiles;

namespace
{

void testBuiltInProfiles()
{
    const Catalog catalog = builtinCatalog();
    assert(catalog.schema_version == 1);
    assert(catalog.catalog_version == 20260741);
    assert(catalog.profiles.size() == 13);
    assert(normalizeProductNumber("T1401D  50 ") == "T1401D50");

    Profile profile;
    bool fallback = false;
    assert(selectProfile(catalog, "MK-51117", Mode::BestValidated,
                         profile, fallback));
    assert(!fallback);
    assert(profile.settings.at("reicast_fast_depth") ==
           "menu_guarded_shadow_safe");
    assert(profile.settings.at("reicast_audio_mixer") == "lowend");

    const auto amberSettings =
        settingsForOptionPrefix(profile.settings, "flycast2022_");
    assert(amberSettings.at("flycast2022_fast_depth") ==
           "menu_guarded_shadow_safe");
    assert(amberSettings.at("flycast2022_audio_mixer") == "lowend");
    assert(amberSettings.count("reicast_fast_depth") == 0);

    const char *knownRetailVariants[] = {
        "MK-51117", "HDR-0165",
        "RDC-0140", "RDC-0149", "T8116D50", "T3602M", "T3601M", "T3601N",
        "T1401D50", "T1401M", "T1401N",
        "MK-51035", "HDR-0053"
    };
    for (const char *product : knownRetailVariants)
        assert(catalog.profiles.count(product) == 1);

    assert(selectProfile(catalog, "mk-51117", Mode::BestPerformance,
                         profile, fallback));
    assert(!fallback);
    assert(profile.settings.at("reicast_fast_depth") ==
           "menu_guarded_shadow_safe");
    assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
    assert(profile.settings.at("reicast_mipmapping") == "enabled");

    assert(selectProfile(catalog, "RDC-0149", Mode::BestPerformance,
                        profile, fallback));
    assert(!fallback);
    assert(profile.mode == Mode::BestPerformance);
    assert(profile.settings.at("reicast_framerate") == "normal");
    assert(profile.settings.at("reicast_loop_declared_fps") == "false");
    assert(profile.settings.at("retrorun_loop_declared_fps") == "true");
    assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");
    const auto mixedSettings =
        settingsForOptionPrefix(profile.settings, "flycast2021_");
    assert(mixedSettings.at("flycast2021_aica_arm_cycles") == "24");
    assert(mixedSettings.at("flycast2021_framerate") == "normal");

    assert(selectProfile(catalog, "T1401D  50", Mode::BestValidated,
                         profile, fallback));
    assert(!fallback);
    assert(profile.settings.at(
               "reicast_translucent_menu_guard_draw_sorting") ==
           "per_triangle");
    assert(profile.settings.at("reicast_framerate") == "normal");
    assert(profile.settings.at("retrorun_loop_declared_fps") == "false");

    assert(selectProfile(catalog, "T1401N", Mode::BestPerformance,
                         profile, fallback));
    assert(!fallback);
    assert(profile.product_number == "T1401N");
    assert(profile.settings.at(
               "reicast_translucent_menu_guard_strategy") ==
           "top_hud_last");
    assert(profile.settings.at("reicast_audio_mixer") == "lowend");

    assert(!selectProfile(catalog, "UNKNOWN", Mode::BestValidated,
                          profile, fallback));
    assert(cachedCatalogPath("/storage/config/retrorun.cfg") ==
           "/storage/config/flycast-game-catalog.cache.ini");
}

void testVersionedRepositoryCatalogMatchesBuiltIn()
{
    Catalog fileCatalog;
    std::vector<std::string> diagnostics;
    assert(loadCatalogFile(
        "../profiles/flycast2022-lowend/flycast-game-catalog.ini",
        fileCatalog, diagnostics));
    assert(diagnostics.empty());

    const Catalog builtIn = builtinCatalog();
    assert(fileCatalog.schema_version == builtIn.schema_version);
    assert(fileCatalog.catalog_version == builtIn.catalog_version);
    assert(fileCatalog.profiles.size() == builtIn.profiles.size());

    Profile fromFile;
    Profile fromBuiltIn;
    bool fileFallback = false;
    bool builtInFallback = false;
    assert(selectProfile(fileCatalog, "T1401D  50",
                         Mode::BestPerformance, fromFile, fileFallback));
    assert(selectProfile(builtIn, "T1401D  50",
                         Mode::BestPerformance, fromBuiltIn,
                         builtInFallback));
    assert(fromFile.settings == fromBuiltIn.settings);

    std::ifstream variants(
        "../profiles/flycast2022-lowend/dreamcast-product-variants.tsv");
    assert(variants.good());
    std::set<std::string> variantProducts;
    std::string line;
    while (std::getline(variants, line))
    {
        if (line.empty() || line.front() == '#')
            continue;
        std::istringstream fieldsStream(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(fieldsStream, field, '\t'))
            fields.push_back(field);
        assert(fields.size() == 6);
        const std::string product = normalizeProductNumber(fields[4]);
        assert(!product.empty());
        assert(fileCatalog.profiles.count(product) == 1);
        variantProducts.insert(product);
    }
    assert(variantProducts.size() == fileCatalog.profiles.size());
}

void testExternalCatalogParsing()
{
    std::istringstream input(
        "schema_version = 1\n"
        "catalog_version = 20260729\n"
        "default.reicast_fast_depth = disabled\n"
        "profile.TEST-1.best_validated.title = Test Game\n"
        "profile.TEST-1.best_validated.reicast_audio_mixer = accurate\n"
        "profile.TEST-1.best_performance.inherits = best_validated\n"
        "profile.TEST-1.best_performance.reicast_fast_depth = vertex_fast_log\n");

    Catalog catalog;
    std::vector<std::string> diagnostics;
    assert(parseCatalog(input, "memory", catalog, diagnostics));
    assert(diagnostics.empty());
    assert(catalog.catalog_version == 20260729);

    Profile profile;
    bool fallback = false;
    assert(selectProfile(catalog, " test-1 ", Mode::BestPerformance,
                         profile, fallback));
    assert(!fallback);
    assert(profile.title == "Test Game");
    assert(profile.settings.at("reicast_audio_mixer") == "accurate");
    assert(profile.settings.at("reicast_fast_depth") == "vertex_fast_log");
}

void testInvalidCatalogIsRejected()
{
    std::istringstream input(
        "schema_version = 2\n"
        "catalog_version = 1\n"
        "profile.TEST.best_validated.execute_command = reboot\n");
    Catalog catalog;
    std::vector<std::string> diagnostics;
    assert(!parseCatalog(input, "invalid", catalog, diagnostics));
    assert(!diagnostics.empty());
}

} // namespace

int main()
{
    testBuiltInProfiles();
    testVersionedRepositoryCatalogMatchesBuiltIn();
    testExternalCatalogParsing();
    testInvalidCatalogIsRejected();
    return 0;
}
