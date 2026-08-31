#include "config/flycast_game_catalog.h"

#include "config/config_file.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <tuple>
#include <unordered_set>

namespace rr::flycast_profiles
{
namespace
{

constexpr int MinimumSchemaVersion = 1;
constexpr int CurrentSchemaVersion = 2;
constexpr const char *CatalogFilename = "flycast-game-catalog.ini";

const char *BuiltinCatalogText = R"catalog(
schema_version = 2
catalog_version = 20260921

default.retrorun_vsync = false
default.retrorun_loop_declared_fps = true
default.retrorun_adaptive_frameskip = false
default.retrorun_frameskip = 0
default.retrorun_audio_stable_buffer = true
default.retrorun_force_audio_multithread = false
default.retrorun_egl_depth_bits = 24
default.retrorun_egl_stencil_bits = 8
default.reicast_system = dreamcast
default.reicast_cpu_mode = dynamic_recompiler
default.reicast_sh4clock = 200
default.reicast_alpha_sorting = per-strip (fast, least accurate)
default.reicast_mipmapping = enabled
default.reicast_fog = enabled
default.reicast_volume_modifier_enable = disabled
default.reicast_enable_dsp = disabled
default.reicast_threaded_rendering = enabled
default.reicast_synchronous_rendering = disabled
default.reicast_delay_frame_swapping = disabled
default.reicast_frame_skipping = disabled
default.reicast_adjacent_state_elision = disabled
default.reicast_translucent_strip_merge = menu_guarded
default.reicast_translucent_menu_guard_strategy = scored
default.reicast_translucent_menu_guard_max_vertices = 64
default.reicast_translucent_menu_guard_risk = 5
default.reicast_translucent_menu_guard_depth_tolerance = 0.0001
default.reicast_translucent_menu_guard_overlap = risky
default.reicast_translucent_menu_guard_draw_sorting = standard
default.reicast_texture_storage_reuse = enabled
default.reicast_palette_fog_storage_reuse = disabled
default.reicast_fast_depth = disabled
default.reicast_audio_mixer = accurate
default.reicast_opaque_strip_merge = disabled
default.reicast_aica_arm_cycles = 32
default.reicast_accurate_aica_batch = disabled
default.reicast_shared_block_checks = disabled
default.reicast_mmu_address_lut = disabled
default.reicast_fmov_fpr64 = disabled
default.reicast_aica_better_lpf = disabled

profile.MK-51117.best_validated.title = Sonic Adventure 2
profile.MK-51117.best_validated.reicast_hle_bios = disabled
profile.MK-51117.best_validated.reicast_gdrom_fast_loading = enabled
profile.MK-51117.best_validated.reicast_translucent_strip_merge = disabled
profile.MK-51117.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
profile.MK-51117.best_validated.reicast_fast_depth = menu_guarded_shadow_safe
profile.MK-51117.best_validated.reicast_audio_mixer = lowend

profile.MK-51117.best_performance.title = Sonic Adventure 2 (aggressive)
profile.MK-51117.best_performance.inherits = best_validated
profile.MK-51117.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
profile.MK-51117.best_performance.reicast_opaque_strip_merge = enabled
profile.MK-51117.best_performance.retrorun_audio_buffer = 2048
profile.MK-51117.best_performance.retrorun_go2_audio_stretch_low_ms = 150
profile.MK-51117.best_performance.retrorun_go2_audio_wsola_profile = lowend_stable_96

profile.HDR-0165.best_validated.title = Sonic Adventure 2 (Japan)
profile.HDR-0165.best_validated.reicast_hle_bios = disabled
profile.HDR-0165.best_validated.reicast_gdrom_fast_loading = enabled
profile.HDR-0165.best_validated.reicast_translucent_strip_merge = disabled
profile.HDR-0165.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
profile.HDR-0165.best_validated.reicast_fast_depth = menu_guarded_shadow_safe
profile.HDR-0165.best_validated.reicast_audio_mixer = lowend

profile.HDR-0165.best_performance.title = Sonic Adventure 2 (Japan, aggressive)
profile.HDR-0165.best_performance.inherits = best_validated
profile.HDR-0165.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
profile.HDR-0165.best_performance.reicast_opaque_strip_merge = enabled
profile.HDR-0165.best_performance.retrorun_audio_buffer = 2048
profile.HDR-0165.best_performance.retrorun_go2_audio_stretch_low_ms = 150
profile.HDR-0165.best_performance.retrorun_go2_audio_wsola_profile = lowend_stable_96

device.RG353M.profile.MK-51117.best_performance.title = Sonic Adventure 2 (RG353M validated)
device.RG353M.profile.MK-51117.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-51117.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.MK-51117.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.MK-51117.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-51117.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51117.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.MK-51117.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.MK-51117.best_performance.retrorun_egl_stencil_bits = 8
device.RG353M.profile.MK-51117.best_performance.reicast_hle_bios = disabled
device.RG353M.profile.MK-51117.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.MK-51117.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.MK-51117.best_performance.reicast_fog = enabled
device.RG353M.profile.MK-51117.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.MK-51117.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.MK-51117.best_performance.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.MK-51117.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
device.RG353M.profile.MK-51117.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.MK-51117.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.MK-51117.best_performance.reicast_aica_arm_cycles = 8

device.RG353M.profile.HDR-0165.best_performance.title = Sonic Adventure 2 (Japan, RG353M validated)
device.RG353M.profile.HDR-0165.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.HDR-0165.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.HDR-0165.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.HDR-0165.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.HDR-0165.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.HDR-0165.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.HDR-0165.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.HDR-0165.best_performance.retrorun_egl_stencil_bits = 8
device.RG353M.profile.HDR-0165.best_performance.reicast_hle_bios = disabled
device.RG353M.profile.HDR-0165.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.HDR-0165.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.HDR-0165.best_performance.reicast_fog = enabled
device.RG353M.profile.HDR-0165.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.HDR-0165.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.HDR-0165.best_performance.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.HDR-0165.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
device.RG353M.profile.HDR-0165.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.HDR-0165.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.HDR-0165.best_performance.reicast_aica_arm_cycles = 8

profile.RDC-0149.best_validated.title = Dead or Alive 2
profile.RDC-0149.best_validated.reicast_hle_bios = enabled
profile.RDC-0149.best_validated.reicast_gdrom_fast_loading = disabled
profile.RDC-0149.best_validated.reicast_mipmapping = disabled
profile.RDC-0149.best_validated.reicast_fog = disabled
profile.RDC-0149.best_validated.reicast_fast_depth = vertex_fast_log
profile.RDC-0149.best_validated.reicast_opaque_strip_merge = enabled
profile.RDC-0149.best_validated.retrorun_egl_stencil_bits = 0
profile.RDC-0149.best_performance.title = Dead or Alive 2 (best performance)
profile.RDC-0149.best_performance.inherits = best_validated
profile.RDC-0149.best_performance.reicast_aica_arm_cycles = 8
profile.RDC-0149.best_performance.reicast_framerate = normal
profile.RDC-0149.best_performance.reicast_loop_declared_fps = false
profile.RDC-0149.best_performance.retrorun_loop_declared_fps = true
profile.RDC-0149.best_performance.reicast_audio_mixer = lowend
profile.RDC-0149.best_performance.reicast_threaded_rendering = enabled
profile.RDC-0149.best_performance.retrorun_adaptive_frameskip = false
profile.RDC-0149.best_performance.reicast_frame_skipping = adaptive
profile.RDC-0149.best_performance.reicast_anisotropic_filtering = off
profile.RDC-0149.best_performance.retrorun_audio_buffer = 4096
profile.RDC-0149.best_performance.retrorun_go2_audio_stretch_low_ms = 200
profile.RDC-0149.best_performance.retrorun_go2_audio_wsola_profile = doa_stable_100

profile.RDC-0140.best_validated.title = Dead or Alive 2 (observed CDI)
profile.RDC-0140.best_validated.reicast_hle_bios = enabled
profile.RDC-0140.best_validated.reicast_gdrom_fast_loading = disabled
profile.RDC-0140.best_validated.reicast_mipmapping = disabled
profile.RDC-0140.best_validated.reicast_fog = disabled
profile.RDC-0140.best_validated.reicast_fast_depth = vertex_fast_log
profile.RDC-0140.best_validated.reicast_opaque_strip_merge = enabled
profile.RDC-0140.best_validated.retrorun_egl_stencil_bits = 0
profile.RDC-0140.best_performance.title = Dead or Alive 2 (observed CDI, best performance)
profile.RDC-0140.best_performance.inherits = best_validated
profile.RDC-0140.best_performance.reicast_aica_arm_cycles = 8
profile.RDC-0140.best_performance.reicast_framerate = normal
profile.RDC-0140.best_performance.reicast_loop_declared_fps = false
profile.RDC-0140.best_performance.retrorun_loop_declared_fps = true
profile.RDC-0140.best_performance.reicast_audio_mixer = lowend
profile.RDC-0140.best_performance.reicast_threaded_rendering = enabled
profile.RDC-0140.best_performance.retrorun_adaptive_frameskip = false
profile.RDC-0140.best_performance.reicast_frame_skipping = adaptive
profile.RDC-0140.best_performance.reicast_anisotropic_filtering = off
profile.RDC-0140.best_performance.retrorun_audio_buffer = 4096
profile.RDC-0140.best_performance.retrorun_go2_audio_stretch_low_ms = 200
profile.RDC-0140.best_performance.retrorun_go2_audio_wsola_profile = doa_stable_100

profile.T8116D50.best_validated.title = Dead or Alive 2 (Europe)
profile.T8116D50.best_validated.reicast_hle_bios = enabled
profile.T8116D50.best_validated.reicast_gdrom_fast_loading = disabled
profile.T8116D50.best_validated.reicast_mipmapping = disabled
profile.T8116D50.best_validated.reicast_fog = disabled
profile.T8116D50.best_validated.reicast_fast_depth = vertex_fast_log
profile.T8116D50.best_validated.reicast_opaque_strip_merge = enabled
profile.T8116D50.best_validated.retrorun_egl_stencil_bits = 0
profile.T8116D50.best_performance.title = Dead or Alive 2 (Europe, best performance)
profile.T8116D50.best_performance.inherits = best_validated
profile.T8116D50.best_performance.reicast_aica_arm_cycles = 8
profile.T8116D50.best_performance.reicast_framerate = normal
profile.T8116D50.best_performance.reicast_loop_declared_fps = false
profile.T8116D50.best_performance.retrorun_loop_declared_fps = true
profile.T8116D50.best_performance.reicast_audio_mixer = lowend
profile.T8116D50.best_performance.reicast_threaded_rendering = enabled
profile.T8116D50.best_performance.retrorun_adaptive_frameskip = false
profile.T8116D50.best_performance.reicast_frame_skipping = adaptive
profile.T8116D50.best_performance.reicast_anisotropic_filtering = off
profile.T8116D50.best_performance.retrorun_audio_buffer = 4096
profile.T8116D50.best_performance.retrorun_go2_audio_stretch_low_ms = 200
profile.T8116D50.best_performance.retrorun_go2_audio_wsola_profile = doa_stable_100

profile.T3602M.best_validated.title = Dead or Alive 2 (Japan)
profile.T3602M.best_validated.reicast_hle_bios = enabled
profile.T3602M.best_validated.reicast_gdrom_fast_loading = disabled
profile.T3602M.best_validated.reicast_mipmapping = disabled
profile.T3602M.best_validated.reicast_fog = disabled
profile.T3602M.best_validated.reicast_fast_depth = vertex_fast_log
profile.T3602M.best_validated.reicast_opaque_strip_merge = enabled
profile.T3602M.best_validated.retrorun_egl_stencil_bits = 0
profile.T3602M.best_performance.title = Dead or Alive 2 (Japan, best performance)
profile.T3602M.best_performance.inherits = best_validated
profile.T3602M.best_performance.reicast_aica_arm_cycles = 8
profile.T3602M.best_performance.reicast_framerate = normal
profile.T3602M.best_performance.reicast_loop_declared_fps = false
profile.T3602M.best_performance.retrorun_loop_declared_fps = true
profile.T3602M.best_performance.reicast_audio_mixer = lowend
profile.T3602M.best_performance.reicast_threaded_rendering = enabled
profile.T3602M.best_performance.retrorun_adaptive_frameskip = false
profile.T3602M.best_performance.reicast_frame_skipping = adaptive
profile.T3602M.best_performance.reicast_anisotropic_filtering = off
profile.T3602M.best_performance.retrorun_audio_buffer = 4096
profile.T3602M.best_performance.retrorun_go2_audio_stretch_low_ms = 200
profile.T3602M.best_performance.retrorun_go2_audio_wsola_profile = doa_stable_100

profile.T3601M.best_validated.title = Dead or Alive 2 (Japan, limited)
profile.T3601M.best_validated.reicast_hle_bios = enabled
profile.T3601M.best_validated.reicast_gdrom_fast_loading = disabled
profile.T3601M.best_validated.reicast_mipmapping = disabled
profile.T3601M.best_validated.reicast_fog = disabled
profile.T3601M.best_validated.reicast_fast_depth = vertex_fast_log
profile.T3601M.best_validated.reicast_opaque_strip_merge = enabled
profile.T3601M.best_validated.retrorun_egl_stencil_bits = 0
profile.T3601M.best_performance.title = Dead or Alive 2 (Japan, limited, best performance)
profile.T3601M.best_performance.inherits = best_validated
profile.T3601M.best_performance.reicast_aica_arm_cycles = 8
profile.T3601M.best_performance.reicast_framerate = normal
profile.T3601M.best_performance.reicast_loop_declared_fps = false
profile.T3601M.best_performance.retrorun_loop_declared_fps = true
profile.T3601M.best_performance.reicast_audio_mixer = lowend
profile.T3601M.best_performance.reicast_threaded_rendering = enabled
profile.T3601M.best_performance.retrorun_adaptive_frameskip = false
profile.T3601M.best_performance.reicast_frame_skipping = adaptive
profile.T3601M.best_performance.reicast_anisotropic_filtering = off
profile.T3601M.best_performance.retrorun_audio_buffer = 4096
profile.T3601M.best_performance.retrorun_go2_audio_stretch_low_ms = 200
profile.T3601M.best_performance.retrorun_go2_audio_wsola_profile = doa_stable_100

profile.T3601N.best_validated.title = Dead or Alive 2 (North America)
profile.T3601N.best_validated.reicast_hle_bios = enabled
profile.T3601N.best_validated.reicast_gdrom_fast_loading = disabled
profile.T3601N.best_validated.reicast_mipmapping = disabled
profile.T3601N.best_validated.reicast_fog = disabled
profile.T3601N.best_validated.reicast_fast_depth = vertex_fast_log
profile.T3601N.best_validated.reicast_opaque_strip_merge = enabled
profile.T3601N.best_validated.retrorun_egl_stencil_bits = 0
profile.T3601N.best_performance.title = Dead or Alive 2 (North America, best performance)
profile.T3601N.best_performance.inherits = best_validated
profile.T3601N.best_performance.reicast_aica_arm_cycles = 8
profile.T3601N.best_performance.reicast_framerate = normal
profile.T3601N.best_performance.reicast_loop_declared_fps = false
profile.T3601N.best_performance.retrorun_loop_declared_fps = true
profile.T3601N.best_performance.reicast_audio_mixer = lowend
profile.T3601N.best_performance.reicast_threaded_rendering = enabled
profile.T3601N.best_performance.retrorun_adaptive_frameskip = false
profile.T3601N.best_performance.reicast_frame_skipping = adaptive
profile.T3601N.best_performance.reicast_anisotropic_filtering = off
profile.T3601N.best_performance.retrorun_audio_buffer = 4096
profile.T3601N.best_performance.retrorun_go2_audio_stretch_low_ms = 200
profile.T3601N.best_performance.retrorun_go2_audio_wsola_profile = doa_stable_100

device.RG353M.profile.RDC-0149.best_validated.title = Dead or Alive 2 (RG353M validated)
device.RG353M.profile.RDC-0149.best_validated.retrorun_loop_declared_fps = false
device.RG353M.profile.RDC-0149.best_validated.retrorun_audio_buffer = 735
device.RG353M.profile.RDC-0149.best_validated.retrorun_audio_stable_buffer = false
device.RG353M.profile.RDC-0149.best_validated.retrorun_go2_audio_prebuffer_ms = 30
device.RG353M.profile.RDC-0149.best_validated.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.RDC-0149.best_validated.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.RDC-0149.best_validated.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.RDC-0149.best_validated.reicast_hle_bios = disabled
device.RG353M.profile.RDC-0149.best_validated.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.RDC-0149.best_validated.reicast_framerate = normal
device.RG353M.profile.RDC-0149.best_validated.reicast_loop_declared_fps = false
device.RG353M.profile.RDC-0149.best_validated.reicast_frame_skipping = disabled
device.RG353M.profile.RDC-0149.best_validated.reicast_anisotropic_filtering = off
device.RG353M.profile.RDC-0149.best_validated.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.RDC-0149.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.RDC-0149.best_validated.reicast_audio_mixer = accurate
device.RG353M.profile.RDC-0149.best_validated.reicast_aica_arm_cycles = 32

device.RG353M.profile.RDC-0140.best_validated.title = Dead or Alive 2 (observed CDI, RG353M validated)
device.RG353M.profile.RDC-0140.best_validated.retrorun_loop_declared_fps = false
device.RG353M.profile.RDC-0140.best_validated.retrorun_audio_buffer = 735
device.RG353M.profile.RDC-0140.best_validated.retrorun_audio_stable_buffer = false
device.RG353M.profile.RDC-0140.best_validated.retrorun_go2_audio_prebuffer_ms = 30
device.RG353M.profile.RDC-0140.best_validated.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.RDC-0140.best_validated.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.RDC-0140.best_validated.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.RDC-0140.best_validated.reicast_hle_bios = disabled
device.RG353M.profile.RDC-0140.best_validated.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.RDC-0140.best_validated.reicast_framerate = normal
device.RG353M.profile.RDC-0140.best_validated.reicast_loop_declared_fps = false
device.RG353M.profile.RDC-0140.best_validated.reicast_frame_skipping = disabled
device.RG353M.profile.RDC-0140.best_validated.reicast_anisotropic_filtering = off
device.RG353M.profile.RDC-0140.best_validated.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.RDC-0140.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.RDC-0140.best_validated.reicast_audio_mixer = accurate
device.RG353M.profile.RDC-0140.best_validated.reicast_aica_arm_cycles = 32

device.RG353M.profile.T8116D50.best_validated.title = Dead or Alive 2 (Europe, RG353M validated)
device.RG353M.profile.T8116D50.best_validated.retrorun_loop_declared_fps = false
device.RG353M.profile.T8116D50.best_validated.retrorun_audio_buffer = 735
device.RG353M.profile.T8116D50.best_validated.retrorun_audio_stable_buffer = false
device.RG353M.profile.T8116D50.best_validated.retrorun_go2_audio_prebuffer_ms = 30
device.RG353M.profile.T8116D50.best_validated.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T8116D50.best_validated.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T8116D50.best_validated.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T8116D50.best_validated.reicast_hle_bios = disabled
device.RG353M.profile.T8116D50.best_validated.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.T8116D50.best_validated.reicast_framerate = normal
device.RG353M.profile.T8116D50.best_validated.reicast_loop_declared_fps = false
device.RG353M.profile.T8116D50.best_validated.reicast_frame_skipping = disabled
device.RG353M.profile.T8116D50.best_validated.reicast_anisotropic_filtering = off
device.RG353M.profile.T8116D50.best_validated.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T8116D50.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.T8116D50.best_validated.reicast_audio_mixer = accurate
device.RG353M.profile.T8116D50.best_validated.reicast_aica_arm_cycles = 32

device.RG353M.profile.T3602M.best_validated.title = Dead or Alive 2 (Japan, RG353M validated)
device.RG353M.profile.T3602M.best_validated.retrorun_loop_declared_fps = false
device.RG353M.profile.T3602M.best_validated.retrorun_audio_buffer = 735
device.RG353M.profile.T3602M.best_validated.retrorun_audio_stable_buffer = false
device.RG353M.profile.T3602M.best_validated.retrorun_go2_audio_prebuffer_ms = 30
device.RG353M.profile.T3602M.best_validated.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T3602M.best_validated.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T3602M.best_validated.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T3602M.best_validated.reicast_hle_bios = disabled
device.RG353M.profile.T3602M.best_validated.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.T3602M.best_validated.reicast_framerate = normal
device.RG353M.profile.T3602M.best_validated.reicast_loop_declared_fps = false
device.RG353M.profile.T3602M.best_validated.reicast_frame_skipping = disabled
device.RG353M.profile.T3602M.best_validated.reicast_anisotropic_filtering = off
device.RG353M.profile.T3602M.best_validated.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T3602M.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.T3602M.best_validated.reicast_audio_mixer = accurate
device.RG353M.profile.T3602M.best_validated.reicast_aica_arm_cycles = 32

device.RG353M.profile.T3601M.best_validated.title = Dead or Alive 2 (Japan limited, RG353M validated)
device.RG353M.profile.T3601M.best_validated.retrorun_loop_declared_fps = false
device.RG353M.profile.T3601M.best_validated.retrorun_audio_buffer = 735
device.RG353M.profile.T3601M.best_validated.retrorun_audio_stable_buffer = false
device.RG353M.profile.T3601M.best_validated.retrorun_go2_audio_prebuffer_ms = 30
device.RG353M.profile.T3601M.best_validated.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T3601M.best_validated.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T3601M.best_validated.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T3601M.best_validated.reicast_hle_bios = disabled
device.RG353M.profile.T3601M.best_validated.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.T3601M.best_validated.reicast_framerate = normal
device.RG353M.profile.T3601M.best_validated.reicast_loop_declared_fps = false
device.RG353M.profile.T3601M.best_validated.reicast_frame_skipping = disabled
device.RG353M.profile.T3601M.best_validated.reicast_anisotropic_filtering = off
device.RG353M.profile.T3601M.best_validated.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T3601M.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.T3601M.best_validated.reicast_audio_mixer = accurate
device.RG353M.profile.T3601M.best_validated.reicast_aica_arm_cycles = 32

device.RG353M.profile.T3601N.best_validated.title = Dead or Alive 2 (North America, RG353M validated)
device.RG353M.profile.T3601N.best_validated.retrorun_loop_declared_fps = false
device.RG353M.profile.T3601N.best_validated.retrorun_audio_buffer = 735
device.RG353M.profile.T3601N.best_validated.retrorun_audio_stable_buffer = false
device.RG353M.profile.T3601N.best_validated.retrorun_go2_audio_prebuffer_ms = 30
device.RG353M.profile.T3601N.best_validated.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T3601N.best_validated.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T3601N.best_validated.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T3601N.best_validated.reicast_hle_bios = disabled
device.RG353M.profile.T3601N.best_validated.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.T3601N.best_validated.reicast_framerate = normal
device.RG353M.profile.T3601N.best_validated.reicast_loop_declared_fps = false
device.RG353M.profile.T3601N.best_validated.reicast_frame_skipping = disabled
device.RG353M.profile.T3601N.best_validated.reicast_anisotropic_filtering = off
device.RG353M.profile.T3601N.best_validated.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T3601N.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.T3601N.best_validated.reicast_audio_mixer = accurate
device.RG353M.profile.T3601N.best_validated.reicast_aica_arm_cycles = 32

profile.T1401D50.best_validated.title = Soul Calibur
profile.T1401D50.best_validated.reicast_hle_bios = disabled
profile.T1401D50.best_validated.reicast_gdrom_fast_loading = enabled
profile.T1401D50.best_validated.reicast_translucent_menu_guard_draw_sorting = per_triangle
profile.T1401D50.best_validated.reicast_palette_fog_storage_reuse = enabled
profile.T1401D50.best_validated.reicast_fast_depth = vertex_fast_log
profile.T1401D50.best_validated.reicast_audio_mixer = lowend
profile.T1401D50.best_validated.reicast_framerate = normal
profile.T1401D50.best_validated.retrorun_loop_declared_fps = false

profile.T1401D50.best_performance.title = Soul Calibur (aggressive)
profile.T1401D50.best_performance.inherits = best_validated
profile.T1401D50.best_performance.reicast_mipmapping = disabled
profile.T1401D50.best_performance.reicast_fog = disabled
profile.T1401D50.best_performance.reicast_translucent_menu_guard_strategy = top_hud_last
profile.T1401D50.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
profile.T1401D50.best_performance.reicast_opaque_strip_merge = enabled
profile.T1401D50.best_performance.retrorun_egl_stencil_bits = 0

profile.T1401N.best_validated.title = Soul Calibur (North America)
profile.T1401N.best_validated.reicast_hle_bios = disabled
profile.T1401N.best_validated.reicast_gdrom_fast_loading = enabled
profile.T1401N.best_validated.reicast_translucent_menu_guard_draw_sorting = per_triangle
profile.T1401N.best_validated.reicast_palette_fog_storage_reuse = enabled
profile.T1401N.best_validated.reicast_fast_depth = vertex_fast_log
profile.T1401N.best_validated.reicast_audio_mixer = lowend
profile.T1401N.best_validated.reicast_framerate = normal
profile.T1401N.best_validated.retrorun_loop_declared_fps = false

profile.T1401N.best_performance.title = Soul Calibur (North America, aggressive)
profile.T1401N.best_performance.inherits = best_validated
profile.T1401N.best_performance.reicast_mipmapping = disabled
profile.T1401N.best_performance.reicast_fog = disabled
profile.T1401N.best_performance.reicast_translucent_menu_guard_strategy = top_hud_last
profile.T1401N.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
profile.T1401N.best_performance.reicast_opaque_strip_merge = enabled
profile.T1401N.best_performance.retrorun_egl_stencil_bits = 0

profile.T1401M.best_validated.title = Soul Calibur (Japan)
profile.T1401M.best_validated.reicast_hle_bios = disabled
profile.T1401M.best_validated.reicast_gdrom_fast_loading = enabled
profile.T1401M.best_validated.reicast_translucent_menu_guard_draw_sorting = per_triangle
profile.T1401M.best_validated.reicast_palette_fog_storage_reuse = enabled
profile.T1401M.best_validated.reicast_fast_depth = vertex_fast_log
profile.T1401M.best_validated.reicast_audio_mixer = lowend
profile.T1401M.best_validated.reicast_framerate = normal
profile.T1401M.best_validated.retrorun_loop_declared_fps = false

profile.T1401M.best_performance.title = Soul Calibur (Japan, aggressive)
profile.T1401M.best_performance.inherits = best_validated
profile.T1401M.best_performance.reicast_mipmapping = disabled
profile.T1401M.best_performance.reicast_fog = disabled
profile.T1401M.best_performance.reicast_translucent_menu_guard_strategy = top_hud_last
profile.T1401M.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
profile.T1401M.best_performance.reicast_opaque_strip_merge = enabled
profile.T1401M.best_performance.retrorun_egl_stencil_bits = 0

device.RG353M.profile.T1401D50.best_performance.title = Soul Calibur (Europe, RG353M validated)
device.RG353M.profile.T1401D50.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T1401D50.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1401D50.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1401D50.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T1401D50.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1401D50.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1401D50.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1401D50.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T1401D50.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1401D50.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T1401D50.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1401D50.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T1401D50.best_performance.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.T1401D50.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
device.RG353M.profile.T1401D50.best_performance.reicast_palette_fog_storage_reuse = enabled
device.RG353M.profile.T1401D50.best_performance.reicast_fast_depth = vertex_fast_log
device.RG353M.profile.T1401D50.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.T1401D50.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T1401D50.best_performance.reicast_aica_arm_cycles = 32

device.RG353M.profile.T1401N.best_performance.title = Soul Calibur (North America, RG353M validated)
device.RG353M.profile.T1401N.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T1401N.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1401N.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1401N.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T1401N.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1401N.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1401N.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1401N.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T1401N.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1401N.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T1401N.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1401N.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T1401N.best_performance.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.T1401N.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
device.RG353M.profile.T1401N.best_performance.reicast_palette_fog_storage_reuse = enabled
device.RG353M.profile.T1401N.best_performance.reicast_fast_depth = vertex_fast_log
device.RG353M.profile.T1401N.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.T1401N.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T1401N.best_performance.reicast_aica_arm_cycles = 32

device.RG353M.profile.T1401M.best_performance.title = Soul Calibur (Japan, RG353M validated)
device.RG353M.profile.T1401M.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T1401M.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1401M.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1401M.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T1401M.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1401M.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1401M.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1401M.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T1401M.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1401M.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T1401M.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1401M.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T1401M.best_performance.reicast_translucent_menu_guard_strategy = top_hud_last
device.RG353M.profile.T1401M.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
device.RG353M.profile.T1401M.best_performance.reicast_palette_fog_storage_reuse = enabled
device.RG353M.profile.T1401M.best_performance.reicast_fast_depth = vertex_fast_log
device.RG353M.profile.T1401M.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.T1401M.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T1401M.best_performance.reicast_aica_arm_cycles = 32

profile.T38706M.best_validated.title = Ikaruga
profile.T38706M.best_validated.retrorun_adaptive_frameskip = false
profile.T38706M.best_validated.retrorun_audio_buffer = 2048
profile.T38706M.best_validated.retrorun_audio_stable_buffer = true
profile.T38706M.best_validated.retrorun_frameskip = 0
profile.T38706M.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.T38706M.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.T38706M.best_validated.retrorun_loop_declared_fps = true
profile.T38706M.best_validated.reicast_hle_bios = enabled
profile.T38706M.best_validated.reicast_gdrom_fast_loading = disabled
profile.T38706M.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T38706M.best_validated.reicast_mipmapping = disabled
profile.T38706M.best_validated.reicast_fog = disabled
profile.T38706M.best_validated.reicast_frame_skipping = adaptive
profile.T38706M.best_validated.reicast_translucent_strip_merge = menu_guarded
profile.T38706M.best_validated.reicast_translucent_menu_guard_strategy = scored
profile.T38706M.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.T38706M.best_validated.reicast_translucent_menu_guard_risk = 5
profile.T38706M.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.T38706M.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.T38706M.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.T38706M.best_validated.reicast_texture_storage_reuse = enabled
profile.T38706M.best_validated.reicast_palette_fog_storage_reuse = disabled
profile.T38706M.best_validated.reicast_adjacent_state_elision = disabled
profile.T38706M.best_validated.reicast_fast_depth = vertex_fast_log
profile.T38706M.best_validated.reicast_audio_mixer = fast
profile.T38706M.best_validated.reicast_opaque_strip_merge = enabled
profile.T38706M.best_validated.retrorun_egl_depth_bits = 24
profile.T38706M.best_validated.retrorun_egl_stencil_bits = 0
profile.T38706M.best_validated.reicast_aica_arm_cycles = 16

device.RG353M.profile.T38706M.best_performance.title = Ikaruga (Japan, RG353M validated)
device.RG353M.profile.T38706M.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T38706M.best_performance.reicast_framerate = normal
device.RG353M.profile.T38706M.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T38706M.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T38706M.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T38706M.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T38706M.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T38706M.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T38706M.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T38706M.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T38706M.best_performance.reicast_frame_skipping = disabled

profile.T1212N.best_validated.title = Marvel vs. Capcom 2
profile.T1212N.best_validated.retrorun_adaptive_frameskip = false
profile.T1212N.best_validated.retrorun_audio_buffer = 2048
profile.T1212N.best_validated.retrorun_audio_stable_buffer = true
profile.T1212N.best_validated.retrorun_frameskip = 0
profile.T1212N.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.T1212N.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.T1212N.best_validated.retrorun_loop_declared_fps = true
profile.T1212N.best_validated.reicast_hle_bios = enabled
profile.T1212N.best_validated.reicast_gdrom_fast_loading = disabled
profile.T1212N.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T1212N.best_validated.reicast_mipmapping = disabled
profile.T1212N.best_validated.reicast_fog = disabled
profile.T1212N.best_validated.reicast_frame_skipping = adaptive
profile.T1212N.best_validated.reicast_threaded_rendering = enabled
profile.T1212N.best_validated.reicast_translucent_strip_merge = menu_guarded
profile.T1212N.best_validated.reicast_translucent_menu_guard_strategy = scored
profile.T1212N.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.T1212N.best_validated.reicast_translucent_menu_guard_risk = 5
profile.T1212N.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.T1212N.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.T1212N.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.T1212N.best_validated.reicast_texture_storage_reuse = enabled
profile.T1212N.best_validated.reicast_palette_fog_storage_reuse = disabled
profile.T1212N.best_validated.reicast_adjacent_state_elision = disabled
profile.T1212N.best_validated.reicast_fast_depth = vertex_fast_log
profile.T1212N.best_validated.reicast_audio_mixer = lowend
profile.T1212N.best_validated.reicast_opaque_strip_merge = enabled
profile.T1212N.best_validated.retrorun_egl_depth_bits = 24
profile.T1212N.best_validated.retrorun_egl_stencil_bits = 0
profile.T1212N.best_validated.reicast_aica_arm_cycles = 8

profile.T7010D50.best_validated.title = Marvel vs. Capcom 2 (Europe)
profile.T7010D50.best_validated.retrorun_adaptive_frameskip = false
profile.T7010D50.best_validated.retrorun_audio_buffer = 2048
profile.T7010D50.best_validated.retrorun_audio_stable_buffer = true
profile.T7010D50.best_validated.retrorun_frameskip = 0
profile.T7010D50.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.T7010D50.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.T7010D50.best_validated.retrorun_loop_declared_fps = true
profile.T7010D50.best_validated.reicast_hle_bios = enabled
profile.T7010D50.best_validated.reicast_gdrom_fast_loading = disabled
profile.T7010D50.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T7010D50.best_validated.reicast_mipmapping = disabled
profile.T7010D50.best_validated.reicast_fog = disabled
profile.T7010D50.best_validated.reicast_frame_skipping = adaptive
profile.T7010D50.best_validated.reicast_threaded_rendering = enabled
profile.T7010D50.best_validated.reicast_translucent_strip_merge = menu_guarded
profile.T7010D50.best_validated.reicast_translucent_menu_guard_strategy = scored
profile.T7010D50.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.T7010D50.best_validated.reicast_translucent_menu_guard_risk = 5
profile.T7010D50.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.T7010D50.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.T7010D50.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.T7010D50.best_validated.reicast_texture_storage_reuse = enabled
profile.T7010D50.best_validated.reicast_palette_fog_storage_reuse = disabled
profile.T7010D50.best_validated.reicast_adjacent_state_elision = disabled
profile.T7010D50.best_validated.reicast_fast_depth = vertex_fast_log
profile.T7010D50.best_validated.reicast_audio_mixer = lowend
profile.T7010D50.best_validated.reicast_opaque_strip_merge = enabled
profile.T7010D50.best_validated.retrorun_egl_depth_bits = 24
profile.T7010D50.best_validated.retrorun_egl_stencil_bits = 0
profile.T7010D50.best_validated.reicast_aica_arm_cycles = 8

profile.T1215M.best_validated.title = Marvel vs. Capcom 2 (Japan)
profile.T1215M.best_validated.retrorun_adaptive_frameskip = false
profile.T1215M.best_validated.retrorun_audio_buffer = 2048
profile.T1215M.best_validated.retrorun_audio_stable_buffer = true
profile.T1215M.best_validated.retrorun_frameskip = 0
profile.T1215M.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.T1215M.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.T1215M.best_validated.retrorun_loop_declared_fps = true
profile.T1215M.best_validated.reicast_hle_bios = enabled
profile.T1215M.best_validated.reicast_gdrom_fast_loading = disabled
profile.T1215M.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T1215M.best_validated.reicast_mipmapping = disabled
profile.T1215M.best_validated.reicast_fog = disabled
profile.T1215M.best_validated.reicast_frame_skipping = adaptive
profile.T1215M.best_validated.reicast_threaded_rendering = enabled
profile.T1215M.best_validated.reicast_translucent_strip_merge = menu_guarded
profile.T1215M.best_validated.reicast_translucent_menu_guard_strategy = scored
profile.T1215M.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.T1215M.best_validated.reicast_translucent_menu_guard_risk = 5
profile.T1215M.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.T1215M.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.T1215M.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.T1215M.best_validated.reicast_texture_storage_reuse = enabled
profile.T1215M.best_validated.reicast_palette_fog_storage_reuse = disabled
profile.T1215M.best_validated.reicast_adjacent_state_elision = disabled
profile.T1215M.best_validated.reicast_fast_depth = vertex_fast_log
profile.T1215M.best_validated.reicast_audio_mixer = lowend
profile.T1215M.best_validated.reicast_opaque_strip_merge = enabled
profile.T1215M.best_validated.retrorun_egl_depth_bits = 24
profile.T1215M.best_validated.retrorun_egl_stencil_bits = 0
profile.T1215M.best_validated.reicast_aica_arm_cycles = 8

device.RG353M.profile.T1212N.best_performance.title = Marvel vs. Capcom 2 (USA, RG353M validated)
device.RG353M.profile.T1212N.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T1212N.best_performance.reicast_framerate = normal
device.RG353M.profile.T1212N.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T1212N.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1212N.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1212N.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.T1212N.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T1212N.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1212N.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1212N.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1212N.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1212N.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T1212N.best_performance.reicast_aica_arm_cycles = 16

device.RG353M.profile.T7010D50.best_performance.title = Marvel vs. Capcom 2 (Europe, RG353M validated)
device.RG353M.profile.T7010D50.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T7010D50.best_performance.reicast_framerate = normal
device.RG353M.profile.T7010D50.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T7010D50.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T7010D50.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T7010D50.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.T7010D50.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T7010D50.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T7010D50.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T7010D50.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T7010D50.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T7010D50.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T7010D50.best_performance.reicast_aica_arm_cycles = 16

device.RG353M.profile.T1215M.best_performance.title = Marvel vs. Capcom 2 (Japan, RG353M validated)
device.RG353M.profile.T1215M.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T1215M.best_performance.reicast_framerate = normal
device.RG353M.profile.T1215M.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T1215M.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1215M.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1215M.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.T1215M.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T1215M.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1215M.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1215M.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1215M.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1215M.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T1215M.best_performance.reicast_aica_arm_cycles = 16

profile.MK-51054.best_validated.title = Virtua Tennis
profile.MK-51054.best_validated.retrorun_adaptive_frameskip = false
profile.MK-51054.best_validated.retrorun_audio_buffer = 2048
profile.MK-51054.best_validated.retrorun_audio_stable_buffer = true
profile.MK-51054.best_validated.retrorun_frameskip = 0
profile.MK-51054.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.MK-51054.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.MK-51054.best_validated.retrorun_loop_declared_fps = true
profile.MK-51054.best_validated.reicast_hle_bios = enabled
profile.MK-51054.best_validated.reicast_gdrom_fast_loading = disabled
profile.MK-51054.best_validated.reicast_alpha_sorting = per-strip (fast, least accurate)
profile.MK-51054.best_validated.reicast_mipmapping = disabled
profile.MK-51054.best_validated.reicast_fog = disabled
profile.MK-51054.best_validated.reicast_frame_skipping = adaptive
profile.MK-51054.best_validated.reicast_translucent_strip_merge = menu_guarded
profile.MK-51054.best_validated.reicast_translucent_menu_guard_strategy = hud_last
profile.MK-51054.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.MK-51054.best_validated.reicast_translucent_menu_guard_risk = 5
profile.MK-51054.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.MK-51054.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.MK-51054.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.MK-51054.best_validated.reicast_texture_storage_reuse = enabled
profile.MK-51054.best_validated.reicast_palette_fog_storage_reuse = disabled
profile.MK-51054.best_validated.reicast_adjacent_state_elision = disabled
profile.MK-51054.best_validated.reicast_fast_depth = enabled
profile.MK-51054.best_validated.reicast_audio_mixer = fast
profile.MK-51054.best_validated.reicast_opaque_strip_merge = enabled
profile.MK-51054.best_validated.retrorun_egl_depth_bits = 24
profile.MK-51054.best_validated.retrorun_egl_stencil_bits = 0
profile.MK-51054.best_validated.reicast_aica_arm_cycles = 16

profile.HDR-0113.best_validated.title = Power Smash
profile.HDR-0113.best_validated.retrorun_adaptive_frameskip = false
profile.HDR-0113.best_validated.retrorun_audio_buffer = 2048
profile.HDR-0113.best_validated.retrorun_audio_stable_buffer = true
profile.HDR-0113.best_validated.retrorun_frameskip = 0
profile.HDR-0113.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.HDR-0113.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.HDR-0113.best_validated.retrorun_loop_declared_fps = true
profile.HDR-0113.best_validated.reicast_hle_bios = enabled
profile.HDR-0113.best_validated.reicast_gdrom_fast_loading = disabled
profile.HDR-0113.best_validated.reicast_alpha_sorting = per-strip (fast, least accurate)
profile.HDR-0113.best_validated.reicast_mipmapping = disabled
profile.HDR-0113.best_validated.reicast_fog = disabled
profile.HDR-0113.best_validated.reicast_frame_skipping = adaptive
profile.HDR-0113.best_validated.reicast_translucent_strip_merge = menu_guarded
profile.HDR-0113.best_validated.reicast_translucent_menu_guard_strategy = hud_last
profile.HDR-0113.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.HDR-0113.best_validated.reicast_translucent_menu_guard_risk = 5
profile.HDR-0113.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.HDR-0113.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.HDR-0113.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.HDR-0113.best_validated.reicast_texture_storage_reuse = enabled
profile.HDR-0113.best_validated.reicast_palette_fog_storage_reuse = disabled
profile.HDR-0113.best_validated.reicast_adjacent_state_elision = disabled
profile.HDR-0113.best_validated.reicast_fast_depth = enabled
profile.HDR-0113.best_validated.reicast_audio_mixer = fast
profile.HDR-0113.best_validated.reicast_opaque_strip_merge = enabled
profile.HDR-0113.best_validated.retrorun_egl_depth_bits = 24
profile.HDR-0113.best_validated.retrorun_egl_stencil_bits = 0
profile.HDR-0113.best_validated.reicast_aica_arm_cycles = 16

device.RG353M.profile.MK-51054.best_performance.title = Virtua Tennis (RG353M validated)
device.RG353M.profile.MK-51054.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-51054.best_performance.reicast_framerate = normal
device.RG353M.profile.MK-51054.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-51054.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-51054.best_performance.retrorun_audio_buffer = 1470
device.RG353M.profile.MK-51054.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.MK-51054.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.MK-51054.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-51054.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51054.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.MK-51054.best_performance.reicast_frame_skipping = disabled

device.RG353M.profile.HDR-0113.best_performance.title = Power Smash (Japan, RG353M validated)
device.RG353M.profile.HDR-0113.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.HDR-0113.best_performance.reicast_framerate = normal
device.RG353M.profile.HDR-0113.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.HDR-0113.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.HDR-0113.best_performance.retrorun_audio_buffer = 1470
device.RG353M.profile.HDR-0113.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.HDR-0113.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.HDR-0113.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.HDR-0113.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.HDR-0113.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.HDR-0113.best_performance.reicast_frame_skipping = disabled

profile.MK-51000.best_validated.title = Sonic Adventure (baseline)

profile.MK-51000.best_performance.title = Sonic Adventure (RG351V)
profile.MK-51000.best_performance.inherits = best_validated
profile.MK-51000.best_performance.retrorun_adaptive_frameskip = false
profile.MK-51000.best_performance.retrorun_audio_buffer = 2048
profile.MK-51000.best_performance.retrorun_frameskip = 0
profile.MK-51000.best_performance.retrorun_go2_audio_wsola_profile = disabled
profile.MK-51000.best_performance.retrorun_loop_declared_fps = true
profile.MK-51000.best_performance.reicast_hle_bios = enabled
profile.MK-51000.best_performance.reicast_gdrom_fast_loading = disabled
profile.MK-51000.best_performance.reicast_mipmapping = disabled
profile.MK-51000.best_performance.reicast_fog = disabled
profile.MK-51000.best_performance.reicast_fast_depth = vertex_fast_log
profile.MK-51000.best_performance.reicast_opaque_strip_merge = enabled
profile.MK-51000.best_performance.retrorun_egl_stencil_bits = 0
profile.MK-51000.best_performance.reicast_aica_arm_cycles = 24
profile.MK-51000.best_performance.reicast_framerate = fullspeed
profile.MK-51000.best_performance.reicast_loop_declared_fps = false
profile.MK-51000.best_performance.reicast_audio_mixer = lowend

device.RG353M.profile.MK-51000.best_performance.title = Sonic Adventure (Europe / North America, RG353M validated)
device.RG353M.profile.MK-51000.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-51000.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.MK-51000.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.MK-51000.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.MK-51000.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-51000.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51000.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.MK-51000.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-51000.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-51000.best_performance.reicast_frame_skipping = disabled

profile.HDR-0001.best_validated.title = Sonic Adventure (Japan, baseline)

profile.HDR-0043.best_validated.title = Sonic Adventure International (Japan, baseline)

profile.MK-51059.best_validated.title = Shenmue (baseline)
profile.MK-51059.best_performance.title = Shenmue (menu-guarded fast depth)
profile.MK-51059.best_performance.inherits = best_validated
profile.MK-51059.best_performance.retrorun_audio_buffer = 2048
profile.MK-51059.best_performance.retrorun_go2_audio_wsola_profile = disabled
profile.MK-51059.best_performance.reicast_aica_arm_cycles = 24
profile.MK-51059.best_performance.reicast_fast_depth = menu_guarded

profile.MK-51131.best_validated.title = Shenmue (USA revision, baseline)
profile.MK-51131.best_performance.title = Shenmue (USA revision, menu-guarded fast depth)
profile.MK-51131.best_performance.inherits = best_validated
profile.MK-51131.best_performance.retrorun_audio_buffer = 2048
profile.MK-51131.best_performance.retrorun_go2_audio_wsola_profile = disabled
profile.MK-51131.best_performance.reicast_aica_arm_cycles = 24
profile.MK-51131.best_performance.reicast_fast_depth = menu_guarded

profile.HDR-0016.best_validated.title = Shenmue (Japan, baseline)
profile.HDR-0016.best_performance.title = Shenmue (Japan, menu-guarded fast depth)
profile.HDR-0016.best_performance.inherits = best_validated
profile.HDR-0016.best_performance.retrorun_audio_buffer = 2048
profile.HDR-0016.best_performance.retrorun_go2_audio_wsola_profile = disabled
profile.HDR-0016.best_performance.reicast_aica_arm_cycles = 24
profile.HDR-0016.best_performance.reicast_fast_depth = menu_guarded

profile.HDR-0031.best_validated.title = Shenmue (Japan revision, baseline)
profile.HDR-0031.best_performance.title = Shenmue (Japan revision, menu-guarded fast depth)
profile.HDR-0031.best_performance.inherits = best_validated
profile.HDR-0031.best_performance.retrorun_audio_buffer = 2048
profile.HDR-0031.best_performance.retrorun_go2_audio_wsola_profile = disabled
profile.HDR-0031.best_performance.reicast_aica_arm_cycles = 24
profile.HDR-0031.best_performance.reicast_fast_depth = menu_guarded

device.RG353M.profile.MK-51059.best_performance.title = Shenmue (USA, RG353M validated)
device.RG353M.profile.MK-51059.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-51059.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-51059.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-51059.best_performance.retrorun_audio_buffer = -1
device.RG353M.profile.MK-51059.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-51059.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51059.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.MK-51059.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.MK-51059.best_performance.reicast_framerate = normal
device.RG353M.profile.MK-51059.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.MK-51059.best_performance.reicast_fast_depth = vertex_fast_log
device.RG353M.profile.MK-51059.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.MK-51059.best_performance.reicast_aica_arm_cycles = 32

device.RG353M.profile.MK-51131.best_performance.title = Shenmue (USA revision, RG353M validated)
device.RG353M.profile.MK-51131.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-51131.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-51131.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-51131.best_performance.retrorun_audio_buffer = -1
device.RG353M.profile.MK-51131.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-51131.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51131.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.MK-51131.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.MK-51131.best_performance.reicast_framerate = normal
device.RG353M.profile.MK-51131.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.MK-51131.best_performance.reicast_fast_depth = vertex_fast_log
device.RG353M.profile.MK-51131.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.MK-51131.best_performance.reicast_aica_arm_cycles = 32

device.RG353M.profile.HDR-0016.best_performance.title = Shenmue (Japan, RG353M validated)
device.RG353M.profile.HDR-0016.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.HDR-0016.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.HDR-0016.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.HDR-0016.best_performance.retrorun_audio_buffer = -1
device.RG353M.profile.HDR-0016.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.HDR-0016.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.HDR-0016.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.HDR-0016.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.HDR-0016.best_performance.reicast_framerate = normal
device.RG353M.profile.HDR-0016.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.HDR-0016.best_performance.reicast_fast_depth = vertex_fast_log
device.RG353M.profile.HDR-0016.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.HDR-0016.best_performance.reicast_aica_arm_cycles = 32

device.RG353M.profile.HDR-0031.best_performance.title = Shenmue (Japan revision, RG353M validated)
device.RG353M.profile.HDR-0031.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.HDR-0031.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.HDR-0031.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.HDR-0031.best_performance.retrorun_audio_buffer = -1
device.RG353M.profile.HDR-0031.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.HDR-0031.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.HDR-0031.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.HDR-0031.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.HDR-0031.best_performance.reicast_framerate = normal
device.RG353M.profile.HDR-0031.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.HDR-0031.best_performance.reicast_fast_depth = vertex_fast_log
device.RG353M.profile.HDR-0031.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.HDR-0031.best_performance.reicast_aica_arm_cycles = 32

profile.MK-5118450.best_validated.title = Shenmue II (Europe, RG351MP validated)
profile.MK-5118450.best_validated.retrorun_audio_buffer = 4096
profile.MK-5118450.best_validated.retrorun_audio_stable_buffer = true
profile.MK-5118450.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.MK-5118450.best_validated.retrorun_go2_audio_wsola_profile = lowend_heavy_100
profile.MK-5118450.best_validated.retrorun_egl_depth_bits = 24
profile.MK-5118450.best_validated.retrorun_egl_stencil_bits = 0
profile.MK-5118450.best_validated.reicast_hle_bios = enabled
profile.MK-5118450.best_validated.reicast_framerate = fullspeed
profile.MK-5118450.best_validated.reicast_alpha_sorting = per-strip (fast, least accurate)
profile.MK-5118450.best_validated.reicast_mipmapping = enabled
profile.MK-5118450.best_validated.reicast_fog = enabled
profile.MK-5118450.best_validated.reicast_frame_skipping = disabled
profile.MK-5118450.best_validated.reicast_translucent_strip_merge = disabled
profile.MK-5118450.best_validated.reicast_translucent_menu_guard_strategy = hud_last
profile.MK-5118450.best_validated.reicast_texture_storage_reuse = disabled
profile.MK-5118450.best_validated.reicast_adjacent_state_elision = disabled
profile.MK-5118450.best_validated.reicast_fast_depth = vertex_fast_log
profile.MK-5118450.best_validated.reicast_audio_mixer = accurate
profile.MK-5118450.best_validated.reicast_opaque_strip_merge = disabled
profile.MK-5118450.best_validated.reicast_aica_arm_cycles = 32

device.RG351MP.profile.MK-5118450.best_performance.title = Shenmue II (Europe, RG351MP batch validated)
device.RG351MP.profile.MK-5118450.best_performance.reicast_accurate_aica_batch = enabled

device.RG353M.profile.MK-5118450.best_performance.title = Shenmue II (Europe, RG353M validated)
device.RG353M.profile.MK-5118450.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-5118450.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.MK-5118450.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.MK-5118450.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.MK-5118450.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-5118450.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-5118450.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.MK-5118450.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.MK-5118450.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.MK-5118450.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.MK-5118450.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.MK-5118450.best_performance.reicast_sh4clock = 200
device.RG353M.profile.MK-5118450.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-5118450.best_performance.reicast_framerate = normal
device.RG353M.profile.MK-5118450.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.MK-5118450.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.MK-5118450.best_performance.reicast_fog = enabled
device.RG353M.profile.MK-5118450.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.MK-5118450.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.MK-5118450.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-5118450.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.MK-5118450.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.MK-5118450.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.MK-5118450.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.MK-5118450.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.MK-5118450.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
device.RG353M.profile.MK-5118450.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.MK-5118450.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.MK-5118450.best_performance.reicast_aica_arm_cycles = 32

profile.HDR-0164.best_validated.title = Shenmue II (Japan, baseline)

profile.HDR-0179.best_validated.title = Shenmue II (Japan revision, baseline)

profile.T36806D05.best_validated.title = Resident Evil: Code Veronica (Europe, RG351V)
profile.T36806D05.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T36806D05.best_performance.title = Resident Evil: Code Veronica (Europe, RG351V)
profile.T36806D05.best_performance.inherits = best_validated

profile.T36806D09.best_validated.title = Resident Evil: Code Veronica (France, RG351V)
profile.T36806D09.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T36806D09.best_performance.title = Resident Evil: Code Veronica (France, RG351V)
profile.T36806D09.best_performance.inherits = best_validated

profile.T36806D18.best_validated.title = Resident Evil: Code Veronica (Germany, RG351V)
profile.T36806D18.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T36806D18.best_performance.title = Resident Evil: Code Veronica (Germany, RG351V)
profile.T36806D18.best_performance.inherits = best_validated

profile.T36806D06.best_validated.title = Resident Evil: Code Veronica (Spain, RG351V)
profile.T36806D06.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T36806D06.best_performance.title = Resident Evil: Code Veronica (Spain, RG351V)
profile.T36806D06.best_performance.inherits = best_validated

profile.T1204N.best_validated.title = Resident Evil: Code Veronica (USA, RG351V)
profile.T1204N.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T1204N.best_performance.title = Resident Evil: Code Veronica (USA, RG351V)
profile.T1204N.best_performance.inherits = best_validated

profile.T1207M.best_validated.title = Biohazard Code Veronica (Japan, RG351V)
profile.T1207M.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T1207M.best_performance.title = Biohazard Code Veronica (Japan, RG351V)
profile.T1207M.best_performance.inherits = best_validated

profile.T1210M.best_validated.title = Biohazard Code Veronica Limited Edition (Japan, RG351V)
profile.T1210M.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T1210M.best_performance.title = Biohazard Code Veronica Limited Edition (Japan, RG351V)
profile.T1210M.best_performance.inherits = best_validated

profile.T1240M.best_validated.title = Biohazard Code Veronica Complete (Japan, RG351V)
profile.T1240M.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T1240M.best_performance.title = Biohazard Code Veronica Complete (Japan, RG351V)
profile.T1240M.best_performance.inherits = best_validated

profile.MK-51058.best_validated.title = Jet Set Radio / Jet Grind Radio (RG351MP validated)
profile.MK-51058.best_validated.retrorun_audio_buffer = 2048
profile.MK-51058.best_validated.retrorun_audio_stable_buffer = true
profile.MK-51058.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.MK-51058.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.MK-51058.best_validated.retrorun_egl_depth_bits = 24
profile.MK-51058.best_validated.retrorun_egl_stencil_bits = 0
profile.MK-51058.best_validated.reicast_hle_bios = enabled
profile.MK-51058.best_validated.reicast_framerate = fullspeed
profile.MK-51058.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.MK-51058.best_validated.reicast_translucent_strip_merge = disabled
profile.MK-51058.best_validated.reicast_fast_depth = enabled
profile.MK-51058.best_validated.reicast_audio_mixer = accurate
profile.MK-51058.best_validated.reicast_opaque_strip_merge = enabled
profile.MK-51058.best_validated.reicast_aica_arm_cycles = 32

device.RG353M.profile.MK-51058.best_performance.title = Jet Grind Radio (USA, RG353M validated)
device.RG353M.profile.MK-51058.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.MK-51058.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-51058.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.MK-51058.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.MK-51058.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.MK-51058.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-51058.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51058.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.MK-51058.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.MK-51058.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.MK-51058.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.MK-51058.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.MK-51058.best_performance.reicast_sh4clock = 200
device.RG353M.profile.MK-51058.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-51058.best_performance.reicast_framerate = normal
device.RG353M.profile.MK-51058.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.MK-51058.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.MK-51058.best_performance.reicast_fog = enabled
device.RG353M.profile.MK-51058.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.MK-51058.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.MK-51058.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-51058.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.MK-51058.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.MK-51058.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.MK-51058.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.MK-51058.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.MK-51058.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
device.RG353M.profile.MK-51058.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.MK-51058.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.MK-51058.best_performance.reicast_aica_arm_cycles = 32

profile.MK-51084.best_validated.title = Jet Grind Radio (USA revision, baseline)

profile.HDR-0078.best_validated.title = Jet Set Radio (Japan, baseline)

profile.HDR-0128.best_validated.title = De La Jet Set Radio (Japan, baseline)

profile.HDR-0186.best_validated.title = De La Jet Set Radio (Japan revision, baseline)

profile.RDC-0034.best_validated.title = Power Stone (US RDC)
profile.RDC-0034.best_validated.retrorun_adaptive_frameskip = false
profile.RDC-0034.best_validated.retrorun_audio_buffer = 2048
profile.RDC-0034.best_validated.retrorun_audio_stable_buffer = true
profile.RDC-0034.best_validated.retrorun_frameskip = 0
profile.RDC-0034.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.RDC-0034.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.RDC-0034.best_validated.retrorun_loop_declared_fps = false
profile.RDC-0034.best_validated.reicast_hle_bios = enabled
profile.RDC-0034.best_validated.reicast_gdrom_fast_loading = disabled
profile.RDC-0034.best_validated.reicast_alpha_sorting = per-strip (fast, least accurate)
profile.RDC-0034.best_validated.reicast_mipmapping = disabled
profile.RDC-0034.best_validated.reicast_fog = disabled
profile.RDC-0034.best_validated.reicast_frame_skipping = disabled
profile.RDC-0034.best_validated.reicast_translucent_strip_merge = menu_guarded
profile.RDC-0034.best_validated.reicast_translucent_menu_guard_strategy = hud_last
profile.RDC-0034.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.RDC-0034.best_validated.reicast_translucent_menu_guard_risk = 5
profile.RDC-0034.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.RDC-0034.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.RDC-0034.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.RDC-0034.best_validated.reicast_texture_storage_reuse = enabled
profile.RDC-0034.best_validated.reicast_palette_fog_storage_reuse = disabled
profile.RDC-0034.best_validated.reicast_adjacent_state_elision = disabled
profile.RDC-0034.best_validated.reicast_fast_depth = menu_guarded
profile.RDC-0034.best_validated.reicast_audio_mixer = fast
profile.RDC-0034.best_validated.reicast_opaque_strip_merge = enabled
profile.RDC-0034.best_validated.retrorun_egl_depth_bits = 24
profile.RDC-0034.best_validated.retrorun_egl_stencil_bits = 0
profile.RDC-0034.best_validated.reicast_aica_arm_cycles = 16

profile.RDC-0034.best_performance.title = Power Stone (US RDC, RG351V)
profile.RDC-0034.best_performance.inherits = best_validated

profile.T36801D61.best_validated.title = Power Stone (Europe 61, baseline)

profile.T36801D64.best_validated.title = Power Stone (Europe 64, baseline)

profile.T1201M.best_validated.title = Power Stone (Japan, baseline)

profile.T1201N.best_validated.title = Power Stone (USA, baseline)

device.RG353M.profile.T36801D61.best_performance.title = Power Stone (Europe 61, RG353M candidate)
device.RG353M.profile.T36801D61.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T36801D61.best_performance.retrorun_adaptive_frameskip = false
device.RG353M.profile.T36801D61.best_performance.retrorun_frameskip = 0
device.RG353M.profile.T36801D61.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T36801D61.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T36801D61.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.T36801D61.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T36801D61.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T36801D61.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T36801D61.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T36801D61.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T36801D61.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T36801D61.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.T36801D61.best_performance.reicast_gdrom_fast_loading = disabled
device.RG353M.profile.T36801D61.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T36801D61.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T36801D61.best_performance.reicast_framerate = normal
device.RG353M.profile.T36801D61.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T36801D61.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.T36801D61.best_performance.reicast_mipmapping = disabled
device.RG353M.profile.T36801D61.best_performance.reicast_fog = disabled
device.RG353M.profile.T36801D61.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.T36801D61.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T36801D61.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.T36801D61.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T36801D61.best_performance.reicast_translucent_menu_guard_strategy = hud_last
device.RG353M.profile.T36801D61.best_performance.reicast_translucent_menu_guard_max_vertices = 64
device.RG353M.profile.T36801D61.best_performance.reicast_translucent_menu_guard_risk = 5
device.RG353M.profile.T36801D61.best_performance.reicast_translucent_menu_guard_depth_tolerance = 0.0001
device.RG353M.profile.T36801D61.best_performance.reicast_translucent_menu_guard_overlap = risky
device.RG353M.profile.T36801D61.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
device.RG353M.profile.T36801D61.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T36801D61.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.T36801D61.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.T36801D61.best_performance.reicast_fast_depth = menu_guarded
device.RG353M.profile.T36801D61.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T36801D61.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T36801D61.best_performance.reicast_aica_arm_cycles = 16

device.RG353M.profile.T36801D64.best_performance.title = Power Stone (Europe 64, RG353M candidate)
device.RG353M.profile.T36801D64.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T36801D64.best_performance.retrorun_adaptive_frameskip = false
device.RG353M.profile.T36801D64.best_performance.retrorun_frameskip = 0
device.RG353M.profile.T36801D64.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T36801D64.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T36801D64.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.T36801D64.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T36801D64.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T36801D64.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T36801D64.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T36801D64.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T36801D64.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T36801D64.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.T36801D64.best_performance.reicast_gdrom_fast_loading = disabled
device.RG353M.profile.T36801D64.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T36801D64.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T36801D64.best_performance.reicast_framerate = normal
device.RG353M.profile.T36801D64.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T36801D64.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.T36801D64.best_performance.reicast_mipmapping = disabled
device.RG353M.profile.T36801D64.best_performance.reicast_fog = disabled
device.RG353M.profile.T36801D64.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.T36801D64.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T36801D64.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.T36801D64.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T36801D64.best_performance.reicast_translucent_menu_guard_strategy = hud_last
device.RG353M.profile.T36801D64.best_performance.reicast_translucent_menu_guard_max_vertices = 64
device.RG353M.profile.T36801D64.best_performance.reicast_translucent_menu_guard_risk = 5
device.RG353M.profile.T36801D64.best_performance.reicast_translucent_menu_guard_depth_tolerance = 0.0001
device.RG353M.profile.T36801D64.best_performance.reicast_translucent_menu_guard_overlap = risky
device.RG353M.profile.T36801D64.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
device.RG353M.profile.T36801D64.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T36801D64.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.T36801D64.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.T36801D64.best_performance.reicast_fast_depth = menu_guarded
device.RG353M.profile.T36801D64.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T36801D64.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T36801D64.best_performance.reicast_aica_arm_cycles = 16

device.RG353M.profile.T1201M.best_performance.title = Power Stone (Japan, RG353M candidate)
device.RG353M.profile.T1201M.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T1201M.best_performance.retrorun_adaptive_frameskip = false
device.RG353M.profile.T1201M.best_performance.retrorun_frameskip = 0
device.RG353M.profile.T1201M.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1201M.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1201M.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.T1201M.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T1201M.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1201M.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1201M.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1201M.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T1201M.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1201M.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.T1201M.best_performance.reicast_gdrom_fast_loading = disabled
device.RG353M.profile.T1201M.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T1201M.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T1201M.best_performance.reicast_framerate = normal
device.RG353M.profile.T1201M.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T1201M.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.T1201M.best_performance.reicast_mipmapping = disabled
device.RG353M.profile.T1201M.best_performance.reicast_fog = disabled
device.RG353M.profile.T1201M.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.T1201M.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1201M.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.T1201M.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T1201M.best_performance.reicast_translucent_menu_guard_strategy = hud_last
device.RG353M.profile.T1201M.best_performance.reicast_translucent_menu_guard_max_vertices = 64
device.RG353M.profile.T1201M.best_performance.reicast_translucent_menu_guard_risk = 5
device.RG353M.profile.T1201M.best_performance.reicast_translucent_menu_guard_depth_tolerance = 0.0001
device.RG353M.profile.T1201M.best_performance.reicast_translucent_menu_guard_overlap = risky
device.RG353M.profile.T1201M.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
device.RG353M.profile.T1201M.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T1201M.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.T1201M.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.T1201M.best_performance.reicast_fast_depth = menu_guarded
device.RG353M.profile.T1201M.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T1201M.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T1201M.best_performance.reicast_aica_arm_cycles = 16

device.RG353M.profile.T1201N.best_performance.title = Power Stone (USA, RG353M candidate)
device.RG353M.profile.T1201N.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T1201N.best_performance.retrorun_adaptive_frameskip = false
device.RG353M.profile.T1201N.best_performance.retrorun_frameskip = 0
device.RG353M.profile.T1201N.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1201N.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1201N.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.T1201N.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T1201N.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1201N.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1201N.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1201N.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T1201N.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1201N.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.T1201N.best_performance.reicast_gdrom_fast_loading = disabled
device.RG353M.profile.T1201N.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T1201N.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T1201N.best_performance.reicast_framerate = normal
device.RG353M.profile.T1201N.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T1201N.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.T1201N.best_performance.reicast_mipmapping = disabled
device.RG353M.profile.T1201N.best_performance.reicast_fog = disabled
device.RG353M.profile.T1201N.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.T1201N.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1201N.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.T1201N.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T1201N.best_performance.reicast_translucent_menu_guard_strategy = hud_last
device.RG353M.profile.T1201N.best_performance.reicast_translucent_menu_guard_max_vertices = 64
device.RG353M.profile.T1201N.best_performance.reicast_translucent_menu_guard_risk = 5
device.RG353M.profile.T1201N.best_performance.reicast_translucent_menu_guard_depth_tolerance = 0.0001
device.RG353M.profile.T1201N.best_performance.reicast_translucent_menu_guard_overlap = risky
device.RG353M.profile.T1201N.best_performance.reicast_translucent_menu_guard_draw_sorting = standard
device.RG353M.profile.T1201N.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T1201N.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.T1201N.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.T1201N.best_performance.reicast_fast_depth = menu_guarded
device.RG353M.profile.T1201N.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T1201N.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T1201N.best_performance.reicast_aica_arm_cycles = 16

profile.MK-51049.best_validated.title = ChuChu Rocket (USA, RG351V)
profile.MK-51049.best_validated.retrorun_adaptive_frameskip = false
profile.MK-51049.best_validated.retrorun_audio_buffer = 2048
profile.MK-51049.best_validated.retrorun_audio_stable_buffer = true
profile.MK-51049.best_validated.retrorun_frameskip = 0
profile.MK-51049.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.MK-51049.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.MK-51049.best_validated.retrorun_loop_declared_fps = false
profile.MK-51049.best_validated.reicast_hle_bios = enabled
profile.MK-51049.best_validated.reicast_gdrom_fast_loading = disabled
profile.MK-51049.best_validated.reicast_mipmapping = disabled
profile.MK-51049.best_validated.reicast_fog = disabled
profile.MK-51049.best_validated.reicast_frame_skipping = disabled
profile.MK-51049.best_validated.reicast_translucent_strip_merge = menu_guarded
profile.MK-51049.best_validated.reicast_translucent_menu_guard_strategy = hud_last
profile.MK-51049.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.MK-51049.best_validated.reicast_translucent_menu_guard_risk = 5
profile.MK-51049.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.MK-51049.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.MK-51049.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.MK-51049.best_validated.reicast_texture_storage_reuse = enabled
profile.MK-51049.best_validated.reicast_palette_fog_storage_reuse = disabled
profile.MK-51049.best_validated.reicast_adjacent_state_elision = disabled
profile.MK-51049.best_validated.reicast_fast_depth = menu_guarded
profile.MK-51049.best_validated.reicast_audio_mixer = fast
profile.MK-51049.best_validated.retrorun_egl_depth_bits = 24
profile.MK-51049.best_validated.retrorun_egl_stencil_bits = 0
profile.MK-51049.best_validated.reicast_aica_arm_cycles = 16
profile.MK-51049.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.MK-51049.best_validated.reicast_opaque_strip_merge = disabled

profile.MK-51049.best_performance.title = ChuChu Rocket (USA, RG351V)
profile.MK-51049.best_performance.inherits = best_validated

device.RG353M.profile.MK-51049.best_performance.title = ChuChu Rocket (USA, RG353M validated)
device.RG353M.profile.MK-51049.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.MK-51049.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-51049.best_performance.reicast_framerate = normal
device.RG353M.profile.MK-51049.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-51049.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.MK-51049.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.MK-51049.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.MK-51049.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-51049.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51049.best_performance.retrorun_go2_audio_wsola_profile = disabled

profile.T36812D61.best_validated.title = Power Stone 2 (Europe 61, baseline)

profile.T36812D64.best_validated.title = Power Stone 2 (Europe 64, baseline)

profile.T1218M.best_validated.title = Power Stone 2 (Japan, baseline)

profile.T1211N.best_validated.title = Power Stone 2 (USA, baseline)

device.RG353M.profile.T36812D61.best_performance.title = Power Stone 2 (Europe 61, RG353M validated)
device.RG353M.profile.T36812D61.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T36812D61.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T36812D61.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T36812D61.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T36812D61.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T36812D61.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T36812D61.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T36812D61.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T36812D61.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T36812D61.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.T36812D61.best_performance.reicast_gdrom_fast_loading = disabled
device.RG353M.profile.T36812D61.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T36812D61.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T36812D61.best_performance.reicast_framerate = normal
device.RG353M.profile.T36812D61.best_performance.reicast_loop_declared_fps = false
device.RG353M.profile.T36812D61.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T36812D61.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.T36812D61.best_performance.reicast_alpha_sorting = per-triangle (normal)
device.RG353M.profile.T36812D61.best_performance.reicast_mipmapping = disabled
device.RG353M.profile.T36812D61.best_performance.reicast_fog = disabled
device.RG353M.profile.T36812D61.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T36812D61.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T36812D61.best_performance.reicast_translucent_menu_guard_strategy = hud_last
device.RG353M.profile.T36812D61.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T36812D61.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.T36812D61.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.T36812D61.best_performance.reicast_fast_depth = menu_guarded
device.RG353M.profile.T36812D61.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T36812D61.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T36812D61.best_performance.reicast_aica_arm_cycles = 16

device.RG353M.profile.T36812D64.best_performance.title = Power Stone 2 (Europe 64, RG353M validated)
device.RG353M.profile.T36812D64.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T36812D64.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T36812D64.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T36812D64.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T36812D64.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T36812D64.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T36812D64.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T36812D64.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T36812D64.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T36812D64.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.T36812D64.best_performance.reicast_gdrom_fast_loading = disabled
device.RG353M.profile.T36812D64.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T36812D64.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T36812D64.best_performance.reicast_framerate = normal
device.RG353M.profile.T36812D64.best_performance.reicast_loop_declared_fps = false
device.RG353M.profile.T36812D64.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T36812D64.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.T36812D64.best_performance.reicast_alpha_sorting = per-triangle (normal)
device.RG353M.profile.T36812D64.best_performance.reicast_mipmapping = disabled
device.RG353M.profile.T36812D64.best_performance.reicast_fog = disabled
device.RG353M.profile.T36812D64.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T36812D64.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T36812D64.best_performance.reicast_translucent_menu_guard_strategy = hud_last
device.RG353M.profile.T36812D64.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T36812D64.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.T36812D64.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.T36812D64.best_performance.reicast_fast_depth = menu_guarded
device.RG353M.profile.T36812D64.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T36812D64.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T36812D64.best_performance.reicast_aica_arm_cycles = 16

device.RG353M.profile.T1218M.best_performance.title = Power Stone 2 (Japan, RG353M validated)
device.RG353M.profile.T1218M.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1218M.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1218M.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T1218M.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T1218M.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1218M.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1218M.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1218M.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T1218M.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1218M.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.T1218M.best_performance.reicast_gdrom_fast_loading = disabled
device.RG353M.profile.T1218M.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T1218M.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T1218M.best_performance.reicast_framerate = normal
device.RG353M.profile.T1218M.best_performance.reicast_loop_declared_fps = false
device.RG353M.profile.T1218M.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T1218M.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.T1218M.best_performance.reicast_alpha_sorting = per-triangle (normal)
device.RG353M.profile.T1218M.best_performance.reicast_mipmapping = disabled
device.RG353M.profile.T1218M.best_performance.reicast_fog = disabled
device.RG353M.profile.T1218M.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1218M.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T1218M.best_performance.reicast_translucent_menu_guard_strategy = hud_last
device.RG353M.profile.T1218M.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T1218M.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.T1218M.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.T1218M.best_performance.reicast_fast_depth = menu_guarded
device.RG353M.profile.T1218M.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T1218M.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T1218M.best_performance.reicast_aica_arm_cycles = 16

device.RG353M.profile.T1211N.best_performance.title = Power Stone 2 (USA, RG353M validated)
device.RG353M.profile.T1211N.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1211N.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1211N.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T1211N.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T1211N.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1211N.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1211N.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1211N.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T1211N.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1211N.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.T1211N.best_performance.reicast_gdrom_fast_loading = disabled
device.RG353M.profile.T1211N.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T1211N.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T1211N.best_performance.reicast_framerate = normal
device.RG353M.profile.T1211N.best_performance.reicast_loop_declared_fps = false
device.RG353M.profile.T1211N.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T1211N.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.T1211N.best_performance.reicast_alpha_sorting = per-triangle (normal)
device.RG353M.profile.T1211N.best_performance.reicast_mipmapping = disabled
device.RG353M.profile.T1211N.best_performance.reicast_fog = disabled
device.RG353M.profile.T1211N.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1211N.best_performance.reicast_translucent_strip_merge = menu_guarded
device.RG353M.profile.T1211N.best_performance.reicast_translucent_menu_guard_strategy = hud_last
device.RG353M.profile.T1211N.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T1211N.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.T1211N.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.T1211N.best_performance.reicast_fast_depth = menu_guarded
device.RG353M.profile.T1211N.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.T1211N.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T1211N.best_performance.reicast_aica_arm_cycles = 16

profile.MK-51052.best_validated.title = Skies of Arcadia (baseline)

profile.HDR-0076.best_validated.title = Eternal Arcadia (Japan, baseline)

profile.HDR-0109.best_validated.title = Eternal Arcadia (Japan revision, baseline)

profile.HDR-0119.best_validated.title = Eternal Arcadia @barai (Japan, baseline)

profile.T1249M.best_validated.title = Capcom vs. SNK 2 (Japan, baseline)

profile.MK-51100.best_validated.title = Phantasy Star Online (baseline)

profile.HDR-0129.best_validated.title = Phantasy Star Online (Japan, baseline)

profile.MK-51002.best_validated.title = The House of the Dead 2 (baseline)

profile.MK-5100250.best_validated.title = The House of the Dead 2 (Europe, observed CHD baseline)

device.RG353M.profile.MK-5100250.best_performance.title = The House of the Dead 2 (Europe, RG353M validated)
device.RG353M.profile.MK-5100250.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-5100250.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.MK-5100250.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.MK-5100250.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.MK-5100250.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-5100250.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-5100250.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.MK-5100250.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.MK-5100250.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.MK-5100250.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.MK-5100250.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.MK-5100250.best_performance.reicast_sh4clock = 200
device.RG353M.profile.MK-5100250.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-5100250.best_performance.reicast_framerate = normal
device.RG353M.profile.MK-5100250.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.MK-5100250.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.MK-5100250.best_performance.reicast_fog = enabled
device.RG353M.profile.MK-5100250.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.MK-5100250.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.MK-5100250.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-5100250.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.MK-5100250.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.MK-5100250.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.MK-5100250.best_performance.reicast_palette_fog_storage_reuse = enabled
device.RG353M.profile.MK-5100250.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.MK-5100250.best_performance.reicast_fast_depth = enabled
device.RG353M.profile.MK-5100250.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.MK-5100250.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.MK-5100250.best_performance.reicast_aica_arm_cycles = 32

profile.MK-55045.best_validated.title = The House of the Dead 2 (Europe revision, baseline)

profile.HDR-0007.best_validated.title = The House of the Dead 2 (Japan, baseline)

profile.HDR-0011.best_validated.title = The House of the Dead 2 (Japan revision, baseline)

profile.MK-51003.best_validated.title = NFL 2K (USA, baseline)

profile.HDR-0058.best_validated.title = NFL 2K (Japan, baseline)

profile.HDR-0141.best_validated.title = NFL 2K (Japan revision, baseline)

profile.MK-51062.best_validated.title = NFL 2K1 (USA, baseline)

profile.MK-51069.best_validated.title = NFL 2K1 (USA revision, baseline)

profile.HDR-0144.best_validated.title = NFL 2K1 (Japan, baseline)

profile.MK-51019.best_validated.title = Sega Rally 2 (baseline)

profile.HDR-0010.best_validated.title = Sega Rally 2 (Japan, baseline)

; RG351MP: confirmed visually with the USA release.  This intentionally uses
; the conservative renderer path; the generic aggressive configuration drops
; environment geometry in Sega Rally 2.  The title is Windows CE and its
; NoBatch AICA path requires 32 ARM samples per tick; lower values underproduce
; audio rather than being a valid performance optimization.  The safe LUT and
; SH4 scheduler settings below are measurably faster than the generic fallback
; while preserving the conservative renderer. best_performance falls back here
; until an independently validated faster path exists.
device.RG351MP.profile.MK-51019.best_validated.title = Sega Rally 2 (USA, RG351MP safe)
device.RG351MP.profile.MK-51019.best_validated.retrorun_loop_declared_fps = true
device.RG351MP.profile.MK-51019.best_validated.retrorun_adaptive_frameskip = false
device.RG351MP.profile.MK-51019.best_validated.retrorun_frameskip = 0
device.RG351MP.profile.MK-51019.best_validated.retrorun_audio_buffer = 4096
device.RG351MP.profile.MK-51019.best_validated.retrorun_audio_stable_buffer = true
device.RG351MP.profile.MK-51019.best_validated.retrorun_go2_audio_stretch_percent = 0
device.RG351MP.profile.MK-51019.best_validated.retrorun_go2_audio_stretch_low_ms = 150
device.RG351MP.profile.MK-51019.best_validated.retrorun_go2_audio_wsola_profile = lowend_heavy_100
device.RG351MP.profile.MK-51019.best_validated.retrorun_egl_depth_bits = 24
device.RG351MP.profile.MK-51019.best_validated.retrorun_egl_stencil_bits = 0
device.RG351MP.profile.MK-51019.best_validated.reicast_hle_bios = enabled
device.RG351MP.profile.MK-51019.best_validated.reicast_internal_resolution = 640x480
device.RG351MP.profile.MK-51019.best_validated.reicast_sh4clock = 200
device.RG351MP.profile.MK-51019.best_validated.reicast_alpha_sorting = per-triangle (normal)
device.RG351MP.profile.MK-51019.best_validated.reicast_gdrom_fast_loading = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_mipmapping = enabled
device.RG351MP.profile.MK-51019.best_validated.reicast_fog = enabled
device.RG351MP.profile.MK-51019.best_validated.reicast_volume_modifier_enable = enabled
device.RG351MP.profile.MK-51019.best_validated.reicast_enable_dsp = enabled
device.RG351MP.profile.MK-51019.best_validated.reicast_anisotropic_filtering = off
device.RG351MP.profile.MK-51019.best_validated.reicast_framerate = fullspeed
device.RG351MP.profile.MK-51019.best_validated.reicast_threaded_rendering = enabled
device.RG351MP.profile.MK-51019.best_validated.reicast_synchronous_rendering = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_delay_frame_swapping = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_frame_skipping = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_adjacent_state_elision = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_translucent_strip_merge = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_texture_storage_reuse = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_palette_fog_storage_reuse = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_fast_depth = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_audio_mixer = accurate
device.RG351MP.profile.MK-51019.best_validated.reicast_opaque_strip_merge = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_aica_arm_cycles = 32
device.RG351MP.profile.MK-51019.best_validated.reicast_shared_block_checks = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_mmu_address_lut = enabled
device.RG351MP.profile.MK-51019.best_validated.reicast_fmov_fpr64 = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_aica_better_lpf = disabled
device.RG351MP.profile.MK-51019.best_validated.reicast_sh4_cycle_mode = accurate

device.RG353M.profile.MK-51019.best_performance.title = Sega Rally 2 (RG353M validated)
device.RG353M.profile.MK-51019.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-51019.best_performance.retrorun_audio_buffer = 1470
device.RG353M.profile.MK-51019.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.MK-51019.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.MK-51019.best_performance.retrorun_go2_audio_stretch_percent = 10
device.RG353M.profile.MK-51019.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51019.best_performance.retrorun_go2_audio_wsola_profile = lowend_stable_96
device.RG353M.profile.MK-51019.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.MK-51019.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.MK-51019.best_performance.reicast_hle_bios = disabled
device.RG353M.profile.MK-51019.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.MK-51019.best_performance.reicast_sh4clock = 110
device.RG353M.profile.MK-51019.best_performance.reicast_sh4_cycle_mode = legacy
device.RG353M.profile.MK-51019.best_performance.reicast_shared_block_checks = enabled
device.RG353M.profile.MK-51019.best_performance.reicast_mmu_address_lut = enabled
device.RG353M.profile.MK-51019.best_performance.reicast_fmov_fpr64 = enabled
device.RG353M.profile.MK-51019.best_performance.reicast_aica_better_lpf = enabled
device.RG353M.profile.MK-51019.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-51019.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.MK-51019.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.MK-51019.best_performance.reicast_fog = enabled
device.RG353M.profile.MK-51019.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.MK-51019.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.MK-51019.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-51019.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.MK-51019.best_performance.reicast_audio_mixer = accurate
device.RG353M.profile.MK-51019.best_performance.reicast_aica_arm_cycles = 32

device.RG353M.profile.HDR-0010.best_performance.title = Sega Rally 2 (Japan, RG353M validated)
device.RG353M.profile.HDR-0010.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.HDR-0010.best_performance.retrorun_audio_buffer = 1470
device.RG353M.profile.HDR-0010.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.HDR-0010.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.HDR-0010.best_performance.retrorun_go2_audio_stretch_percent = 10
device.RG353M.profile.HDR-0010.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.HDR-0010.best_performance.retrorun_go2_audio_wsola_profile = lowend_stable_96
device.RG353M.profile.HDR-0010.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.HDR-0010.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.HDR-0010.best_performance.reicast_hle_bios = disabled
device.RG353M.profile.HDR-0010.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.HDR-0010.best_performance.reicast_sh4clock = 110
device.RG353M.profile.HDR-0010.best_performance.reicast_sh4_cycle_mode = legacy
device.RG353M.profile.HDR-0010.best_performance.reicast_shared_block_checks = enabled
device.RG353M.profile.HDR-0010.best_performance.reicast_mmu_address_lut = enabled
device.RG353M.profile.HDR-0010.best_performance.reicast_fmov_fpr64 = enabled
device.RG353M.profile.HDR-0010.best_performance.reicast_aica_better_lpf = enabled
device.RG353M.profile.HDR-0010.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.HDR-0010.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.HDR-0010.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.HDR-0010.best_performance.reicast_fog = enabled
device.RG353M.profile.HDR-0010.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.HDR-0010.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.HDR-0010.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.HDR-0010.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.HDR-0010.best_performance.reicast_audio_mixer = accurate
device.RG353M.profile.HDR-0010.best_performance.reicast_aica_arm_cycles = 32

profile.T9702D51.best_validated.title = Hydro Thunder (Europe, baseline)

profile.T9702N.best_validated.title = Hydro Thunder (USA, baseline)

profile.T8118D50.best_validated.title = F355 Challenge (Europe, baseline)

profile.T8119N.best_validated.title = F355 Challenge (USA, baseline)

profile.HDR-0100.best_validated.title = F355 Challenge (Japan, baseline)

profile.MK-51037.best_validated.title = Daytona USA 2001 / Daytona USA (RG351MP validated)
profile.MK-51037.best_validated.retrorun_audio_buffer = 2048
profile.MK-51037.best_validated.retrorun_audio_stable_buffer = true
profile.MK-51037.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.MK-51037.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.MK-51037.best_validated.retrorun_egl_depth_bits = 24
profile.MK-51037.best_validated.retrorun_egl_stencil_bits = 0
profile.MK-51037.best_validated.reicast_hle_bios = enabled
profile.MK-51037.best_validated.reicast_framerate = fullspeed
profile.MK-51037.best_validated.reicast_alpha_sorting = per-strip (fast, least accurate)
profile.MK-51037.best_validated.reicast_mipmapping = enabled
profile.MK-51037.best_validated.reicast_fog = enabled
profile.MK-51037.best_validated.reicast_frame_skipping = disabled
profile.MK-51037.best_validated.reicast_translucent_strip_merge = disabled
profile.MK-51037.best_validated.reicast_translucent_menu_guard_strategy = hud_last
profile.MK-51037.best_validated.reicast_texture_storage_reuse = disabled
profile.MK-51037.best_validated.reicast_adjacent_state_elision = disabled
profile.MK-51037.best_validated.reicast_fast_depth = disabled
profile.MK-51037.best_validated.reicast_audio_mixer = accurate
profile.MK-51037.best_validated.reicast_opaque_strip_merge = enabled
profile.MK-51037.best_validated.reicast_aica_arm_cycles = 32

device.RG353M.profile.MK-51037.best_performance.title = Daytona USA 2001 (USA, RG353M validated)
device.RG353M.profile.MK-51037.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-51037.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.MK-51037.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.MK-51037.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.MK-51037.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-51037.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51037.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.MK-51037.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.MK-51037.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.MK-51037.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_sh4clock = 200
device.RG353M.profile.MK-51037.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-51037.best_performance.reicast_framerate = normal
device.RG353M.profile.MK-51037.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.MK-51037.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_fog = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.MK-51037.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.MK-51037.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-51037.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.MK-51037.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_palette_fog_storage_reuse = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_adjacent_state_elision = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_fast_depth = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.MK-51037.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.MK-51037.best_performance.reicast_aica_arm_cycles = 32

profile.HDR-0106.best_validated.title = Daytona USA 2001 (Japan, baseline)

profile.MK-51001.best_validated.title = Virtua Fighter 3tb (baseline)

profile.HDR-0002.best_validated.title = Virtua Fighter 3tb (Japan, baseline)

profile.HDR-0017.best_validated.title = Virtua Fighter 3tb (Japan revision, baseline)

profile.HDR-0176.best_validated.title = Cosmic Smash (Japan, baseline)

profile.MK-51020.best_validated.title = Toy Commander (baseline)

profile.T46601D50.best_validated.title = Cannon Spike (Europe, baseline)

profile.T1215N.best_validated.title = Cannon Spike (USA, RG351MP validated)
profile.T1215N.best_validated.retrorun_audio_buffer = 2048
profile.T1215N.best_validated.retrorun_audio_stable_buffer = true
profile.T1215N.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.T1215N.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.T1215N.best_validated.retrorun_egl_depth_bits = 24
profile.T1215N.best_validated.retrorun_egl_stencil_bits = 0
profile.T1215N.best_validated.reicast_hle_bios = enabled
profile.T1215N.best_validated.reicast_framerate = fullspeed
profile.T1215N.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T1215N.best_validated.reicast_mipmapping = enabled
profile.T1215N.best_validated.reicast_fog = enabled
profile.T1215N.best_validated.reicast_frame_skipping = disabled
profile.T1215N.best_validated.reicast_translucent_strip_merge = disabled
profile.T1215N.best_validated.reicast_translucent_menu_guard_strategy = hud_last
profile.T1215N.best_validated.reicast_texture_storage_reuse = disabled
profile.T1215N.best_validated.reicast_adjacent_state_elision = disabled
profile.T1215N.best_validated.reicast_fast_depth = disabled
profile.T1215N.best_validated.reicast_audio_mixer = accurate
profile.T1215N.best_validated.reicast_opaque_strip_merge = enabled
profile.T1215N.best_validated.reicast_aica_arm_cycles = 32

device.RG353M.profile.T1215N.best_performance.title = Cannon Spike (USA, RG353M validated)
device.RG353M.profile.T1215N.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1215N.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1215N.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T1215N.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T1215N.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1215N.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1215N.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1215N.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.T1215N.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1215N.best_performance.reicast_hle_bios = enabled
device.RG353M.profile.T1215N.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.T1215N.best_performance.reicast_sh4clock = 200
device.RG353M.profile.T1215N.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T1215N.best_performance.reicast_framerate = normal
device.RG353M.profile.T1215N.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.T1215N.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.T1215N.best_performance.reicast_fog = enabled
device.RG353M.profile.T1215N.best_performance.reicast_volume_modifier_enable = disabled
device.RG353M.profile.T1215N.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.T1215N.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T1215N.best_performance.reicast_threaded_rendering = enabled
device.RG353M.profile.T1215N.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.T1215N.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T1215N.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.T1215N.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.T1215N.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
device.RG353M.profile.T1215N.best_performance.reicast_audio_mixer = lowend
device.RG353M.profile.T1215N.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.T1215N.best_performance.reicast_aica_arm_cycles = 32

profile.T1219M.best_validated.title = Gunspike (Japan, baseline)

profile.MK-51192.best_validated.title = Rez (Europe, baseline)

profile.HDR-0178.best_validated.title = Rez (Japan, baseline)

profile.T7013D50.best_validated.title = Street Fighter III: 3rd Strike (Europe, RG351MP validated)
profile.T7013D50.best_validated.retrorun_audio_buffer = 2048
profile.T7013D50.best_validated.retrorun_audio_stable_buffer = true
profile.T7013D50.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.T7013D50.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.T7013D50.best_validated.retrorun_egl_depth_bits = 24
profile.T7013D50.best_validated.retrorun_egl_stencil_bits = 8
profile.T7013D50.best_validated.reicast_hle_bios = enabled
profile.T7013D50.best_validated.reicast_gdrom_fast_loading = disabled
profile.T7013D50.best_validated.reicast_framerate = fullspeed
profile.T7013D50.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T7013D50.best_validated.reicast_mipmapping = enabled
profile.T7013D50.best_validated.reicast_fog = enabled
profile.T7013D50.best_validated.reicast_frame_skipping = disabled
profile.T7013D50.best_validated.reicast_translucent_strip_merge = disabled
profile.T7013D50.best_validated.reicast_texture_storage_reuse = disabled
profile.T7013D50.best_validated.reicast_adjacent_state_elision = disabled
profile.T7013D50.best_validated.reicast_fast_depth = vertex_fast_log
profile.T7013D50.best_validated.reicast_audio_mixer = accurate
profile.T7013D50.best_validated.reicast_opaque_strip_merge = enabled
profile.T7013D50.best_validated.reicast_aica_arm_cycles = 32
profile.T7013D50.best_performance.title = Street Fighter III: 3rd Strike (Europe, best performance)
profile.T7013D50.best_performance.inherits = best_validated

profile.T1213N.best_validated.title = Street Fighter III: 3rd Strike (USA, RG351MP validated)
profile.T1213N.best_validated.retrorun_audio_buffer = 2048
profile.T1213N.best_validated.retrorun_audio_stable_buffer = true
profile.T1213N.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.T1213N.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.T1213N.best_validated.retrorun_egl_depth_bits = 24
profile.T1213N.best_validated.retrorun_egl_stencil_bits = 8
profile.T1213N.best_validated.reicast_hle_bios = enabled
profile.T1213N.best_validated.reicast_gdrom_fast_loading = disabled
profile.T1213N.best_validated.reicast_framerate = fullspeed
profile.T1213N.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T1213N.best_validated.reicast_mipmapping = enabled
profile.T1213N.best_validated.reicast_fog = enabled
profile.T1213N.best_validated.reicast_frame_skipping = disabled
profile.T1213N.best_validated.reicast_translucent_strip_merge = disabled
profile.T1213N.best_validated.reicast_texture_storage_reuse = disabled
profile.T1213N.best_validated.reicast_adjacent_state_elision = disabled
profile.T1213N.best_validated.reicast_fast_depth = vertex_fast_log
profile.T1213N.best_validated.reicast_audio_mixer = accurate
profile.T1213N.best_validated.reicast_opaque_strip_merge = enabled
profile.T1213N.best_validated.reicast_aica_arm_cycles = 32
profile.T1213N.best_performance.title = Street Fighter III: 3rd Strike (USA, best performance)
profile.T1213N.best_performance.inherits = best_validated

profile.T1209M.best_validated.title = Street Fighter III: 3rd Strike (Japan, RG351MP validated)
profile.T1209M.best_validated.retrorun_audio_buffer = 2048
profile.T1209M.best_validated.retrorun_audio_stable_buffer = true
profile.T1209M.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.T1209M.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.T1209M.best_validated.retrorun_egl_depth_bits = 24
profile.T1209M.best_validated.retrorun_egl_stencil_bits = 8
profile.T1209M.best_validated.reicast_hle_bios = enabled
profile.T1209M.best_validated.reicast_gdrom_fast_loading = disabled
profile.T1209M.best_validated.reicast_framerate = fullspeed
profile.T1209M.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.T1209M.best_validated.reicast_mipmapping = enabled
profile.T1209M.best_validated.reicast_fog = enabled
profile.T1209M.best_validated.reicast_frame_skipping = disabled
profile.T1209M.best_validated.reicast_translucent_strip_merge = disabled
profile.T1209M.best_validated.reicast_texture_storage_reuse = disabled
profile.T1209M.best_validated.reicast_adjacent_state_elision = disabled
profile.T1209M.best_validated.reicast_fast_depth = vertex_fast_log
profile.T1209M.best_validated.reicast_audio_mixer = accurate
profile.T1209M.best_validated.reicast_opaque_strip_merge = enabled
profile.T1209M.best_validated.reicast_aica_arm_cycles = 32
profile.T1209M.best_performance.title = Street Fighter III: 3rd Strike (Japan, best performance)
profile.T1209M.best_performance.inherits = best_validated

device.RG351MP.profile.T7013D50.best_performance.title = Street Fighter III: 3rd Strike (Europe, RG351MP upstream 620 validated)
device.RG351MP.profile.T7013D50.best_performance.retrorun_flycast_core_variant = upstream_620

device.RG351MP.profile.T1213N.best_performance.title = Street Fighter III: 3rd Strike (USA, RG351MP upstream 620 validated)
device.RG351MP.profile.T1213N.best_performance.retrorun_flycast_core_variant = upstream_620

device.RG351MP.profile.T1209M.best_performance.title = Street Fighter III: 3rd Strike (Japan, RG351MP upstream 620 validated)
device.RG351MP.profile.T1209M.best_performance.retrorun_flycast_core_variant = upstream_620

device.RG353M.profile.T7013D50.best_performance.title = Street Fighter III: 3rd Strike (Europe, RG353M validated)
device.RG353M.profile.T7013D50.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T7013D50.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T7013D50.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T7013D50.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T7013D50.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T7013D50.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T7013D50.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T7013D50.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T7013D50.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T7013D50.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T7013D50.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T7013D50.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.T7013D50.best_performance.reicast_framerate = normal
device.RG353M.profile.T7013D50.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T7013D50.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
device.RG353M.profile.T7013D50.best_performance.reicast_audio_mixer = lowend

device.RG353M.profile.T1213N.best_performance.title = Street Fighter III: 3rd Strike (USA, RG353M validated)
device.RG353M.profile.T1213N.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T1213N.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T1213N.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T1213N.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1213N.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1213N.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T1213N.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T1213N.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1213N.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1213N.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1213N.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1213N.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.T1213N.best_performance.reicast_framerate = normal
device.RG353M.profile.T1213N.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T1213N.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
device.RG353M.profile.T1213N.best_performance.reicast_audio_mixer = lowend

device.RG353M.profile.T1209M.best_performance.title = Street Fighter III: 3rd Strike (Japan, RG353M validated)
device.RG353M.profile.T1209M.best_performance.retrorun_flycast_core_variant = upstream_620
device.RG353M.profile.T1209M.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.T1209M.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.T1209M.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.T1209M.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.T1209M.best_performance.retrorun_audio_stable_buffer = true
device.RG353M.profile.T1209M.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.T1209M.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.T1209M.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.T1209M.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.T1209M.best_performance.retrorun_egl_stencil_bits = 0
device.RG353M.profile.T1209M.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.T1209M.best_performance.reicast_framerate = normal
device.RG353M.profile.T1209M.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.T1209M.best_performance.reicast_fast_depth = menu_guarded_shadow_safe
device.RG353M.profile.T1209M.best_performance.reicast_audio_mixer = lowend

profile.T7005D50.best_validated.title = Street Fighter Alpha 3 (Europe, baseline)

profile.T1203N.best_validated.title = Street Fighter Alpha 3 (USA, baseline)

profile.T1203M.best_validated.title = Street Fighter Zero 3 (Japan, baseline)

profile.T1230M.best_validated.title = Street Fighter Zero 3 Matching Service (Japan, baseline)

profile.MK-51006.best_validated.title = Sega Bass Fishing (baseline)

profile.HDR-0012.best_validated.title = Get Bass (Japan, baseline)

profile.MK-51035.best_validated.title = Crazy Taxi
profile.MK-51035.best_validated.retrorun_adaptive_frameskip = false
profile.MK-51035.best_validated.retrorun_audio_buffer = 2048
profile.MK-51035.best_validated.retrorun_audio_stable_buffer = true
profile.MK-51035.best_validated.retrorun_frameskip = 0
profile.MK-51035.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.MK-51035.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.MK-51035.best_validated.retrorun_loop_declared_fps = true
profile.MK-51035.best_validated.reicast_hle_bios = enabled
profile.MK-51035.best_validated.reicast_gdrom_fast_loading = disabled
profile.MK-51035.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.MK-51035.best_validated.reicast_frame_skipping = disabled
profile.MK-51035.best_validated.reicast_translucent_strip_merge = disabled
profile.MK-51035.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
profile.MK-51035.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.MK-51035.best_validated.reicast_translucent_menu_guard_risk = 5
profile.MK-51035.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.MK-51035.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.MK-51035.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.MK-51035.best_validated.reicast_fast_depth = menu_guarded_shadow_safe
profile.MK-51035.best_validated.reicast_audio_mixer = lowend
profile.MK-51035.best_validated.reicast_opaque_strip_merge = disabled
profile.MK-51035.best_validated.retrorun_egl_depth_bits = 24
profile.MK-51035.best_validated.retrorun_egl_stencil_bits = 8
profile.MK-51035.best_validated.reicast_aica_arm_cycles = 32

profile.HDR-0053.best_validated.title = Crazy Taxi (Japan)
profile.HDR-0053.best_validated.retrorun_adaptive_frameskip = false
profile.HDR-0053.best_validated.retrorun_audio_buffer = 2048
profile.HDR-0053.best_validated.retrorun_audio_stable_buffer = true
profile.HDR-0053.best_validated.retrorun_frameskip = 0
profile.HDR-0053.best_validated.retrorun_go2_audio_stretch_low_ms = 150
profile.HDR-0053.best_validated.retrorun_go2_audio_wsola_profile = lowend_stable_96
profile.HDR-0053.best_validated.retrorun_loop_declared_fps = true
profile.HDR-0053.best_validated.reicast_hle_bios = enabled
profile.HDR-0053.best_validated.reicast_gdrom_fast_loading = disabled
profile.HDR-0053.best_validated.reicast_alpha_sorting = per-triangle (normal)
profile.HDR-0053.best_validated.reicast_frame_skipping = disabled
profile.HDR-0053.best_validated.reicast_translucent_strip_merge = disabled
profile.HDR-0053.best_validated.reicast_translucent_menu_guard_strategy = top_hud_last
profile.HDR-0053.best_validated.reicast_translucent_menu_guard_max_vertices = 64
profile.HDR-0053.best_validated.reicast_translucent_menu_guard_risk = 5
profile.HDR-0053.best_validated.reicast_translucent_menu_guard_depth_tolerance = 0.0001
profile.HDR-0053.best_validated.reicast_translucent_menu_guard_overlap = risky
profile.HDR-0053.best_validated.reicast_translucent_menu_guard_draw_sorting = standard
profile.HDR-0053.best_validated.reicast_fast_depth = menu_guarded_shadow_safe
profile.HDR-0053.best_validated.reicast_audio_mixer = lowend
profile.HDR-0053.best_validated.reicast_opaque_strip_merge = disabled
profile.HDR-0053.best_validated.retrorun_egl_depth_bits = 24
profile.HDR-0053.best_validated.retrorun_egl_stencil_bits = 8
profile.HDR-0053.best_validated.reicast_aica_arm_cycles = 32

device.RG353M.profile.MK-51035.best_performance.title = Crazy Taxi (USA/Europe, RG353M candidate)
device.RG353M.profile.MK-51035.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.MK-51035.best_performance.reicast_framerate = normal
device.RG353M.profile.MK-51035.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.MK-51035.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.MK-51035.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.MK-51035.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.MK-51035.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.MK-51035.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.MK-51035.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.MK-51035.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.MK-51035.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.MK-51035.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.MK-51035.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.MK-51035.best_performance.reicast_fog = enabled
device.RG353M.profile.MK-51035.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.MK-51035.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.MK-51035.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.MK-51035.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.MK-51035.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.MK-51035.best_performance.reicast_fast_depth = disabled
device.RG353M.profile.MK-51035.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.MK-51035.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.MK-51035.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.MK-51035.best_performance.retrorun_egl_stencil_bits = 8
device.RG353M.profile.MK-51035.best_performance.reicast_aica_arm_cycles = 16

device.RG353M.profile.HDR-0053.best_performance.title = Crazy Taxi (Japan, RG353M candidate)
device.RG353M.profile.HDR-0053.best_performance.reicast_internal_resolution = 640x480
device.RG353M.profile.HDR-0053.best_performance.reicast_framerate = normal
device.RG353M.profile.HDR-0053.best_performance.reicast_anisotropic_filtering = off
device.RG353M.profile.HDR-0053.best_performance.retrorun_loop_declared_fps = false
device.RG353M.profile.HDR-0053.best_performance.retrorun_audio_buffer = 735
device.RG353M.profile.HDR-0053.best_performance.retrorun_audio_stable_buffer = false
device.RG353M.profile.HDR-0053.best_performance.retrorun_go2_audio_prebuffer_ms = 60
device.RG353M.profile.HDR-0053.best_performance.retrorun_go2_audio_stretch_percent = 0
device.RG353M.profile.HDR-0053.best_performance.retrorun_go2_audio_stretch_low_ms = 40
device.RG353M.profile.HDR-0053.best_performance.retrorun_go2_audio_wsola_profile = disabled
device.RG353M.profile.HDR-0053.best_performance.reicast_gdrom_fast_loading = enabled
device.RG353M.profile.HDR-0053.best_performance.reicast_alpha_sorting = per-strip (fast, least accurate)
device.RG353M.profile.HDR-0053.best_performance.reicast_mipmapping = enabled
device.RG353M.profile.HDR-0053.best_performance.reicast_fog = enabled
device.RG353M.profile.HDR-0053.best_performance.reicast_frame_skipping = disabled
device.RG353M.profile.HDR-0053.best_performance.reicast_translucent_strip_merge = disabled
device.RG353M.profile.HDR-0053.best_performance.reicast_texture_storage_reuse = enabled
device.RG353M.profile.HDR-0053.best_performance.reicast_palette_fog_storage_reuse = disabled
device.RG353M.profile.HDR-0053.best_performance.reicast_adjacent_state_elision = disabled
device.RG353M.profile.HDR-0053.best_performance.reicast_fast_depth = disabled
device.RG353M.profile.HDR-0053.best_performance.reicast_audio_mixer = fast
device.RG353M.profile.HDR-0053.best_performance.reicast_opaque_strip_merge = enabled
device.RG353M.profile.HDR-0053.best_performance.retrorun_egl_depth_bits = 24
device.RG353M.profile.HDR-0053.best_performance.retrorun_egl_stencil_bits = 8
device.RG353M.profile.HDR-0053.best_performance.reicast_aica_arm_cycles = 16
)catalog";

struct RawProfile
{
    std::string title;
    Mode mode = Mode::Invalid;
    std::string inheritance;
    std::map<std::string, std::string> settings;
};

const std::unordered_set<std::string> &allowedSettings()
{
    static const std::unordered_set<std::string> settings = {
        "retrorun_adaptive_frameskip",
        "retrorun_audio_buffer",
        "retrorun_audio_stable_buffer",
        "retrorun_drm_direct_scanout",
        "retrorun_egl_depth_bits",
        "retrorun_egl_stencil_bits",
        "retrorun_force_audio_multithread",
        "retrorun_force_video_multithread",
        "retrorun_flycast_core_variant",
        "retrorun_frameskip",
        "retrorun_go2_audio_prebuffer_ms",
        "retrorun_go2_audio_stretch_low_ms",
        "retrorun_go2_audio_stretch_percent",
        "retrorun_go2_audio_wsola_profile",
        "retrorun_loop_declared_fps",
        "retrorun_sdl_audio_stretch_low_ms",
        "retrorun_sdl_audio_stretch_percent",
        "retrorun_vsync",
        "reicast_adjacent_state_elision",
        "reicast_accurate_aica_batch",
        "reicast_aica_better_lpf",
        "reicast_aica_arm_cycles",
        "reicast_alpha_sorting",
        "reicast_anisotropic_filtering",
        "reicast_audio_mixer",
        "reicast_boot_to_bios",
        "reicast_broadcast",
        "reicast_cable_type",
        "reicast_cpu_mode",
        "reicast_custom_textures",
        "reicast_delay_frame_swapping",
        "reicast_div_matching",
        "reicast_dump_textures",
        "reicast_enable_dsp",
        "reicast_enable_purupuru",
        "reicast_enable_rttb",
        "reicast_fast_depth",
        "reicast_fog",
        "reicast_force_wince",
        "reicast_fmov_fpr64",
        "reicast_loop_declared_fps",
        "reicast_frame_skipping",
        "reicast_framerate",
        "reicast_gdrom_fast_loading",
        "reicast_hle_bios",
        "reicast_internal_resolution",
        "reicast_mipmapping",
        "reicast_mmu_address_lut",
        "reicast_opaque_strip_merge",
        "reicast_palette_fog_storage_reuse",
        "reicast_pvr2_filtering",
        "reicast_render_to_texture_upscaling",
        "reicast_screen_rotation",
        "reicast_sh4clock",
        "reicast_sh4_cycle_mode",
        "reicast_shared_block_checks",
        "reicast_synchronous_rendering",
        "reicast_system",
        "reicast_texture_storage_reuse",
        "reicast_texupscale",
        "reicast_texupscale_max_filtered_texture_size",
        "reicast_threaded_rendering",
        "reicast_translucent_menu_guard_depth_tolerance",
        "reicast_translucent_menu_guard_draw_sorting",
        "reicast_translucent_menu_guard_max_vertices",
        "reicast_translucent_menu_guard_overlap",
        "reicast_translucent_menu_guard_risk",
        "reicast_translucent_menu_guard_strategy",
        "reicast_translucent_strip_merge",
        "reicast_volume_modifier_enable",
        "reicast_widescreen_cheats",
        "reicast_widescreen_hack"
    };
    return settings;
}

bool splitProfileKey(const std::string &key, std::string &product,
                     std::string &mode, std::string &field)
{
    constexpr const char *prefix = "profile.";
    if (key.compare(0, std::char_traits<char>::length(prefix), prefix) != 0)
        return false;

    const std::size_t productStart = std::char_traits<char>::length(prefix);
    const std::size_t productEnd = key.find('.', productStart);
    if (productEnd == std::string::npos)
        return false;
    const std::size_t modeEnd = key.find('.', productEnd + 1);
    if (modeEnd == std::string::npos || modeEnd + 1 >= key.size())
        return false;

    product = key.substr(productStart, productEnd - productStart);
    mode = key.substr(productEnd + 1, modeEnd - productEnd - 1);
    field = key.substr(modeEnd + 1);
    return !product.empty() && !mode.empty() && !field.empty();
}

bool splitDeviceProfileKey(const std::string &key, std::string &device,
                           std::string &product, std::string &mode,
                           std::string &field)
{
    constexpr const char *prefix = "device.";
    constexpr const char *profileMarker = ".profile.";
    if (key.compare(0, std::char_traits<char>::length(prefix), prefix) != 0)
        return false;

    const std::size_t deviceStart =
        std::char_traits<char>::length(prefix);
    const std::size_t deviceEnd = key.find(profileMarker, deviceStart);
    if (deviceEnd == std::string::npos || deviceEnd == deviceStart)
        return false;

    device = key.substr(deviceStart, deviceEnd - deviceStart);
    const std::string profileKey =
        "profile." + key.substr(deviceEnd +
            std::char_traits<char>::length(profileMarker));
    return splitProfileKey(profileKey, product, mode, field);
}

bool validTextValue(const std::string &value, std::size_t maximum)
{
    if (value.empty() || value.size() > maximum)
        return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isprint(character) || character == '\t';
    });
}

