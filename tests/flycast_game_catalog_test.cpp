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

void assertSameProfile(const Profile &left, const Profile &right)
{
    assert(left.product_number == right.product_number);
    assert(left.title == right.title);
    assert(left.mode == right.mode);
    assert(left.validated == right.validated);
    assert(left.settings == right.settings);
}

void assertSameScopedProfiles(
    const std::map<std::string,
                   std::map<std::string, std::map<Mode, Profile>>> &left,
    const std::map<std::string,
                   std::map<std::string, std::map<Mode, Profile>>> &right)
{
    assert(left.size() == right.size());
    for (const auto &[scope, leftProducts] : left)
    {
        const auto rightScope = right.find(scope);
        assert(rightScope != right.end());
        assert(leftProducts.size() == rightScope->second.size());
        for (const auto &[product, leftModes] : leftProducts)
        {
            const auto rightProduct = rightScope->second.find(product);
            assert(rightProduct != rightScope->second.end());
            assert(leftModes.size() == rightProduct->second.size());
            for (const auto &[mode, leftProfile] : leftModes)
            {
                const auto rightMode = rightProduct->second.find(mode);
                assert(rightMode != rightProduct->second.end());
                assertSameProfile(leftProfile, rightMode->second);
            }
        }
    }
}

void assertSameCatalogPayload(const Catalog &left, const Catalog &right)
{
    assert(left.schema_version == right.schema_version);
    assert(left.catalog_version == right.catalog_version);
    assert(left.safe_defaults == right.safe_defaults);
    assert(left.profiles.size() == right.profiles.size());

    for (const auto &[product, leftModes] : left.profiles)
    {
        const auto rightProduct = right.profiles.find(product);
        assert(rightProduct != right.profiles.end());
        assert(leftModes.size() == rightProduct->second.size());
        for (const auto &[mode, leftProfile] : leftModes)
        {
            const auto rightMode = rightProduct->second.find(mode);
            assert(rightMode != rightProduct->second.end());
            assertSameProfile(leftProfile, rightMode->second);
        }
    }

    assertSameScopedProfiles(left.chip_profiles, right.chip_profiles);
    assertSameScopedProfiles(left.device_profiles, right.device_profiles);
}

