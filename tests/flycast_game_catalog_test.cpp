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
    assert(catalog.schema_version == 2);
    assert(catalog.catalog_version == 20260830);
    assert(catalog.profiles.size() == 97);
    assert(catalog.device_profiles.size() == 1);
    assert(normalizeProductNumber("T1401D  50 ") == "T1401D50");
    assert(catalog.safe_defaults.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");
    assert(catalog.safe_defaults.at("reicast_translucent_strip_merge") ==
           "disabled");
    assert(catalog.safe_defaults.at("reicast_texture_storage_reuse") ==
           "disabled");
    assert(catalog.safe_defaults.at("reicast_enable_dsp") == "disabled");

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
        "MK-51035", "HDR-0053", "MK-51058", "T1215N", "MK-51037",
        "T36801D61", "T36801D64", "T1201M", "T1201N",
        "MK-5118450", "HDR-0164", "HDR-0179",
        "T7013D50", "T1213N", "T1209M"
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

    assert(selectProfile(catalog, "RDC-0140", Mode::BestValidated,
                         profile, fallback, "rg353m"));
    assert(!fallback);
    assert(profile.title ==
           "Dead or Alive 2 (observed CDI, RG353M validated)");
    assert(profile.settings.at("retrorun_audio_buffer") == "735");
    assert(profile.settings.at("retrorun_audio_stable_buffer") == "false");
    assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "disabled");
    assert(profile.settings.at("reicast_audio_mixer") == "lowend");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "8");

    assert(selectProfile(catalog, "RDC-0140", Mode::BestPerformance,
                         profile, fallback, "RG353M"));
    assert(fallback);
    assert(profile.mode == Mode::BestValidated);
    assert(profile.settings.at("retrorun_audio_buffer") == "735");

    const auto mixedSettings =
        settingsForOptionPrefix(profile.settings, "flycast2021_");
    assert(mixedSettings.at("flycast2021_aica_arm_cycles") == "8");
    assert(mixedSettings.at("flycast2021_framerate") == "normal");

    assert(selectProfile(catalog, "T1215N", Mode::BestPerformance,
                         profile, fallback));
    assert(fallback);
    assert(profile.title == "Cannon Spike (USA, RG351MP validated)");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");
    assert(profile.settings.at("reicast_translucent_strip_merge") ==
           "disabled");
    assert(profile.settings.at("reicast_translucent_menu_guard_strategy") ==
           "hud_last");
    assert(profile.settings.at("reicast_fast_depth") == "disabled");
    assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
    assert(profile.settings.at("reicast_audio_mixer") == "accurate");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_stable_96");
    assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");

    assert(selectProfile(catalog, "MK-51037", Mode::BestPerformance,
                         profile, fallback));
    assert(fallback);
    assert(profile.title ==
           "Daytona USA 2001 / Daytona USA (RG351MP validated)");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-strip (fast, least accurate)");
    assert(profile.settings.at("reicast_translucent_strip_merge") ==
           "disabled");
    assert(profile.settings.at("reicast_fast_depth") == "disabled");
    assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
    assert(profile.settings.at("reicast_audio_mixer") == "accurate");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_stable_96");
    assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");

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

    assert(selectProfile(catalog, "T1401N", Mode::BestPerformance,
                         profile, fallback, "rg353m"));
    assert(!fallback);
    assert(profile.title ==
           "Soul Calibur (North America, RG353M validated)");
    assert(profile.settings.at("retrorun_audio_buffer") == "735");
    assert(profile.settings.at("retrorun_audio_stable_buffer") == "true");
    assert(profile.settings.at("retrorun_egl_depth_bits") == "24");
    assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");
    assert(profile.settings.at("reicast_sh4clock") == "200");
    assert(profile.settings.at("reicast_frame_skipping") == "disabled");
    assert(profile.settings.at("reicast_translucent_strip_merge") ==
           "menu_guarded");
    assert(profile.settings.at("reicast_fast_depth") == "vertex_fast_log");
    assert(profile.settings.at("reicast_audio_mixer") == "lowend");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "32");

    assert(selectProfile(catalog, "MK-51035", Mode::BestValidated,
                         profile, fallback));
    assert(!fallback);
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");
    assert(profile.settings.at("reicast_fast_depth") ==
           "menu_guarded_shadow_safe");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_stable_96");
    assert(profile.settings.at("retrorun_audio_buffer") == "2048");

    assert(selectProfile(catalog, "HDR-0053", Mode::BestPerformance,
                         profile, fallback));
    assert(fallback);
    assert(profile.product_number == "HDR-0053");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");

    assert(selectProfile(catalog, "MK-51058", Mode::BestPerformance,
                         profile, fallback));
    assert(fallback);
    assert(profile.title ==
           "Jet Set Radio / Jet Grind Radio (RG351MP validated)");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");
    assert(profile.settings.at("reicast_translucent_strip_merge") ==
           "disabled");
    assert(profile.settings.at("reicast_fast_depth") == "enabled");
    assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
    assert(profile.settings.at("reicast_audio_mixer") == "accurate");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_stable_96");
    assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");

    assert(selectProfile(catalog, "T38706M", Mode::BestValidated,
                         profile, fallback));
    assert(!fallback);
    assert(profile.title == "Ikaruga");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");
    assert(profile.settings.at("reicast_fast_depth") ==
           "vertex_fast_log");
    assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_stable_96");

    assert(selectProfile(catalog, "MK-5118450", Mode::BestPerformance,
                         profile, fallback));
    assert(fallback);
    assert(profile.title == "Shenmue II (Europe, RG351MP validated)");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-strip (fast, least accurate)");
    assert(profile.settings.at("reicast_fast_depth") == "vertex_fast_log");
    assert(profile.settings.at("reicast_opaque_strip_merge") == "disabled");
    assert(profile.settings.at("reicast_audio_mixer") == "accurate");
    assert(profile.settings.at("retrorun_audio_buffer") == "4096");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_heavy_100");
    assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");

    assert(!selectProfile(catalog, "UNKNOWN", Mode::BestValidated,
                          profile, fallback));

    const char *streetFighterVariants[] = {
        "T7013D50", "T1213N", "T1209M"
    };
    Profile streetFighterReference;
    for (const char *product : streetFighterVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback));
        assert(!fallback);
        assert(profile.mode == Mode::BestPerformance);
        assert(profile.product_number == product);
        assert(profile.settings.at("reicast_alpha_sorting") ==
               "per-triangle (normal)");
        assert(profile.settings.at("reicast_translucent_strip_merge") ==
               "disabled");
        assert(profile.settings.at("reicast_texture_storage_reuse") ==
               "disabled");
        assert(profile.settings.at("reicast_adjacent_state_elision") ==
               "disabled");
        assert(profile.settings.at("reicast_fast_depth") ==
               "vertex_fast_log");
        assert(profile.settings.at("reicast_opaque_strip_merge") ==
               "enabled");
        assert(profile.settings.at("reicast_audio_mixer") == "accurate");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "lowend_stable_96");
        assert(profile.settings.at("retrorun_egl_stencil_bits") == "8");

        if (streetFighterReference.settings.empty())
            streetFighterReference = profile;
        else
            assert(profile.settings == streetFighterReference.settings);
    }
    assert(streetFighterReference.title ==
           "Street Fighter III: 3rd Strike (Europe, best performance)");
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
    assert(fileCatalog.device_profiles.size() ==
           builtIn.device_profiles.size());

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

    assert(selectProfile(fileCatalog, "MK-51058",
                         Mode::BestPerformance, fromFile, fileFallback));
    assert(selectProfile(builtIn, "MK-51058",
                         Mode::BestPerformance, fromBuiltIn,
                         builtInFallback));
    assert(fileFallback && builtInFallback);
    assert(fromFile.settings == fromBuiltIn.settings);

    assert(selectProfile(fileCatalog, "MK-51037",
                         Mode::BestPerformance, fromFile, fileFallback));
    assert(selectProfile(builtIn, "MK-51037",
                         Mode::BestPerformance, fromBuiltIn,
                         builtInFallback));
    assert(fileFallback && builtInFallback);
    assert(fromFile.settings == fromBuiltIn.settings);

    assert(selectProfile(fileCatalog, "T1215N",
                         Mode::BestPerformance, fromFile, fileFallback));
    assert(selectProfile(builtIn, "T1215N",
                         Mode::BestPerformance, fromBuiltIn,
                         builtInFallback));
    assert(fileFallback && builtInFallback);
    assert(fromFile.settings == fromBuiltIn.settings);

    assert(selectProfile(fileCatalog, "T1401N",
                         Mode::BestPerformance, fromFile, fileFallback,
                         "RG353M"));
    assert(selectProfile(builtIn, "T1401N",
                         Mode::BestPerformance, fromBuiltIn,
                         builtInFallback, "RG353M"));
    assert(!fileFallback && !builtInFallback);
    assert(fromFile.title == fromBuiltIn.title);
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