bool resolveProfile(const std::string &product, Mode mode,
                    const std::map<std::pair<std::string, Mode>, RawProfile> &raw,
                    const std::map<std::string, std::string> &defaults,
                    const std::map<std::string, std::string> &safeDefaults,
                    Profile &profile, std::vector<std::string> &diagnostics)
{
    const auto found = raw.find({product, mode});
    if (found == raw.end())
        return false;

    profile = {};
    profile.product_number = product;
    profile.mode = mode;
    const RawProfile &record = found->second;
    const bool titleOnlyBaseline =
        mode == Mode::BestValidated && record.settings.empty() &&
        record.inheritance.empty();
    profile.validated = !titleOnlyBaseline;
    profile.settings = titleOnlyBaseline ? safeDefaults : defaults;

    if (!record.inheritance.empty())
    {
        const Mode inheritedMode = parseMode(record.inheritance);
        if (inheritedMode != Mode::BestValidated ||
            mode != Mode::BestPerformance)
        {
            diagnostics.push_back(
                "profile." + product + "." + modeName(mode) +
                ": only best_performance may inherit best_validated");
            return false;
        }
        Profile inherited;
        if (!resolveProfile(product, inheritedMode, raw, defaults,
                            safeDefaults, inherited, diagnostics))
        {
            diagnostics.push_back(
                "profile." + product +
                ".best_performance: missing inherited best_validated profile");
            return false;
        }
        profile.settings = inherited.settings;
        profile.title = inherited.title;
        profile.validated = inherited.validated;
    }

    if (!record.title.empty())
        profile.title = record.title;
    profile.settings.insert(record.settings.begin(), record.settings.end());
    for (const auto &[setting, value] : record.settings)
        profile.settings[setting] = value;
    if (!record.settings.empty())
        profile.validated = true;

    if (profile.title.empty())
        profile.title = product;
    return true;
}