void testBuiltInProfiles()
{
    const Catalog catalog = builtinCatalog();
    assert(catalog.schema_version == 3);
    assert(catalog.catalog_version == 20261001);
    assert(catalog.profiles.size() == 98);
    assert(catalog.chip_profiles.size() == 3);
    assert(catalog.device_profiles.empty());
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
        "MK-51019", "HDR-0010", "MK-5100250",
        "T7013D50", "T1213N", "T1209M"
    };
    for (const char *product : knownRetailVariants)
        assert(catalog.profiles.count(product) == 1);

    const auto validatedProfiles = validatedCatalogProfiles(catalog);
    assert(validatedProfiles.count("MK-51117") == 1);
    assert(validatedProfiles.count("T1211N") == 1);
    assert(validatedProfiles.count("MK-51003") == 0);
    assert(catalog.profiles.count("MK-51003") == 1);
    assert(!selectProfile(catalog, "MK-51003", Mode::BestValidated,
                          profile, fallback));
    assert(!fallback);
    assert(!selectProfile(catalog, "MK-51003", Mode::BestPerformance,
                          profile, fallback));

    assert(selectProfile(catalog, "mk-51117", Mode::BestPerformance,
                         profile, fallback));
    assert(!fallback);
    assert(profile.settings.at("reicast_fast_depth") ==
           "menu_guarded_shadow_safe");
    assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
    assert(profile.settings.at("reicast_mipmapping") == "enabled");
    assert(profile.settings.at("retrorun_audio_buffer") == "2048");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_stable_96");
    assert(profile.settings.at("retrorun_loop_declared_fps") == "true");

    const char *sonicVariants[] = {"MK-51117", "HDR-0165"};
    Profile sonicReference;
    for (const char *product : sonicVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg353m"));
        assert(!fallback);
        assert(profile.mode == Mode::BestPerformance);
        assert(profile.settings.at("retrorun_loop_declared_fps") ==
               "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "735");
        assert(profile.settings.at("retrorun_audio_stable_buffer") ==
               "false");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "disabled");
        assert(profile.settings.at("retrorun_egl_stencil_bits") == "8");
        assert(profile.settings.at("reicast_mipmapping") == "enabled");
        assert(profile.settings.at("reicast_fog") == "enabled");
        assert(profile.settings.at("reicast_translucent_strip_merge") ==
               "disabled");
        assert(profile.settings.at("reicast_fast_depth") ==
               "menu_guarded_shadow_safe");
        assert(profile.settings.at("reicast_opaque_strip_merge") ==
               "enabled");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "8");

        if (sonicReference.settings.empty())
            sonicReference = profile;
        else
            assert(profile.settings == sonicReference.settings);
    }

    assert(selectProfile(catalog, "MK-51000", Mode::BestPerformance,
                         profile, fallback, "rg353m"));
    assert(!fallback);
    assert(profile.mode == Mode::BestPerformance);
    assert(profile.title ==
           "Sonic Adventure (Europe / North America, RG353M validated)");
    assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
    assert(profile.settings.at("retrorun_audio_buffer") == "735");
    assert(profile.settings.at("retrorun_audio_stable_buffer") == "false");
    assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "60");
    assert(profile.settings.at("retrorun_go2_audio_stretch_percent") == "0");
    assert(profile.settings.at("retrorun_go2_audio_stretch_low_ms") == "40");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "disabled");
    assert(profile.settings.at("reicast_internal_resolution") == "640x480");
    assert(profile.settings.at("reicast_anisotropic_filtering") == "off");
    assert(profile.settings.at("reicast_frame_skipping") == "disabled");
    assert(profile.settings.at("reicast_hle_bios") == "enabled");
    assert(profile.settings.at("reicast_gdrom_fast_loading") == "disabled");
    assert(profile.settings.at("reicast_mipmapping") == "disabled");
    assert(profile.settings.at("reicast_fog") == "disabled");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");
    assert(profile.settings.at("reicast_translucent_strip_merge") ==
           "disabled");
    assert(profile.settings.at("reicast_texture_storage_reuse") ==
           "disabled");
    assert(profile.settings.at("reicast_fast_depth") == "vertex_fast_log");
    assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
    assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "24");
    assert(profile.settings.at("reicast_framerate") == "fullspeed");
    assert(profile.settings.at("reicast_loop_declared_fps") == "false");
    assert(profile.settings.at("reicast_audio_mixer") == "lowend");

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
    assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "30");
    assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "disabled");
    assert(profile.settings.at("reicast_audio_mixer") == "accurate");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "32");

    assert(selectProfile(catalog, "T8116D50", Mode::BestPerformance,
                         profile, fallback, "RG353M"));
    assert(fallback);
    assert(profile.mode == Mode::BestValidated);
    assert(profile.settings.at("reicast_audio_mixer") == "accurate");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
    assert(profile.settings.at("reicast_render_queue_no_drop") == "enabled");

    assert(selectProfile(catalog, "RDC-0140", Mode::BestPerformance,
                         profile, fallback, "RG353M"));
    assert(fallback);
    assert(profile.mode == Mode::BestValidated);
    assert(profile.settings.at("retrorun_audio_buffer") == "735");
    assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "30");

    const auto mixedSettings =
        settingsForOptionPrefix(profile.settings, "flycast2021_");
    assert(mixedSettings.at("flycast2021_aica_arm_cycles") == "32");
    assert(mixedSettings.at("flycast2021_framerate") == "normal");

    const char *doa2Rg353Variants[] = {
        "RDC-0140", "RDC-0149", "T8116D50", "T3602M", "T3601M", "T3601N"
    };
    for (const char *product : doa2Rg353Variants)
    {
        assert(selectProfile(catalog, product, Mode::BestValidated,
                             profile, fallback, "RG353M"));
        assert(!fallback);
        assert(profile.settings.at("reicast_audio_mixer") == "accurate");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
    }

    const char *segaRallyVariants[] = {"MK-51019", "HDR-0010"};
    for (const char *product : segaRallyVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg353m"));
        assert(!fallback);
        assert(profile.mode == Mode::BestPerformance);
        assert(profile.settings.at("retrorun_loop_declared_fps") ==
               "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "1470");
        assert(profile.settings.at("retrorun_audio_stable_buffer") ==
               "true");
        assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") ==
               "60");
        assert(profile.settings.at("retrorun_go2_audio_stretch_percent") ==
               "10");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "lowend_stable_96");
        assert(profile.settings.count("retrorun_flycast_core_variant") == 0);
        assert(profile.settings.at("reicast_sh4clock") == "110");
        assert(profile.settings.at("reicast_sh4_cycle_mode") ==
               (std::string(product) == "MK-51019" ? "accurate" :
                                                    "legacy"));
        assert(profile.settings.at("reicast_shared_block_checks") == "enabled");
        assert(profile.settings.at("reicast_mmu_address_lut") == "enabled");
        assert(profile.settings.at("reicast_fmov_fpr64") == "enabled");
        assert(profile.settings.at("reicast_aica_better_lpf") == "enabled");
        assert(profile.settings.at("reicast_internal_resolution") ==
               "640x480");
        assert(profile.settings.at("reicast_alpha_sorting") ==
               "per-strip (fast, least accurate)");
        assert(profile.settings.at("reicast_frame_skipping") == "disabled");
        assert(profile.settings.at("reicast_audio_mixer") == "accurate");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "32");

        assert(profile.settings.count("reicast_render_queue_no_drop") == 0);
    }

    Profile segaRallyRg552Reference;
    for (const char *product : segaRallyVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestValidated,
                             profile, fallback, "rG552"));
        assert(!fallback);
        assert(profile.validated);
        assert(profile.mode == Mode::BestValidated);
        assert(profile.settings.count("retrorun_flycast_core_variant") == 0);
        assert(profile.settings.at("retrorun_audio_buffer") == "2048");
        assert(profile.settings.at("retrorun_audio_stable_buffer") == "true");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "disabled");
        assert(profile.settings.at("retrorun_video_multithread_mode") ==
               "enabled");
        assert(profile.settings.count("retrorun_force_video_multithread") == 0);
        assert(profile.settings.at("retrorun_loop_declared_fps") == "true");
        assert(profile.settings.at("reicast_threaded_rendering") == "enabled");
        assert(profile.settings.at("reicast_internal_resolution") ==
               "640x480");
        assert(profile.settings.at("reicast_anisotropic_filtering") == "off");
        assert(profile.settings.at("reicast_enable_dsp") == "disabled");
        assert(profile.settings.at("reicast_synchronous_rendering") ==
               "disabled");
        assert(profile.settings.at("reicast_enable_rttb") == "disabled");
        assert(profile.settings.at("reicast_delay_frame_swapping") ==
               "disabled");
        assert(profile.settings.at("reicast_alpha_sorting") ==
               "per-strip (fast, least accurate)");
        assert(profile.settings.at("reicast_div_matching") == "auto");
        assert(profile.settings.at("reicast_texupscale") == "1");
        assert(profile.settings.at("reicast_enable_purupuru") == "enabled");
        assert(profile.settings.at("reicast_auto_skip_frame") == "disabled");
        assert(profile.settings.at("reicast_gdrom_fast_loading") == "enabled");
        assert(profile.settings.at("reicast_volume_modifier_enable") ==
               "disabled");
        assert(profile.settings.at("reicast_framerate") == "fullspeed");
        assert(profile.settings.at("reicast_mmu_address_lut") == "enabled");
        assert(profile.settings.at("reicast_shared_block_checks") == "enabled");
        assert(profile.settings.at("reicast_fmov_fpr64") == "enabled");
        assert(profile.settings.at("reicast_aica_better_lpf") == "enabled");
        assert(profile.settings.at("reicast_sh4_cycle_mode") == "accurate");

        const auto rg552CoreSettings =
            settingsForOptionPrefix(profile.settings, "flycast2022_");
        assert(rg552CoreSettings.at("flycast2022_auto_skip_frame") ==
               "disabled");
        assert(rg552CoreSettings.at("flycast2022_texupscale") == "1");

        if (segaRallyRg552Reference.settings.empty())
            segaRallyRg552Reference = profile;
        else
            assert(profile.settings == segaRallyRg552Reference.settings);

        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "RG552"));
        assert(fallback);
        assert(profile.validated);
        assert(profile.mode == Mode::BestValidated);
        assert(profile.settings == segaRallyRg552Reference.settings);
        assert(profile.settings.count("retrorun_flycast_core_variant") == 0);
    }

    assert(selectProfile(catalog, "MK-51019", Mode::BestPerformance,
                         profile, fallback, "RG351MP"));
    assert(fallback);
    assert(profile.validated);
    assert(profile.mode == Mode::BestValidated);
    assert(profile.title == "Sega Rally 2 (USA, RK3326 validated)");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");
    assert(profile.settings.at("reicast_mipmapping") == "enabled");
    assert(profile.settings.at("reicast_fog") == "enabled");
    assert(profile.settings.at("reicast_volume_modifier_enable") ==
           "enabled");
    assert(profile.settings.at("reicast_enable_dsp") == "enabled");
    assert(profile.settings.at("reicast_translucent_strip_merge") ==
           "disabled");
    assert(profile.settings.at("reicast_texture_storage_reuse") ==
           "disabled");
    assert(profile.settings.at("reicast_fast_depth") == "disabled");
    assert(profile.settings.at("reicast_opaque_strip_merge") == "disabled");
    assert(profile.settings.at("reicast_audio_mixer") == "accurate");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
    assert(profile.settings.at("retrorun_audio_buffer") == "4096");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_heavy_100");
    assert(profile.settings.at("reicast_mmu_address_lut") == "enabled");
    assert(profile.settings.at("reicast_shared_block_checks") == "enabled");
    assert(profile.settings.at("reicast_accurate_aica_batch") == "disabled");
    assert(profile.settings.at("reicast_sh4_cycle_mode") == "accurate");

    fallback = false;
    assert(!selectProfile(catalog, "HDR-0010", Mode::BestPerformance,
                          profile, fallback, "RG351MP"));
    assert(!fallback);

    const char *upstream620Variants[] = {
        "T1401D50", "T1401N", "T1401M", "MK-51058",
        "T36801D61", "T36801D64", "T1201M",
        "T7013D50", "T1209M"
    };
    const std::set<std::string> rg351MpUpstream620 = {
        "T7013D50", "T1209M"
    };
    for (const char *product : upstream620Variants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "RG353M"));
        assert(profile.settings.at("retrorun_flycast_core_variant") ==
               "upstream_620");

        if (selectProfile(catalog, product, Mode::BestPerformance,
                          profile, fallback, "RG351MP"))
        {
            if (rg351MpUpstream620.count(product) != 0)
                assert(profile.settings.at("retrorun_flycast_core_variant") ==
                       "upstream_620");
            else
                assert(profile.settings.count(
                           "retrorun_flycast_core_variant") == 0);
        }
    }

    assert(selectProfile(catalog, "T1213N", Mode::BestPerformance,
                         profile, fallback, "RG353M"));
    assert(profile.settings.count("retrorun_flycast_core_variant") == 0);
    assert(profile.settings.at("reicast_render_queue_no_drop") == "enabled");
    for (const char *device : {"RG351MP", "RG351V"})
    {
        assert(selectProfile(catalog, "T1213N", Mode::BestPerformance,
                             profile, fallback, device));
        assert(!fallback);
        assert(profile.settings.count("retrorun_flycast_core_variant") == 0);
        assert(profile.settings.at("reicast_render_queue_no_drop") ==
               "enabled");
    }

    struct Rk3326Promotion
    {
        const char *product;
        const char *setting;
        const char *value;
    };
    const Rk3326Promotion rk3326Promotions[] = {
        {"T7010D50", "reicast_render_queue_no_drop", "enabled"},
        {"MK-51058", "reicast_render_queue_no_drop", "enabled"},
        {"MK-51035", "reicast_sh4_cycle_mode", "accurate"},
        {"MK-51054", "reicast_render_queue_no_drop", "enabled"},
        {"T1204N", "reicast_sh4_cycle_mode", "accurate"},
        {"T38706M", "reicast_sh4_cycle_mode", "accurate"},
    };
    for (const Rk3326Promotion &expected : rk3326Promotions)
    {
        assert(selectProfile(catalog, expected.product,
                             Mode::BestPerformance, profile, fallback,
                             "RG351V"));
        assert(!fallback);
        assert(profile.settings.at(expected.setting) == expected.value);
    }
    assert(selectProfile(catalog, "T7010D50", Mode::BestPerformance,
                         profile, fallback, "RG351V"));
    assert(profile.settings.at("reicast_frame_skipping") == "disabled");
    assert(selectProfile(catalog, "MK-51054", Mode::BestPerformance,
                         profile, fallback, "RG351V"));
    assert(profile.settings.at("reicast_frame_skipping") == "disabled");

    for (const char *product : {"T1201N", "MK-51049"})
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "RG353M"));
        assert(profile.settings.count("retrorun_flycast_core_variant") == 0);
        assert(profile.settings.at("reicast_render_queue_no_drop") ==
               "enabled");
    }

    assert(selectProfile(catalog, "RDC-0140", Mode::BestPerformance,
                         profile, fallback, "RG353M"));
    assert(profile.settings.count("retrorun_flycast_core_variant") == 0);

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
    assert(profile.settings.count("reicast_render_queue_no_drop") == 0);

    const char *soulCaliburVariants[] = {
        "T1401D50", "T1401N", "T1401M"
    };
    Profile soulCaliburRg552Reference;
    for (const char *product : soulCaliburVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestValidated,
                             profile, fallback, "rG552"));
        assert(!fallback);
        assert(profile.validated);
        assert(profile.mode == Mode::BestValidated);
        assert(profile.settings.count("retrorun_flycast_core_variant") == 0);
        assert(profile.settings.at("retrorun_loop_declared_fps") == "true");
        assert(profile.settings.at("retrorun_drm_direct_scanout") == "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "735");
        assert(profile.settings.at("retrorun_audio_stable_buffer") ==
               "false");
        assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") ==
               "60");
        assert(profile.settings.at("retrorun_go2_audio_stretch_percent") ==
               "0");
        assert(profile.settings.at("retrorun_go2_audio_stretch_low_ms") ==
               "40");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "disabled");
        assert(profile.settings.at("retrorun_video_multithread_mode") ==
               "enabled");
        assert(profile.settings.count("retrorun_force_video_multithread") ==
               0);
        assert(profile.settings.at("reicast_system") == "dreamcast");
        assert(profile.settings.at("reicast_boot_to_bios") == "disabled");
        assert(profile.settings.at("reicast_hle_bios") == "disabled");
        assert(profile.settings.at("reicast_internal_resolution") ==
               "640x480");
        assert(profile.settings.at("reicast_screen_rotation") ==
               "horizontal");
        assert(profile.settings.at("reicast_cpu_mode") ==
               "dynamic_recompiler");
        assert(profile.settings.at("reicast_sh4clock") == "200");
        assert(profile.settings.at("reicast_sh4_cycle_mode") == "legacy");
        assert(profile.settings.at("reicast_cable_type") ==
               "TV (Composite)");
        assert(profile.settings.at("reicast_broadcast") == "NTSC");
        assert(profile.settings.at("reicast_alpha_sorting") ==
               "per-strip (fast, least accurate)");
        assert(profile.settings.at("reicast_gdrom_fast_loading") ==
               "enabled");
        assert(profile.settings.at("reicast_mipmapping") == "enabled");
        assert(profile.settings.at("reicast_fog") == "enabled");
        assert(profile.settings.at("reicast_volume_modifier_enable") ==
               "disabled");
        assert(profile.settings.at("reicast_enable_dsp") == "disabled");
        assert(profile.settings.at("reicast_anisotropic_filtering") ==
               "disabled");
        assert(profile.settings.at("reicast_div_matching") == "auto");
        assert(profile.settings.at("reicast_texupscale") == "off");
        assert(profile.settings.at("reicast_enable_rttb") == "disabled");
        assert(profile.settings.at("reicast_enable_purupuru") == "enabled");
        assert(profile.settings.at("reicast_framerate") == "fullspeed");
        assert(profile.settings.at("reicast_threaded_rendering") ==
               "enabled");
        assert(profile.settings.at("reicast_synchronous_rendering") ==
               "disabled");
        assert(profile.settings.at("reicast_delay_frame_swapping") ==
               "disabled");
        assert(profile.settings.at("reicast_auto_skip_frame") == "disabled");
        assert(profile.settings.at("reicast_frame_skipping") == "disabled");
        assert(profile.settings.at("reicast_adjacent_state_elision") ==
               "disabled");
        assert(profile.settings.at("reicast_translucent_strip_merge") ==
               "disabled");
        assert(profile.settings.at("reicast_texture_storage_reuse") ==
               "disabled");
        assert(profile.settings.at("reicast_palette_fog_storage_reuse") ==
               "disabled");
        assert(profile.settings.at("reicast_fast_depth") == "disabled");
        assert(profile.settings.at("reicast_audio_mixer") == "accurate");
        assert(profile.settings.at("reicast_opaque_strip_merge") ==
               "disabled");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
        assert(profile.settings.at("reicast_accurate_aica_batch") ==
               "disabled");
        assert(profile.settings.at("reicast_shared_block_checks") ==
               "disabled");
        assert(profile.settings.at("reicast_mmu_address_lut") == "disabled");
        assert(profile.settings.at("reicast_fmov_fpr64") == "disabled");
        assert(profile.settings.at("reicast_aica_better_lpf") == "disabled");
        assert(profile.settings.at("reicast_render_queue_no_drop") ==
               "enabled");

        const auto rg552CoreSettings =
            settingsForOptionPrefix(profile.settings, "flycast2022_");
        assert(rg552CoreSettings.at("flycast2022_render_queue_no_drop") ==
               "enabled");
        assert(rg552CoreSettings.at("flycast2022_threaded_rendering") ==
               "enabled");
        assert(rg552CoreSettings.count("reicast_render_queue_no_drop") == 0);

        if (soulCaliburRg552Reference.settings.empty())
            soulCaliburRg552Reference = profile;
        else
            assert(profile.settings == soulCaliburRg552Reference.settings);

        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "RG552"));
        assert(fallback);
        assert(profile.validated);
        assert(profile.mode == Mode::BestValidated);
        assert(profile.settings == soulCaliburRg552Reference.settings);
        assert(profile.settings.count("retrorun_flycast_core_variant") == 0);
    }

    assert(selectProfile(catalog, "T1401N", Mode::BestPerformance,
                         profile, fallback));
    assert(!fallback);
    assert(profile.settings.count("reicast_render_queue_no_drop") == 0);
    assert(selectProfile(catalog, "MK-51019", Mode::BestValidated,
                         profile, fallback, "RG552"));
    assert(!fallback);
    assert(profile.settings.count("reicast_render_queue_no_drop") == 0);

    struct Rg552NoDropProfile
    {
        const char *product;
        bool disablesCoreFrameskip;
    };
    const Rg552NoDropProfile rg552NoDropProfiles[] = {
        {"MK-51049", false},
        {"T8116D50", false},
        {"T38706M", true},
        {"MK-51035", false},
        {"T7010D50", true},
        {"T1213N", false},
        {"MK-51054", true},
        {"T1215N", false},
        {"MK-51037", false},
        {"MK-51058", false},
        {"MK-51117", false},
        {"MK-5118450", false},
        {"MK-51000", false},
        {"T1201N", false},
        {"T1211N", false},
        {"MK-5100250", false},
    };
    for (const Rg552NoDropProfile &expected : rg552NoDropProfiles)
    {
        assert(selectProfile(catalog, expected.product, Mode::BestValidated,
                             profile, fallback, "rg552"));
        assert(!fallback);
        assert(profile.validated);
        assert(profile.mode == Mode::BestValidated);
        assert(profile.settings.count("retrorun_flycast_core_variant") == 0);
        assert(profile.settings.at("reicast_render_queue_no_drop") ==
               "enabled");
        if (expected.disablesCoreFrameskip)
            assert(profile.settings.at("reicast_frame_skipping") ==
                   "disabled");

        const Profile validated = profile;
        assert(selectProfile(catalog, expected.product,
                             Mode::BestPerformance, profile, fallback,
                             "RG552"));
        assert(fallback);
        assert(profile.settings == validated.settings);
    }

    assert(selectProfile(catalog, "T38706M", Mode::BestValidated,
                         profile, fallback, "RG552"));
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");

    assert(selectProfile(catalog, "MK-51117", Mode::BestValidated,
                         profile, fallback, "RG552"));
    assert(profile.settings.at("retrorun_audio_buffer") == "2048");
    assert(profile.settings.at("retrorun_audio_stable_buffer") == "true");
    assert(profile.settings.at("retrorun_go2_audio_stretch_low_ms") ==
           "150");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_stable_96");

    assert(selectProfile(catalog, "MK-5100250", Mode::BestValidated,
                         profile, fallback, "RG552"));
    assert(!fallback);
    assert(profile.settings.at("retrorun_audio_buffer") == "2048");
    assert(profile.settings.at("retrorun_audio_stable_buffer") == "true");
    assert(profile.settings.at("retrorun_go2_audio_stretch_low_ms") ==
           "150");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_stable_96");

    assert(!selectProfile(catalog, "T1201N", Mode::BestValidated,
                          profile, fallback));
    assert(!fallback);
    assert(!selectProfile(catalog, "T1211N", Mode::BestValidated,
                          profile, fallback));
    assert(!fallback);

    assert(selectProfile(catalog, "T1204N", Mode::BestValidated,
                         profile, fallback, "RG552"));
    assert(!fallback);
    assert(profile.validated);
    assert(profile.settings.at("reicast_sh4_cycle_mode") == "accurate");
    assert(profile.settings.count("reicast_render_queue_no_drop") == 0);
    const Profile codeVeronicaRg552 = profile;
    assert(selectProfile(catalog, "T1204N", Mode::BestPerformance,
                         profile, fallback, "RG552"));
    assert(fallback);
    assert(profile.settings == codeVeronicaRg552.settings);
    assert(selectProfile(catalog, "T1204N", Mode::BestValidated,
                         profile, fallback));
    assert(!fallback);
    assert(profile.settings.count("reicast_sh4_cycle_mode") == 0);

    assert(selectProfile(catalog, "T1204N", Mode::BestPerformance,
                         profile, fallback, "RG353M"));
    assert(!fallback);
    assert(profile.title ==
           "Resident Evil: Code Veronica (USA, RG353M validated)");
    assert(profile.settings.at("reicast_sh4_cycle_mode") == "accurate");
    assert(profile.settings.count("reicast_render_queue_no_drop") == 0);

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

    assert(selectProfile(catalog, "MK-5118450", Mode::BestPerformance,
                         profile, fallback, "RG351MP"));
    assert(!fallback);
    assert(profile.title ==
           "Shenmue II (Europe, RG351MP batch validated)");
    assert(profile.settings.at("reicast_accurate_aica_batch") == "enabled");
    assert(profile.settings.at("reicast_audio_mixer") == "accurate");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "lowend_heavy_100");

    const auto amberShenmueSettings =
        settingsForOptionPrefix(profile.settings, "flycast2022_");
    assert(amberShenmueSettings.at("flycast2022_accurate_aica_batch") ==
           "enabled");
    assert(amberShenmueSettings.count("reicast_accurate_aica_batch") == 0);

    assert(selectProfile(catalog, "MK-5118450", Mode::BestValidated,
                         profile, fallback, "RG351MP"));
    assert(!fallback);
    assert(profile.settings.at("reicast_accurate_aica_batch") == "disabled");

    assert(!selectProfile(catalog, "MK-5100250", Mode::BestPerformance,
                          profile, fallback));
    assert(!fallback);

    struct Rg353ValidatedProfile
    {
        const char *product;
        const char *title;
        const char *fastDepth;
        const char *paletteReuse;
        const char *adjacentElision;
    };
    const Rg353ValidatedProfile rg353Validated[] = {
        {"T1215N", "Cannon Spike (USA, RG353M validated)",
         "menu_guarded_shadow_safe", "disabled", "disabled"},
        {"MK-51037", "Daytona USA 2001 (USA, RG353M validated)",
         "enabled", "enabled", "enabled"},
        {"MK-5100250", "The House of the Dead 2 (Europe, RG353M validated)",
         "enabled", "enabled", "disabled"},
        {"MK-51058", "Jet Grind Radio (USA, RG353M validated)",
         "menu_guarded_shadow_safe", "disabled", "disabled"},
        {"MK-5118450", "Shenmue II (Europe, RG353M validated)",
         "menu_guarded_shadow_safe", "disabled", "disabled"},
    };
    for (const Rg353ValidatedProfile &expected : rg353Validated)
    {
        assert(selectProfile(catalog, expected.product,
                             Mode::BestPerformance, profile, fallback,
                             "rg353m"));
        assert(!fallback);
        assert(profile.mode == Mode::BestPerformance);
        assert(profile.title == expected.title);
        assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "735");
        assert(profile.settings.at("retrorun_audio_stable_buffer") == "true");
        assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "60");
        assert(profile.settings.at("retrorun_go2_audio_stretch_low_ms") == "40");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "disabled");
        assert(profile.settings.at("retrorun_egl_depth_bits") == "24");
        assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");
        assert(profile.settings.at("reicast_framerate") == "normal");
        assert(profile.settings.at("reicast_alpha_sorting") ==
               "per-strip (fast, least accurate)");
        assert(profile.settings.at("reicast_translucent_strip_merge") ==
               "disabled");
        assert(profile.settings.at("reicast_texture_storage_reuse") ==
               "enabled");
        assert(profile.settings.at("reicast_fast_depth") ==
               expected.fastDepth);
        assert(profile.settings.at("reicast_palette_fog_storage_reuse") ==
               expected.paletteReuse);
        assert(profile.settings.at("reicast_adjacent_state_elision") ==
               expected.adjacentElision);
        assert(profile.settings.at("reicast_audio_mixer") == "lowend");
        assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
        if (std::string(expected.product) == "T1215N")
        {
            assert(profile.settings.at("reicast_render_queue_no_drop") ==
                   "enabled");
            assert(profile.settings.count("retrorun_flycast_core_variant") ==
                   0);
        }
        else if (std::string(expected.product) == "MK-5100250")
        {
            assert(profile.settings.at("reicast_render_queue_no_drop") ==
                   "enabled");
            assert(profile.settings.at("retrorun_flycast_core_variant") ==
                   "renderq_wait8");
        }
        else if (std::string(expected.product) == "MK-51058")
        {
            assert(profile.settings.count("reicast_render_queue_no_drop") ==
                   0);
            assert(profile.settings.at("retrorun_flycast_core_variant") ==
                   "upstream_620");
        }
        else
        {
            assert(profile.settings.count("reicast_render_queue_no_drop") ==
                   0);
            assert(profile.settings.count("retrorun_flycast_core_variant") ==
                   0);
        }
    }

    assert(selectProfile(catalog, "T38706M", Mode::BestPerformance,
                         profile, fallback, "rg353m"));
    assert(!fallback);
    assert(profile.title == "Ikaruga (Japan, RG353M validated)");
    assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
    assert(profile.settings.at("retrorun_audio_buffer") == "735");
    assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "60");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "disabled");
    assert(profile.settings.at("reicast_frame_skipping") == "disabled");
    assert(profile.settings.at("reicast_internal_resolution") == "640x480");
    assert(profile.settings.at("reicast_framerate") == "normal");
    assert(profile.settings.at("reicast_anisotropic_filtering") == "off");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");
    assert(profile.settings.at("retrorun_flycast_core_variant") ==
           "renderq_wait8");
    assert(profile.settings.at("reicast_render_queue_no_drop") == "enabled");

    const char *marvelRg353Variants[] = {"T1212N", "T7010D50", "T1215M"};
    for (const char *product : marvelRg353Variants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg353m"));
        assert(!fallback);
        assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "735");
        assert(profile.settings.at("retrorun_audio_stable_buffer") == "false");
        assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "60");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "disabled");
        assert(profile.settings.at("reicast_alpha_sorting") ==
               "per-triangle (normal)");
        assert(profile.settings.at("reicast_frame_skipping") == "disabled");
        assert(profile.settings.at("reicast_audio_mixer") == "fast");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "16");
        if (std::string(product) == "T7010D50")
        {
            assert(profile.title ==
                   "Marvel vs. Capcom 2 (Europe, RG353M validated)");
            assert(profile.settings.at("reicast_render_queue_no_drop") ==
                   "enabled");
        }
        else
            assert(profile.settings.count("reicast_render_queue_no_drop") ==
                   0);
    }

    const char *powerStoneVariants[] = {
        "T36801D61", "T36801D64", "T1201M", "T1201N"
    };
    for (const char *product : powerStoneVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg353m"));
        assert(!fallback);
        assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "735");
        assert(profile.settings.at("retrorun_audio_stable_buffer") == "false");
        assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "60");
        assert(profile.settings.at("reicast_alpha_sorting") ==
               (std::string(product) == "T1201N"
                    ? "per-triangle (normal)"
                    : "per-strip (fast, least accurate)"));
        assert(profile.settings.at("reicast_translucent_strip_merge") ==
               "menu_guarded");
        assert(profile.settings.at("reicast_fast_depth") == "menu_guarded");
        assert(profile.settings.at("reicast_audio_mixer") == "fast");
        assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "16");
        if (std::string(product) == "T1201N")
        {
            assert(profile.title == "Power Stone (USA, RG353M validated)");
            assert(profile.settings.at("reicast_render_queue_no_drop") ==
                   "enabled");
            assert(profile.settings.count("retrorun_flycast_core_variant") ==
                   0);
        }
        else
        {
            assert(profile.settings.count("reicast_render_queue_no_drop") ==
                   0);
            assert(profile.settings.at("retrorun_flycast_core_variant") ==
                   "upstream_620");
        }
    }

    const char *crazyTaxiVariants[] = {"MK-51035", "HDR-0053"};
    for (const char *product : crazyTaxiVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg353m"));
        assert(!fallback);
        assert(profile.settings.at("retrorun_audio_buffer") == "735");
        assert(profile.settings.at("retrorun_audio_stable_buffer") == "false");
        assert(profile.settings.at("reicast_alpha_sorting") ==
               "per-strip (fast, least accurate)");
        assert(profile.settings.at("reicast_fast_depth") == "disabled");
        assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "16");
        if (std::string(product) == "MK-51035")
        {
            assert(profile.settings.at("retrorun_flycast_core_variant") ==
                   "renderq_wait8");
            assert(profile.settings.at("reicast_render_queue_no_drop") ==
                   "enabled");
            const auto coreSettings =
                settingsForOptionPrefix(profile.settings, "flycast2022_");
            assert(coreSettings.at("flycast2022_render_queue_no_drop") ==
                   "enabled");
            assert(coreSettings.at("retrorun_flycast_core_variant") ==
                   "renderq_wait8");
        }
        else
        {
            assert(profile.settings.count("retrorun_flycast_core_variant") ==
                   0);
            assert(profile.settings.count("reicast_render_queue_no_drop") ==
                   0);
        }
    }

    const char *virtuaTennisVariants[] = {"MK-51054", "HDR-0113"};
    for (const char *product : virtuaTennisVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg353m"));
        assert(!fallback);
        assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "1470");
        assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "60");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "disabled");
        assert(profile.settings.at("reicast_frame_skipping") == "disabled");
        assert(profile.settings.at("reicast_internal_resolution") ==
               "640x480");
        assert(profile.settings.at("reicast_framerate") == "normal");
        assert(profile.settings.at("reicast_anisotropic_filtering") == "off");
        assert(profile.settings.at("reicast_translucent_strip_merge") ==
               "menu_guarded");
        assert(profile.settings.at("reicast_translucent_menu_guard_strategy") ==
               "hud_last");
        assert(profile.settings.at("reicast_fast_depth") == "enabled");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "16");
        if (std::string(product) == "MK-51054")
        {
            assert(profile.title == "Virtua Tennis (RG353M validated)");
            assert(profile.settings.at("reicast_render_queue_no_drop") ==
                   "enabled");
        }
        else
            assert(profile.settings.count("reicast_render_queue_no_drop") ==
                   0);
    }

    const char *shenmueVariants[] = {
        "MK-51059", "MK-51131", "HDR-0016", "HDR-0031"
    };
    for (const char *product : shenmueVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg353m"));
        assert(!fallback);
        assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "-1");
        assert(profile.settings.at("retrorun_go2_audio_stretch_low_ms") ==
               "40");
        assert(profile.settings.at("reicast_hle_bios") == "enabled");
        assert(profile.settings.at("reicast_gdrom_fast_loading") == "enabled");
        assert(profile.settings.at("reicast_internal_resolution") ==
               "640x480");
        assert(profile.settings.at("reicast_framerate") == "normal");
        assert(profile.settings.at("reicast_anisotropic_filtering") == "off");
        assert(profile.settings.at("reicast_translucent_strip_merge") ==
               "disabled");
        assert(profile.settings.at("reicast_fast_depth") ==
               "vertex_fast_log");
        assert(profile.settings.at("reicast_audio_mixer") == "lowend");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "32");
    }

    assert(selectProfile(catalog, "MK-51049", Mode::BestPerformance,
                         profile, fallback, "rg353m"));
    assert(!fallback);
    assert(profile.title == "ChuChu Rocket (USA, RG353M validated)");
    assert(profile.settings.at("retrorun_audio_buffer") == "735");
    assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "60");
    assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
           "disabled");
    assert(profile.settings.at("reicast_internal_resolution") == "640x480");
    assert(profile.settings.at("reicast_framerate") == "normal");
    assert(profile.settings.at("reicast_anisotropic_filtering") == "off");
    assert(profile.settings.at("reicast_alpha_sorting") ==
           "per-triangle (normal)");
    assert(profile.settings.at("reicast_translucent_strip_merge") ==
           "menu_guarded");
    assert(profile.settings.at("reicast_fast_depth") == "menu_guarded");
    assert(profile.settings.at("reicast_aica_arm_cycles") == "16");
    assert(profile.settings.at("reicast_render_queue_no_drop") == "enabled");
    assert(profile.settings.count("retrorun_flycast_core_variant") == 0);

    struct PowerStone2Variant
    {
        const char *product;
        const char *title;
    };
    const PowerStone2Variant powerStone2Variants[] = {
        {"T36812D61", "Power Stone 2 (Europe 61, RG353M validated)"},
        {"T36812D64", "Power Stone 2 (Europe 64, RG353M validated)"},
        {"T1218M", "Power Stone 2 (Japan, RG353M validated)"},
        {"T1211N", "Power Stone 2 (USA, RG353M validated)"},
    };
    Profile powerStone2Reference;
    for (const PowerStone2Variant &expected : powerStone2Variants)
    {
        assert(selectProfile(catalog, expected.product,
                             Mode::BestPerformance, profile, fallback,
                             "rg353m"));
        assert(!fallback);
        assert(profile.mode == Mode::BestPerformance);
        assert(profile.title == expected.title);
        assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "735");
        assert(profile.settings.at("retrorun_audio_stable_buffer") == "true");
        assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "60");
        assert(profile.settings.at("retrorun_go2_audio_stretch_percent") ==
               "0");
        assert(profile.settings.at("retrorun_go2_audio_stretch_low_ms") ==
               "40");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "disabled");
        assert(profile.settings.at("retrorun_egl_depth_bits") == "24");
        assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");
        assert(profile.settings.at("reicast_hle_bios") == "enabled");
        assert(profile.settings.at("reicast_gdrom_fast_loading") ==
               "disabled");
        assert(profile.settings.at("reicast_internal_resolution") ==
               "640x480");
        assert(profile.settings.at("reicast_framerate") == "normal");
        assert(profile.settings.at("reicast_loop_declared_fps") == "false");
        assert(profile.settings.at("reicast_anisotropic_filtering") == "off");
        assert(profile.settings.at("reicast_alpha_sorting") ==
               "per-triangle (normal)");
        assert(profile.settings.at("reicast_mipmapping") == "disabled");
        assert(profile.settings.at("reicast_fog") == "disabled");
        assert(profile.settings.at("reicast_frame_skipping") == "disabled");
        assert(profile.settings.at("reicast_translucent_strip_merge") ==
               "menu_guarded");
        assert(profile.settings.at(
                   "reicast_translucent_menu_guard_strategy") == "hud_last");
        assert(profile.settings.at("reicast_texture_storage_reuse") ==
               "enabled");
        assert(profile.settings.at("reicast_palette_fog_storage_reuse") ==
               "disabled");
        assert(profile.settings.at("reicast_adjacent_state_elision") ==
               "disabled");
        assert(profile.settings.at("reicast_fast_depth") == "menu_guarded");
        assert(profile.settings.at("reicast_audio_mixer") == "fast");
        assert(profile.settings.at("reicast_opaque_strip_merge") == "enabled");
        assert(profile.settings.at("reicast_aica_arm_cycles") == "16");

        if (powerStone2Reference.settings.empty())
            powerStone2Reference = profile;
        else
            assert(profile.settings == powerStone2Reference.settings);
    }

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

    for (const char *product : streetFighterVariants)
    {
        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg351mp"));
        assert(!fallback);
        if (std::string(product) == "T1213N")
        {
            assert(profile.settings.count("retrorun_flycast_core_variant") ==
                   0);
            assert(profile.settings.at("reicast_render_queue_no_drop") ==
                   "enabled");
        }
        else
        {
            assert(profile.settings.at("retrorun_flycast_core_variant") ==
                   "upstream_620");
        }
        assert(profile.settings.at("reicast_alpha_sorting") ==
               "per-triangle (normal)");
        assert(profile.settings.at("reicast_fast_depth") ==
               "vertex_fast_log");
        assert(profile.settings.at("reicast_audio_mixer") == "accurate");

        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg351v"));
        if (std::string(product) == "T1213N")
        {
            assert(profile.settings.count("retrorun_flycast_core_variant") ==
                   0);
            assert(profile.settings.at("reicast_render_queue_no_drop") ==
                   "enabled");
        }
        else
        {
            assert(profile.settings.at("retrorun_flycast_core_variant") ==
                   "upstream_620");
        }

        assert(selectProfile(catalog, product, Mode::BestPerformance,
                             profile, fallback, "rg353m"));
        assert(!fallback);
        assert(profile.settings.at("retrorun_loop_declared_fps") == "false");
        assert(profile.settings.at("retrorun_audio_buffer") == "735");
        assert(profile.settings.at("retrorun_go2_audio_prebuffer_ms") == "60");
        assert(profile.settings.at("retrorun_go2_audio_wsola_profile") ==
               "disabled");
        assert(profile.settings.at("retrorun_egl_stencil_bits") == "0");
        assert(profile.settings.at("reicast_gdrom_fast_loading") == "enabled");
        assert(profile.settings.at("reicast_internal_resolution") ==
               "640x480");
        assert(profile.settings.at("reicast_framerate") == "normal");
        assert(profile.settings.at("reicast_anisotropic_filtering") == "off");
        assert(profile.settings.at("reicast_texture_storage_reuse") ==
               "enabled");
        assert(profile.settings.at("reicast_fast_depth") ==
               "menu_guarded_shadow_safe");
        assert(profile.settings.at("reicast_audio_mixer") == "lowend");
        if (std::string(product) == "T1213N")
        {
            assert(profile.settings.count("retrorun_flycast_core_variant") ==
                   0);
            assert(profile.settings.at("reicast_render_queue_no_drop") ==
                   "enabled");
        }
        else
        {
            assert(profile.settings.at("retrorun_flycast_core_variant") ==
                   "upstream_620");
            assert(profile.settings.count("reicast_render_queue_no_drop") ==
                   0);
        }
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
    assertSameCatalogPayload(fileCatalog, builtIn);

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

    assert(selectProfile(fileCatalog, "MK-51019",
                         Mode::BestPerformance, fromFile, fileFallback,
                         "RG351MP"));
    assert(selectProfile(builtIn, "MK-51019",
                         Mode::BestPerformance, fromBuiltIn,
                         builtInFallback, "RG351MP"));
    assert(fileFallback && builtInFallback);
    assert(fromFile.title == fromBuiltIn.title);
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

    const char *rg552SoulProducts[] = {
        "T1401D50", "T1401N", "T1401M"
    };
    for (const char *product : rg552SoulProducts)
    {
        assert(selectProfile(fileCatalog, product,
                             Mode::BestPerformance, fromFile,
                             fileFallback, "RG552"));
        assert(selectProfile(builtIn, product,
                             Mode::BestPerformance, fromBuiltIn,
                             builtInFallback, "RG552"));
        assert(fileFallback && builtInFallback);
        assert(fromFile.mode == Mode::BestValidated);
        assert(fromBuiltIn.mode == Mode::BestValidated);
        assert(fromFile.title == fromBuiltIn.title);
        assert(fromFile.settings == fromBuiltIn.settings);
        assert(fromFile.settings.at("reicast_render_queue_no_drop") ==
               "enabled");
    }

    const char *rg353Products[] = {
        "T1215N", "MK-51037", "MK-5100250", "MK-51058", "MK-5118450",
        "T38706M", "MK-51054", "HDR-0113", "MK-51059", "MK-51131",
        "HDR-0016", "HDR-0031", "MK-51049", "T7013D50", "T1213N",
        "T1209M", "MK-51000", "T36812D61", "T36812D64", "T1218M",
        "T1211N"
    };
    for (const char *product : rg353Products)
    {
        assert(selectProfile(fileCatalog, product, Mode::BestPerformance,
                             fromFile, fileFallback, "RG353M"));
        assert(selectProfile(builtIn, product, Mode::BestPerformance,
                             fromBuiltIn, builtInFallback, "RG353M"));
        assert(!fileFallback && !builtInFallback);
        assert(fromFile.title == fromBuiltIn.title);
        assert(fromFile.settings == fromBuiltIn.settings);
    }

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
        "profile.TEST-1.best_performance.reicast_fast_depth = vertex_fast_log\n"
        "profile.META.best_validated.title = Metadata Only (baseline)\n");

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
    assert(catalog.profiles.count("META") == 1);
    assert(validatedCatalogProfiles(catalog).count("META") == 0);
    assert(!selectProfile(catalog, "META", Mode::BestValidated,
                          profile, fallback));
    assert(!fallback);
}