using DeviceProfileIdentity = std::tuple<std::string, std::string, Mode>;

bool resolveDeviceProfile(
    const std::string &device, const std::string &product, Mode mode,
    const std::map<DeviceProfileIdentity, RawProfile> &raw,
    const std::map<std::string, std::map<Mode, Profile>> &globalProfiles,
    Profile &profile, std::vector<std::string> &diagnostics)
{
    const auto found = raw.find({device, product, mode});
    if (found == raw.end())
        return false;

    const RawProfile &record = found->second;
    profile = {};
    profile.product_number = product;
    profile.mode = mode;

    if (!record.inheritance.empty())
    {
        const Mode inheritedMode = parseMode(record.inheritance);
        if (mode != Mode::BestPerformance ||
            inheritedMode != Mode::BestValidated ||
            !resolveDeviceProfile(device, product, inheritedMode, raw,
                                  globalProfiles, profile, diagnostics))
        {
            diagnostics.push_back(
                "device." + device + ".profile." + product + "." +
                modeName(mode) +
                ": only best_performance may inherit an existing device best_validated profile");
            return false;
        }
        profile.mode = mode;
    }
    else
    {
        const auto globalProduct = globalProfiles.find(product);
        if (globalProduct == globalProfiles.end())
        {
            diagnostics.push_back(
                "device." + device + ".profile." + product +
                ": missing global product profile");
            return false;
        }
        auto globalMode = globalProduct->second.find(mode);
        if (globalMode == globalProduct->second.end() &&
            mode == Mode::BestPerformance)
            globalMode = globalProduct->second.find(Mode::BestValidated);
        if (globalMode == globalProduct->second.end())
        {
            diagnostics.push_back(
                "device." + device + ".profile." + product + "." +
                modeName(mode) + ": missing global base profile");
            return false;
        }
        profile.settings = globalMode->second.settings;
        profile.title = globalMode->second.title;
        profile.validated = globalMode->second.validated;
    }

    if (!record.title.empty())
        profile.title = record.title;
    for (const auto &[setting, value] : record.settings)
        profile.settings[setting] = value;
    if (!record.settings.empty())
        profile.validated = true;
    return true;
}

} // namespace

Mode parseMode(const std::string &value)
{
    if (value == "disabled")
        return Mode::Disabled;
    if (value == "best_validated")
        return Mode::BestValidated;
    if (value == "best_performance")
        return Mode::BestPerformance;
    return Mode::Invalid;
}

const char *modeName(Mode mode)
{
    switch (mode)
    {
    case Mode::Disabled: return "disabled";
    case Mode::BestValidated: return "best_validated";
    case Mode::BestPerformance: return "best_performance";
    case Mode::Invalid: return "invalid";
    }
    return "invalid";
}

std::string normalizeProductNumber(const std::string &product_number)
{
    std::string normalized;
    normalized.reserve(product_number.size());
    for (const unsigned char character : product_number)
    {
        if (std::isspace(character))
            continue;
        normalized.push_back(static_cast<char>(std::toupper(character)));
    }
    return normalized;
}

std::string normalizeDeviceName(const std::string &device_name)
{
    return normalizeProductNumber(device_name);
}

bool parseCatalog(std::istream &input, const std::string &source,
                  Catalog &catalog, std::vector<std::string> &diagnostics)
{
    catalog = {};
    catalog.source = source;
    diagnostics.clear();

    const rr::config::Document document = rr::config::parse(input);
    for (const rr::config::Diagnostic &diagnostic : document.diagnostics)
    {
        diagnostics.push_back(
            "line " + std::to_string(diagnostic.line) + ": " +
            diagnostic.message);
    }
    if (!document.diagnostics.empty())
        return false;

    const auto schema = document.values.find("schema_version");
    const auto version = document.values.find("catalog_version");
    if (schema == document.values.end() ||
        !rr::config::parseInteger(schema->second, 1, 100,
                                  catalog.schema_version))
        diagnostics.push_back("missing or invalid schema_version");
    if (version == document.values.end() ||
        !rr::config::parseInteger(version->second, 1, 2147483647,
                                  catalog.catalog_version))
        diagnostics.push_back("missing or invalid catalog_version");
    if (catalog.schema_version < MinimumSchemaVersion ||
        catalog.schema_version > CurrentSchemaVersion)
        diagnostics.push_back(
            "unsupported schema_version " +
            std::to_string(catalog.schema_version));

    std::map<std::string, std::string> defaults;
    std::map<std::pair<std::string, Mode>, RawProfile> rawProfiles;
    std::map<DeviceProfileIdentity, RawProfile> rawDeviceProfiles;
    for (const auto &[key, value] : document.values)
    {
        if (key == "schema_version" || key == "catalog_version")
            continue;

        constexpr const char *defaultPrefix = "default.";
        if (key.compare(0, std::char_traits<char>::length(defaultPrefix),
                        defaultPrefix) == 0)
        {
            const std::string setting =
                key.substr(std::char_traits<char>::length(defaultPrefix));
            if (allowedSettings().find(setting) == allowedSettings().end())
                diagnostics.push_back("setting '" + setting +
                                      "' is not allowed in catalog defaults");
            else if (!validTextValue(value, 256))
                diagnostics.push_back("invalid value for default setting '" +
                                      setting + "'");
            else
                defaults[setting] = value;
            continue;
        }

        std::string device;
        std::string product;
        std::string modeText;
        std::string field;
        const bool deviceScoped =
            splitDeviceProfileKey(key, device, product, modeText, field);
        if (!deviceScoped &&
            !splitProfileKey(key, product, modeText, field))
        {
            diagnostics.push_back("unknown catalog key '" + key + "'");
            continue;
        }

        if (deviceScoped)
        {
            const std::string normalizedDevice = normalizeDeviceName(device);
            if (catalog.schema_version < 2)
            {
                diagnostics.push_back(
                    "device profiles require schema_version 2");
                continue;
            }
            if (normalizedDevice != device || normalizedDevice.empty())
            {
                diagnostics.push_back("device key '" + device +
                                      "' is not normalized");
                continue;
            }
        }

        const std::string normalizedProduct = normalizeProductNumber(product);
        const Mode mode = parseMode(modeText);
        if (normalizedProduct != product || normalizedProduct.empty())
        {
            diagnostics.push_back("product key '" + product +
                                  "' is not normalized");
            continue;
        }
        if (mode != Mode::BestValidated && mode != Mode::BestPerformance)
        {
            diagnostics.push_back("invalid profile mode '" + modeText + "'");
            continue;
        }

        RawProfile &profile = deviceScoped
            ? rawDeviceProfiles[{device, product, mode}]
            : rawProfiles[{product, mode}];
        profile.mode = mode;
        if (field == "title")
        {
            if (!validTextValue(value, 128))
                diagnostics.push_back("invalid title for product '" +
                                      product + "'");
            else
                profile.title = value;
        }
        else if (field == "inherits")
        {
            profile.inheritance = value;
        }
        else if (allowedSettings().find(field) == allowedSettings().end())
        {
            diagnostics.push_back("setting '" + field +
                                  "' is not allowed in profiles");
        }
        else if (!validTextValue(value, 256))
        {
            diagnostics.push_back("invalid value for profile setting '" +
                                  field + "'");
        }
        else
        {
            profile.settings[field] = value;
        }
    }

    if (!diagnostics.empty())
        return false;
    if (rawProfiles.empty())
    {
        diagnostics.push_back("catalog contains no profiles");
        return false;
    }

    // Catalog defaults intentionally retain the established low-end
    // performance foundation for profiles that have explicit, validated
    // settings. A title-only baseline is not such a validation: use accurate
    // graphics/audio choices there, and for uncataloged content, so a generic
    // fallback cannot make sprites or effects disappear.
    std::map<std::string, std::string> safeDefaults = defaults;
    safeDefaults["reicast_alpha_sorting"] = "per-triangle (normal)";
    safeDefaults["reicast_translucent_strip_merge"] = "disabled";
    safeDefaults["reicast_texture_storage_reuse"] = "disabled";
    safeDefaults["reicast_opaque_strip_merge"] = "disabled";
    safeDefaults["reicast_adjacent_state_elision"] = "disabled";
    safeDefaults["reicast_fast_depth"] = "disabled";
    catalog.safe_defaults = safeDefaults;

    for (const auto &[identity, raw] : rawProfiles)
    {
        (void)raw;
        Profile profile;
        if (!resolveProfile(identity.first, identity.second, rawProfiles,
                            defaults, safeDefaults, profile, diagnostics))
            continue;
        catalog.profiles[identity.first][identity.second] =
            std::move(profile);
    }

    for (const auto &[identity, raw] : rawDeviceProfiles)
    {
        (void)raw;
        const auto &[device, product, mode] = identity;
        Profile profile;
        if (!resolveDeviceProfile(device, product, mode, rawDeviceProfiles,
                                  catalog.profiles, profile, diagnostics))
            continue;
        catalog.device_profiles[device][product][mode] = std::move(profile);
    }
    return diagnostics.empty() && !catalog.profiles.empty();
}