void testChipDetectionAndFamilySelection()
{
    const std::map<std::string, std::string> expectedChips = {
        {"RG351P", "RK3326"},
        {"RG351M", "RK3326"},
        {"RG351V", "RK3326"},
        {"RG351MP", "RK3326"},
        {"RGB20S", "RK3326"},
        {"XU10", "RK3326"},
        {"R35S", "RK3326"},
        {"RG552", "RK3399"},
        {"RG503", "RK3566"},
        {"RG353P", "RK3566"},
        {"RG353PS", "RK3566"},
        {"RG353V", "RK3566"},
        {"RG353VS", "RK3566"},
        {"RG353M", "RK3566"},
    };
    assert(chipForDeviceName("rk3326") == "RK3326");
    assert(chipForDeviceName("rk3399") == "RK3399");
    assert(chipForDeviceName("rk3566") == "RK3566");
    for (const auto &[device, chip] : expectedChips)
    {
        assert(chipForDeviceName(device) == chip);
        assert(detectDeviceChip(" " + device + " ") == chip);
    }
    assert(chipForDeviceName("unknown") == "");

    const char rk3326Data[] =
        "gameconsole,r35s\0rockchip,rk3326\0";
    const char rk3399Data[] =
        "anbernic,rg552\0rockchip,rk3399\0";
    const char rk3566Data[] =
        "anbernic,rg353m\0rockchip,rk3566\0";
    const char unknownData[] = "vendor,board\0vendor,soc\0";
    assert(chipFromDeviceTreeCompatible(
               std::string(rk3326Data, sizeof(rk3326Data) - 1)) == "RK3326");
    assert(chipFromDeviceTreeCompatible(
               std::string(rk3399Data, sizeof(rk3399Data) - 1)) == "RK3399");
    assert(chipFromDeviceTreeCompatible(
               std::string(rk3566Data, sizeof(rk3566Data) - 1)) == "RK3566");
    assert(chipFromDeviceTreeCompatible(
               std::string(unknownData, sizeof(unknownData) - 1)).empty());

    const Catalog catalog = builtinCatalog();
    Profile reference;
    Profile candidate;
    bool referenceFallback = false;
    bool candidateFallback = false;

    const auto assertWholeChipCatalog =
        [&catalog](const std::string &chip,
                   const std::vector<std::string> &devices)
    {
        const auto chipProfiles = catalog.chip_profiles.find(chip);
        assert(chipProfiles != catalog.chip_profiles.end());
        for (const auto &[product, modes] : chipProfiles->second)
        {
            for (const auto &[mode, expected] : modes)
            {
                for (const std::string &device : devices)
                {
                    Profile selected;
                    bool fallback = false;
                    assert(selectProfile(catalog, product, mode, selected,
                                         fallback, device));
                    assert(!fallback);
                    assertSameProfile(expected, selected);
                }
            }
        }
    };
    assertWholeChipCatalog(
        "RK3326", {"RG351P", "RG351M", "RG351V", "RG351MP",
                   "RGB20S", "XU10", "R35S"});
    assertWholeChipCatalog(
        "RK3399", {"RG552"});
    assertWholeChipCatalog(
        "RK3566", {"RG503", "RG353P", "RG353PS", "RG353V",
                   "RG353VS", "RG353M"});

    assert(selectProfile(catalog, "MK-51019", Mode::BestPerformance,
                         reference, referenceFallback, "RG351MP"));
    assert(selectProfile(catalog, "MK-51019", Mode::BestPerformance,
                         candidate, candidateFallback, "RG351P", "RK3399"));
    assert(referenceFallback == candidateFallback);
    assertSameProfile(reference, candidate);
    const char *rk3326Devices[] = {
        "RG351P", "RG351M", "RG351V", "RG351MP",
        "RGB20S", "XU10", "R35S"
    };
    for (const char *device : rk3326Devices)
    {
        assert(selectProfile(catalog, "MK-51019", Mode::BestPerformance,
                             candidate, candidateFallback, device));
        assert(referenceFallback == candidateFallback);
        assertSameProfile(reference, candidate);
    }

    assert(selectProfile(catalog, "T38706M", Mode::BestPerformance,
                         reference, referenceFallback, "RG353M"));
    const char *rk3566Devices[] = {
        "RG503", "RG353P", "RG353PS", "RG353V", "RG353VS", "RG353M"
    };
    for (const char *device : rk3566Devices)
    {
        assert(selectProfile(catalog, "T38706M", Mode::BestPerformance,
                             candidate, candidateFallback, device));
        assert(referenceFallback == candidateFallback);
        assertSameProfile(reference, candidate);
    }

    assert(selectProfile(catalog, "T1401N", Mode::BestValidated,
                         reference, referenceFallback, "RG552"));
    assert(selectProfile(catalog, "T1401N", Mode::BestValidated,
                         candidate, candidateFallback, "unknown-board",
                         "RK3399"));
    assertSameProfile(reference, candidate);
}