bool loadCatalogFile(const std::string &path, Catalog &catalog,
                     std::vector<std::string> &diagnostics)
{
    std::ifstream input(path);
    if (!input.good())
    {
        diagnostics = {"unable to open catalog"};
        catalog = {};
        return false;
    }
    return parseCatalog(input, path, catalog, diagnostics);
}

Catalog builtinCatalog()
{
    std::istringstream input(BuiltinCatalogText);
    Catalog catalog;
    std::vector<std::string> diagnostics;
    if (!parseCatalog(input, "built-in", catalog, diagnostics))
        return {};
    return catalog;
}

std::string localCatalogPath(const char *argv0)
{
    namespace fs = std::filesystem;
    std::error_code error;
#ifdef __linux__
    const fs::path procExecutable = fs::read_symlink("/proc/self/exe", error);
    if (!error && !procExecutable.empty())
        return (procExecutable.parent_path() / CatalogFilename).string();
#endif
    error.clear();
    fs::path executable = argv0 && *argv0 ? fs::path(argv0) : fs::path();
    if (executable.empty())
        executable = fs::current_path(error) / "retrorun";
    else if (executable.is_relative())
        executable = fs::absolute(executable, error);
    if (error)
        return CatalogFilename;
    return (executable.parent_path() / CatalogFilename).string();
}

std::string cachedCatalogPath(const std::string &active_config_file)
{
    namespace fs = std::filesystem;
    std::error_code error;
    fs::path configPath(active_config_file);
    fs::path directory = configPath.has_parent_path()
        ? configPath.parent_path()
        : fs::current_path(error);
    if (error || directory.empty())
        directory = ".";
    return (directory / "flycast-game-catalog.cache.ini").string();
}

bool selectProfile(const Catalog &catalog, const std::string &product_number,
                   Mode mode, Profile &profile, bool &used_fallback,
                   const std::string &device_name)
{
    used_fallback = false;
    profile = {};
    if (mode != Mode::BestValidated && mode != Mode::BestPerformance)
        return false;

    const std::string normalized = normalizeProductNumber(product_number);
    const std::string normalizedDevice = normalizeDeviceName(device_name);

    const auto device = catalog.device_profiles.find(normalizedDevice);
    if (device != catalog.device_profiles.end())
    {
        const auto deviceProduct = device->second.find(normalized);
        if (deviceProduct != device->second.end())
        {
            auto selected = deviceProduct->second.find(mode);
            if (selected == deviceProduct->second.end() &&
                mode == Mode::BestPerformance)
            {
                selected =
                    deviceProduct->second.find(Mode::BestValidated);
                used_fallback =
                    selected != deviceProduct->second.end();
            }
            if (selected != deviceProduct->second.end() &&
                selected->second.validated)
            {
                profile = selected->second;
                return true;
            }
            used_fallback = false;
        }
    }

    const auto product = catalog.profiles.find(normalized);
    if (product == catalog.profiles.end())
        return false;

    auto selected = product->second.find(mode);
    if (selected == product->second.end() &&
        mode == Mode::BestPerformance)
    {
        selected = product->second.find(Mode::BestValidated);
        used_fallback = selected != product->second.end();
    }
    if (selected == product->second.end() || !selected->second.validated)
    {
        used_fallback = false;
        return false;
    }

    profile = selected->second;
    return true;
}