void testDeviceOverrideWinsOverChipProfile()
{
    std::istringstream input(
        "schema_version = 3\n"
        "catalog_version = 2\n"
        "default.reicast_render_queue_no_drop = disabled\n"
        "profile.TEST.best_validated.title = Test Game\n"
        "profile.TEST.best_validated.reicast_audio_mixer = accurate\n"
        "profile.TEST.best_performance.inherits = best_validated\n"
        "profile.TEST.best_performance.reicast_fast_depth = vertex_fast_log\n"
        "chip.RK3326.profile.TEST.best_performance.reicast_audio_mixer = lowend\n"
        "chip.RK3326.profile.TEST.best_performance.reicast_render_queue_no_drop = enabled\n"
        "device.RG351V.profile.TEST.best_performance.reicast_render_queue_no_drop = disabled\n");

    Catalog catalog;
    std::vector<std::string> diagnostics;
    assert(parseCatalog(input, "memory", catalog, diagnostics));
    assert(diagnostics.empty());
    assert(catalog.chip_profiles.size() == 1);
    assert(catalog.device_profiles.size() == 1);

    Profile chipProfile;
    Profile deviceProfile;
    bool fallback = false;
    assert(selectProfile(catalog, "TEST", Mode::BestPerformance,
                         chipProfile, fallback, "RG351P"));
    assert(!fallback);
    assert(chipProfile.settings.at("reicast_audio_mixer") == "lowend");
    assert(chipProfile.settings.at("reicast_fast_depth") ==
           "vertex_fast_log");
    assert(chipProfile.settings.at("reicast_render_queue_no_drop") ==
           "enabled");

    assert(selectProfile(catalog, "TEST", Mode::BestPerformance,
                         deviceProfile, fallback, "RG351V"));
    assert(!fallback);
    assert(deviceProfile.title == "Test Game");
    assert(deviceProfile.settings.at("reicast_audio_mixer") == "lowend");
    assert(deviceProfile.settings.at("reicast_fast_depth") ==
           "vertex_fast_log");
    assert(deviceProfile.settings.at("reicast_render_queue_no_drop") ==
           "disabled");
}

void testChipProfilesRequireSchemaThree()
{
    std::istringstream input(
        "schema_version = 2\n"
        "catalog_version = 1\n"
        "profile.TEST.best_validated.title = Test Game\n"
        "profile.TEST.best_validated.reicast_audio_mixer = accurate\n"
        "chip.RK3326.profile.TEST.best_validated.reicast_audio_mixer = lowend\n");
    Catalog catalog;
    std::vector<std::string> diagnostics;
    assert(!parseCatalog(input, "invalid-chip-schema", catalog, diagnostics));
    assert(!diagnostics.empty());
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
    testChipDetectionAndFamilySelection();
    testDeviceOverrideWinsOverChipProfile();
    testChipProfilesRequireSchemaThree();
    testInvalidCatalogIsRejected();
    return 0;
}