std::map<std::string, Profile> validatedCatalogProfiles(
    const Catalog &catalog)
{
    std::map<std::string, Profile> result;
    const auto addProduct = [&result](
        const std::string &product,
        const std::map<Mode, Profile> &profiles)
    {
        auto selected = profiles.find(Mode::BestValidated);
        if (selected == profiles.end() || !selected->second.validated)
            selected = profiles.find(Mode::BestPerformance);
        if (selected == profiles.end() || !selected->second.validated)
            return;
        result.try_emplace(product, selected->second);
    };

    for (const auto &[product, profiles] : catalog.profiles)
        addProduct(product, profiles);
    for (const auto &[device, products] : catalog.device_profiles)
    {
        (void)device;
        for (const auto &[product, profiles] : products)
            addProduct(product, profiles);
    }
    return result;
}

std::map<std::string, std::string> settingsForOptionPrefix(
    const std::map<std::string, std::string> &settings,
    const std::string &core_option_prefix)
{
    std::map<std::string, std::string> translated;
    for (const auto &[setting, value] : settings)
    {
        if (setting.rfind("reicast_", 0) == 0)
            translated[core_option_prefix + setting.substr(8)] = value;
        else
            translated[setting] = value;
    }
    return translated;
}

} // namespace rr::flycast_profiles
