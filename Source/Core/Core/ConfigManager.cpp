// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/ConfigManager.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include <fmt/format.h>

#include "AudioCommon/AudioCommon.h"

#include "Common/Assert.h"
#include "Common/CDUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/Config/DefaultLocale.h"
#include "Core/Config/FreeLookSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/Host.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/TitleDatabase.h"
#include "VideoCommon/HiresTextures.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWad.h"

SConfig* SConfig::m_Instance;

SConfig::SConfig()
{
  LoadDefaults();
  // Make sure we have log manager
  LoadSettings();
}

void SConfig::Init()
{
  m_Instance = new SConfig;
}

void SConfig::Shutdown()
{
  delete m_Instance;
  m_Instance = nullptr;
}

SConfig::~SConfig()
{
  SaveSettings();
}

void SConfig::SaveSettings()
{
  NOTICE_LOG_FMT(BOOT, "Saving settings to {}", File::GetUserPath(F_DOLPHINCONFIG_IDX));
  Config::Save();
}

void SConfig::LoadSettings()
{
  INFO_LOG_FMT(BOOT, "Loading Settings from {}", File::GetUserPath(F_DOLPHINCONFIG_IDX));
  Config::Load();
}

void SConfig::ResetSettings(ResetSettingsEnum settings_to_reset)
{
  Config::ConfigChangeCallbackGuard guard;

  // collect all existing keys, then remove those that we don't want to reset
  std::set<Config::Location> keys_to_delete = Config::GetLocations(Config::LayerType::Base);

  // always keep analytics data
  keys_to_delete.erase(Config::MAIN_ANALYTICS_ID.GetLocation());
  keys_to_delete.erase(Config::MAIN_ANALYTICS_ENABLED.GetLocation());
  keys_to_delete.erase(Config::MAIN_ANALYTICS_PERMISSION_ASKED.GetLocation());

  if ((settings_to_reset & ResetSettingsEnum::Core) == ResetSettingsEnum::None)
  {
    keys_to_delete.erase(Config::MAIN_SKIP_IPL.GetLocation());
    keys_to_delete.erase(Config::MAIN_CPU_CORE.GetLocation());
    keys_to_delete.erase(Config::MAIN_JIT_FOLLOW_BRANCH.GetLocation());
    keys_to_delete.erase(Config::MAIN_FASTMEM.GetLocation());
    keys_to_delete.erase(Config::MAIN_DSP_HLE.GetLocation());
    keys_to_delete.erase(Config::MAIN_TIMING_VARIANCE.GetLocation());
    keys_to_delete.erase(Config::MAIN_CPU_THREAD.GetLocation());
    keys_to_delete.erase(Config::MAIN_SYNC_ON_SKIP_IDLE.GetLocation());
    keys_to_delete.erase(Config::MAIN_ENABLE_CHEATS.GetLocation());
    keys_to_delete.erase(Config::MAIN_GC_LANGUAGE.GetLocation());
    keys_to_delete.erase(Config::MAIN_OVERRIDE_REGION_SETTINGS.GetLocation());
    keys_to_delete.erase(Config::MAIN_DPL2_DECODER.GetLocation());
    keys_to_delete.erase(Config::MAIN_DPL2_QUALITY.GetLocation());
    keys_to_delete.erase(Config::MAIN_AUDIO_LATENCY.GetLocation());
    keys_to_delete.erase(Config::MAIN_AUDIO_STRETCH.GetLocation());
    keys_to_delete.erase(Config::MAIN_AUDIO_STRETCH_LATENCY.GetLocation());
    keys_to_delete.erase(Config::MAIN_MEMORY_CARD_SIZE.GetLocation());
    keys_to_delete.erase(Config::MAIN_SLOT_A.GetLocation());
    keys_to_delete.erase(Config::MAIN_SLOT_B.GetLocation());
    keys_to_delete.erase(Config::MAIN_SERIAL_PORT_1.GetLocation());
    keys_to_delete.erase(Config::MAIN_BBA_MAC.GetLocation());
    keys_to_delete.erase(Config::MAIN_BBA_XLINK_IP.GetLocation());
    keys_to_delete.erase(Config::MAIN_BBA_XLINK_CHAT_OSD.GetLocation());
    keys_to_delete.erase(Config::MAIN_WII_SD_CARD.GetLocation());
    keys_to_delete.erase(Config::MAIN_WII_KEYBOARD.GetLocation());
    keys_to_delete.erase(Config::MAIN_MMU.GetLocation());
    keys_to_delete.erase(Config::MAIN_BB_DUMP_PORT.GetLocation());
    keys_to_delete.erase(Config::MAIN_SYNC_GPU.GetLocation());
    keys_to_delete.erase(Config::MAIN_SYNC_GPU_MAX_DISTANCE.GetLocation());
    keys_to_delete.erase(Config::MAIN_SYNC_GPU_MIN_DISTANCE.GetLocation());
    keys_to_delete.erase(Config::MAIN_SYNC_GPU_OVERCLOCK.GetLocation());
    keys_to_delete.erase(Config::MAIN_FAST_DISC_SPEED.GetLocation());
    keys_to_delete.erase(Config::MAIN_LOW_DCBZ_HACK.GetLocation());
    keys_to_delete.erase(Config::MAIN_FLOAT_EXCEPTIONS.GetLocation());
    keys_to_delete.erase(Config::MAIN_DIVIDE_BY_ZERO_EXCEPTIONS.GetLocation());
    keys_to_delete.erase(Config::MAIN_FPRF.GetLocation());
    keys_to_delete.erase(Config::MAIN_ACCURATE_NANS.GetLocation());
    keys_to_delete.erase(Config::MAIN_DISABLE_ICACHE.GetLocation());
    keys_to_delete.erase(Config::MAIN_EMULATION_SPEED.GetLocation());
    keys_to_delete.erase(Config::MAIN_OVERCLOCK.GetLocation());
    keys_to_delete.erase(Config::MAIN_OVERCLOCK_ENABLE.GetLocation());
    keys_to_delete.erase(Config::MAIN_RAM_OVERRIDE_ENABLE.GetLocation());
    keys_to_delete.erase(Config::MAIN_MEM1_SIZE.GetLocation());
    keys_to_delete.erase(Config::MAIN_MEM2_SIZE.GetLocation());
    keys_to_delete.erase(Config::MAIN_GFX_BACKEND.GetLocation());
    keys_to_delete.erase(Config::MAIN_GPU_DETERMINISM_MODE.GetLocation());
    keys_to_delete.erase(Config::MAIN_OVERRIDE_BOOT_IOS.GetLocation());
    keys_to_delete.erase(Config::MAIN_CUSTOM_RTC_ENABLE.GetLocation());
    keys_to_delete.erase(Config::MAIN_CUSTOM_RTC_VALUE.GetLocation());
    keys_to_delete.erase(Config::MAIN_AUTO_DISC_CHANGE.GetLocation());
    keys_to_delete.erase(Config::MAIN_ALLOW_SD_WRITES.GetLocation());
    keys_to_delete.erase(Config::MAIN_ENABLE_SAVESTATES.GetLocation());
    keys_to_delete.erase(Config::MAIN_FALLBACK_REGION.GetLocation());
    keys_to_delete.erase(Config::MAIN_REAL_WII_REMOTE_REPEAT_REPORTS.GetLocation());
    keys_to_delete.erase(Config::MAIN_DSP_THREAD.GetLocation());
    keys_to_delete.erase(Config::MAIN_DSP_CAPTURE_LOG.GetLocation());
    keys_to_delete.erase(Config::MAIN_DSP_JIT.GetLocation());
    keys_to_delete.erase(Config::MAIN_DUMP_AUDIO.GetLocation());
    keys_to_delete.erase(Config::MAIN_DUMP_AUDIO_SILENT.GetLocation());
    keys_to_delete.erase(Config::MAIN_DUMP_UCODE.GetLocation());
    keys_to_delete.erase(Config::MAIN_AUDIO_BACKEND.GetLocation());
    keys_to_delete.erase(Config::MAIN_AUDIO_VOLUME.GetLocation());
    keys_to_delete.erase(Config::MAIN_AUDIO_MUTED.GetLocation());
#ifdef _WIN32
    keys_to_delete.erase(Config::MAIN_WASAPI_DEVICE.GetLocation());
#endif
    keys_to_delete.erase(Config::MAIN_SHOW_LAG.GetLocation());
    keys_to_delete.erase(Config::MAIN_SHOW_FRAME_COUNT.GetLocation());
    keys_to_delete.erase(Config::MAIN_WIRELESS_MAC.GetLocation());
    keys_to_delete.erase(Config::MAIN_GDB_SOCKET.GetLocation());
    keys_to_delete.erase(Config::MAIN_GDB_PORT.GetLocation());
#ifdef HAS_LIBMGBA
    keys_to_delete.erase(Config::MAIN_GBA_THREADS.GetLocation());
#endif
    keys_to_delete.erase(Config::MAIN_NETWORK_SSL_DUMP_READ.GetLocation());
    keys_to_delete.erase(Config::MAIN_NETWORK_SSL_DUMP_WRITE.GetLocation());
    keys_to_delete.erase(Config::MAIN_NETWORK_SSL_VERIFY_CERTIFICATES.GetLocation());
    keys_to_delete.erase(Config::MAIN_NETWORK_SSL_DUMP_ROOT_CA.GetLocation());
    keys_to_delete.erase(Config::MAIN_NETWORK_SSL_DUMP_PEER_CERT.GetLocation());
    keys_to_delete.erase(Config::MAIN_NETWORK_DUMP_AS_PCAP.GetLocation());
    keys_to_delete.erase(Config::MAIN_NETWORK_TIMEOUT.GetLocation());
    keys_to_delete.erase(Config::MAIN_USE_PANIC_HANDLERS.GetLocation());
    keys_to_delete.erase(Config::MAIN_ABORT_ON_PANIC_ALERT.GetLocation());
    keys_to_delete.erase(Config::MAIN_OSD_MESSAGES.GetLocation());
    keys_to_delete.erase(Config::MAIN_SKIP_NKIT_WARNING.GetLocation());
    keys_to_delete.erase(Config::MAIN_CONFIRM_ON_STOP.GetLocation());
    keys_to_delete.erase(Config::MAIN_SHOW_CURSOR.GetLocation());
    keys_to_delete.erase(Config::MAIN_LOCK_CURSOR.GetLocation());
    keys_to_delete.erase(Config::MAIN_FIFOPLAYER_LOOP_REPLAY.GetLocation());
    keys_to_delete.erase(Config::MAIN_FIFOPLAYER_EARLY_MEMORY_UPDATES.GetLocation());
    keys_to_delete.erase(Config::MAIN_AUTOUPDATE_UPDATE_TRACK.GetLocation());
    keys_to_delete.erase(Config::MAIN_AUTOUPDATE_HASH_OVERRIDE.GetLocation());
    keys_to_delete.erase(Config::MAIN_MOVIE_PAUSE_MOVIE.GetLocation());
    keys_to_delete.erase(Config::MAIN_MOVIE_MOVIE_AUTHOR.GetLocation());
    keys_to_delete.erase(Config::MAIN_MOVIE_DUMP_FRAMES.GetLocation());
    keys_to_delete.erase(Config::MAIN_MOVIE_DUMP_FRAMES_SILENT.GetLocation());
    keys_to_delete.erase(Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY.GetLocation());
    keys_to_delete.erase(Config::MAIN_MOVIE_SHOW_RTC.GetLocation());
    keys_to_delete.erase(Config::MAIN_MOVIE_SHOW_RERECORD.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_LOAD_STORE_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_LOAD_STORE_LXZ_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_LOAD_STORE_LWZ_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_LOAD_STORE_LBZX_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_LOAD_STORE_FLOATING_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_LOAD_STORE_PAIRED_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_FLOATING_POINT_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_INTEGER_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_PAIRED_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_SYSTEM_REGISTERS_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_BRANCH_OFF.GetLocation());
    keys_to_delete.erase(Config::MAIN_DEBUG_JIT_REGISTER_CACHE_OFF.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_SCREENSAVER.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_LANGUAGE.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_COUNTRY.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_WIDESCREEN.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_PROGRESSIVE_SCAN.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_PAL60.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_SOUND_MODE.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_SENSOR_BAR_POSITION.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_SENSOR_BAR_SENSITIVITY.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_SPEAKER_VOLUME.GetLocation());
    keys_to_delete.erase(Config::SYSCONF_WIIMOTE_MOTOR.GetLocation());
    keys_to_delete.erase(Config::FREE_LOOK_ENABLED.GetLocation());
  }

  if ((settings_to_reset & ResetSettingsEnum::Graphics) == ResetSettingsEnum::None)
  {
    keys_to_delete.erase(Config::MAIN_FULLSCREEN_DISPLAY_RES.GetLocation());
    keys_to_delete.erase(Config::MAIN_FULLSCREEN.GetLocation());
    keys_to_delete.erase(Config::MAIN_RENDER_TO_MAIN.GetLocation());
    keys_to_delete.erase(Config::MAIN_RENDER_WINDOW_XPOS.GetLocation());
    keys_to_delete.erase(Config::MAIN_RENDER_WINDOW_YPOS.GetLocation());
    keys_to_delete.erase(Config::MAIN_RENDER_WINDOW_WIDTH.GetLocation());
    keys_to_delete.erase(Config::MAIN_RENDER_WINDOW_HEIGHT.GetLocation());
    keys_to_delete.erase(Config::MAIN_RENDER_WINDOW_AUTOSIZE.GetLocation());
    keys_to_delete.erase(Config::GFX_VSYNC.GetLocation());
    keys_to_delete.erase(Config::GFX_ADAPTER.GetLocation());
    keys_to_delete.erase(Config::GFX_WIDESCREEN_HACK.GetLocation());
    keys_to_delete.erase(Config::GFX_ASPECT_RATIO.GetLocation());
    keys_to_delete.erase(Config::GFX_SUGGESTED_ASPECT_RATIO.GetLocation());
    keys_to_delete.erase(Config::GFX_CROP.GetLocation());
    keys_to_delete.erase(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES.GetLocation());
    keys_to_delete.erase(Config::GFX_SHOW_FPS.GetLocation());
    keys_to_delete.erase(Config::GFX_SHOW_NETPLAY_PING.GetLocation());
    keys_to_delete.erase(Config::GFX_SHOW_NETPLAY_MESSAGES.GetLocation());
    keys_to_delete.erase(Config::GFX_LOG_RENDER_TIME_TO_FILE.GetLocation());
    keys_to_delete.erase(Config::GFX_OVERLAY_STATS.GetLocation());
    keys_to_delete.erase(Config::GFX_OVERLAY_PROJ_STATS.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_TEXTURES.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_MIP_TEXTURES.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_BASE_TEXTURES.GetLocation());
    keys_to_delete.erase(Config::GFX_HIRES_TEXTURES.GetLocation());
    keys_to_delete.erase(Config::GFX_CACHE_HIRES_TEXTURES.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_EFB_TARGET.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_XFB_TARGET.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_FRAMES_AS_IMAGES.GetLocation());
    keys_to_delete.erase(Config::GFX_USE_FFV1.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_FORMAT.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_CODEC.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_ENCODER.GetLocation());
    keys_to_delete.erase(Config::GFX_DUMP_PATH.GetLocation());
    keys_to_delete.erase(Config::GFX_BITRATE_KBPS.GetLocation());
    keys_to_delete.erase(Config::GFX_INTERNAL_RESOLUTION_FRAME_DUMPS.GetLocation());
    keys_to_delete.erase(Config::GFX_PNG_COMPRESSION_LEVEL.GetLocation());
    keys_to_delete.erase(Config::GFX_ENABLE_GPU_TEXTURE_DECODING.GetLocation());
    keys_to_delete.erase(Config::GFX_ENABLE_PIXEL_LIGHTING.GetLocation());
    keys_to_delete.erase(Config::GFX_FAST_DEPTH_CALC.GetLocation());
    keys_to_delete.erase(Config::GFX_MSAA.GetLocation());
    keys_to_delete.erase(Config::GFX_SSAA.GetLocation());
    keys_to_delete.erase(Config::GFX_EFB_SCALE.GetLocation());
    keys_to_delete.erase(Config::GFX_MAX_EFB_SCALE.GetLocation());
    keys_to_delete.erase(Config::GFX_TEXFMT_OVERLAY_ENABLE.GetLocation());
    keys_to_delete.erase(Config::GFX_TEXFMT_OVERLAY_CENTER.GetLocation());
    keys_to_delete.erase(Config::GFX_ENABLE_WIREFRAME.GetLocation());
    keys_to_delete.erase(Config::GFX_DISABLE_FOG.GetLocation());
    keys_to_delete.erase(Config::GFX_BORDERLESS_FULLSCREEN.GetLocation());
    keys_to_delete.erase(Config::GFX_ENABLE_VALIDATION_LAYER.GetLocation());
    keys_to_delete.erase(Config::GFX_BACKEND_MULTITHREADING.GetLocation());
    keys_to_delete.erase(Config::GFX_COMMAND_BUFFER_EXECUTE_INTERVAL.GetLocation());
    keys_to_delete.erase(Config::GFX_SHADER_CACHE.GetLocation());
    keys_to_delete.erase(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING.GetLocation());
    keys_to_delete.erase(Config::GFX_SHADER_COMPILATION_MODE.GetLocation());
    keys_to_delete.erase(Config::GFX_SHADER_COMPILER_THREADS.GetLocation());
    keys_to_delete.erase(Config::GFX_SHADER_PRECOMPILER_THREADS.GetLocation());
    keys_to_delete.erase(Config::GFX_SAVE_TEXTURE_CACHE_TO_STATE.GetLocation());
    keys_to_delete.erase(Config::GFX_SW_ZCOMPLOC.GetLocation());
    keys_to_delete.erase(Config::GFX_SW_ZFREEZE.GetLocation());
    keys_to_delete.erase(Config::GFX_SW_DUMP_OBJECTS.GetLocation());
    keys_to_delete.erase(Config::GFX_SW_DUMP_TEV_STAGES.GetLocation());
    keys_to_delete.erase(Config::GFX_SW_DUMP_TEV_TEX_FETCHES.GetLocation());
    keys_to_delete.erase(Config::GFX_SW_DRAW_START.GetLocation());
    keys_to_delete.erase(Config::GFX_SW_DRAW_END.GetLocation());
    keys_to_delete.erase(Config::GFX_PREFER_GLES.GetLocation());
    keys_to_delete.erase(Config::GFX_ENHANCE_FORCE_FILTERING.GetLocation());
    keys_to_delete.erase(Config::GFX_ENHANCE_MAX_ANISOTROPY.GetLocation());
    keys_to_delete.erase(Config::GFX_ENHANCE_POST_SHADER.GetLocation());
    keys_to_delete.erase(Config::GFX_ENHANCE_FORCE_TRUE_COLOR.GetLocation());
    keys_to_delete.erase(Config::GFX_ENHANCE_DISABLE_COPY_FILTER.GetLocation());
    keys_to_delete.erase(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION.GetLocation());
    keys_to_delete.erase(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION_THRESHOLD.GetLocation());
    keys_to_delete.erase(Config::GFX_STEREO_MODE.GetLocation());
    keys_to_delete.erase(Config::GFX_STEREO_DEPTH.GetLocation());
    keys_to_delete.erase(Config::GFX_STEREO_CONVERGENCE_PERCENTAGE.GetLocation());
    keys_to_delete.erase(Config::GFX_STEREO_SWAP_EYES.GetLocation());
    keys_to_delete.erase(Config::GFX_STEREO_CONVERGENCE.GetLocation());
    keys_to_delete.erase(Config::GFX_STEREO_EFB_MONO_DEPTH.GetLocation());
    keys_to_delete.erase(Config::GFX_STEREO_DEPTH_PERCENTAGE.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_EFB_ACCESS_ENABLE.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_EFB_DEFER_INVALIDATION.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_EFB_ACCESS_TILE_SIZE.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_BBOX_ENABLE.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_FORCE_PROGRESSIVE.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_DISABLE_COPY_TO_VRAM.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_DEFER_EFB_COPIES.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_IMMEDIATE_XFB.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_SKIP_DUPLICATE_XFBS.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_EARLY_XFB_OUTPUT.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_COPY_EFB_SCALED.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_VERTEX_ROUDING.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_MISSING_COLOR_VALUE.GetLocation());
    keys_to_delete.erase(Config::GFX_HACK_FAST_TEXTURE_SAMPLING.GetLocation());
    keys_to_delete.erase(Config::GFX_PERF_QUERIES_ENABLE.GetLocation());
  }

  if ((settings_to_reset & ResetSettingsEnum::Paths) == ResetSettingsEnum::None)
  {
    keys_to_delete.erase(Config::MAIN_DEFAULT_ISO.GetLocation());
    keys_to_delete.erase(Config::MAIN_MEMCARD_A_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_MEMCARD_B_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_AGP_CART_A_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_AGP_CART_B_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_GCI_FOLDER_A_PATH_OVERRIDE.GetLocation());
    keys_to_delete.erase(Config::MAIN_GCI_FOLDER_B_PATH_OVERRIDE.GetLocation());
    keys_to_delete.erase(Config::MAIN_PERF_MAP_DIR.GetLocation());
    keys_to_delete.erase(Config::MAIN_DUMP_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_LOAD_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_RESOURCEPACK_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_FS_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_SD_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_WFS_PATH.GetLocation());

    keys_to_delete.erase(Config::MAIN_ISO_PATH_COUNT.GetLocation());
    const size_t iso_path_count =
        MathUtil::SaturatingCast<size_t>(Config::Get(Config::MAIN_ISO_PATH_COUNT));
    for (int i = 0; i < iso_path_count; ++i)
      keys_to_delete.erase(Config::MakeISOPathConfigInfo(i).GetLocation());
    keys_to_delete.erase(Config::MAIN_RECURSIVE_ISO_PATHS.GetLocation());

#ifdef HAS_LIBMGBA
    keys_to_delete.erase(Config::MAIN_GBA_BIOS_PATH.GetLocation());
    for (const auto& gba_rom_path : Config::MAIN_GBA_ROM_PATHS)
      keys_to_delete.erase(gba_rom_path.GetLocation());
    keys_to_delete.erase(Config::MAIN_GBA_SAVES_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_GBA_SAVES_IN_ROM_PATH.GetLocation());
#endif
  }

  if ((settings_to_reset & ResetSettingsEnum::GUI) == ResetSettingsEnum::None)
  {
    keys_to_delete.erase(Config::MAIN_KEEP_WINDOW_ON_TOP.GetLocation());
    keys_to_delete.erase(Config::MAIN_DISABLE_SCREENSAVER.GetLocation());
    keys_to_delete.erase(Config::MAIN_USE_HIGH_CONTRAST_TOOLTIPS.GetLocation());
    keys_to_delete.erase(Config::MAIN_INTERFACE_LANGUAGE.GetLocation());
    keys_to_delete.erase(Config::MAIN_EXTENDED_FPS_INFO.GetLocation());
    keys_to_delete.erase(Config::MAIN_SHOW_ACTIVE_TITLE.GetLocation());
    keys_to_delete.erase(Config::MAIN_USE_BUILT_IN_TITLE_DATABASE.GetLocation());
    keys_to_delete.erase(Config::MAIN_THEME_NAME.GetLocation());
    keys_to_delete.erase(Config::MAIN_PAUSE_ON_FOCUS_LOST.GetLocation());
    keys_to_delete.erase(Config::MAIN_ENABLE_DEBUGGING.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_DRIVES.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_WAD.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_ELF_DOL.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_WII.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_GC.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_JPN.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_PAL.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_USA.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_AUSTRALIA.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_FRANCE.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_GERMANY.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_ITALY.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_KOREA.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_NETHERLANDS.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_RUSSIA.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_SPAIN.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_TAIWAN.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_WORLD.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_UNKNOWN.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_SORT.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_LIST_SORT_SECONDARY.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_PLATFORM.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_DESCRIPTION.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_BANNER.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_TITLE.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_MAKER.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_FILE_NAME.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_FILE_PATH.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_GAME_ID.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_REGION.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_FILE_SIZE.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_FILE_FORMAT.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_BLOCK_SIZE.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_COMPRESSION.GetLocation());
    keys_to_delete.erase(Config::MAIN_GAMELIST_COLUMN_TAGS.GetLocation());
    keys_to_delete.erase(Config::MAIN_USE_DISCORD_PRESENCE.GetLocation());
    keys_to_delete.erase(Config::MAIN_USE_GAME_COVERS.GetLocation());
  }

  if ((settings_to_reset & ResetSettingsEnum::Controller) == ResetSettingsEnum::None)
  {
    keys_to_delete.erase(Config::GetInfoForSIDevice(0).GetLocation());
    keys_to_delete.erase(Config::GetInfoForSIDevice(1).GetLocation());
    keys_to_delete.erase(Config::GetInfoForSIDevice(2).GetLocation());
    keys_to_delete.erase(Config::GetInfoForSIDevice(3).GetLocation());
    keys_to_delete.erase(Config::GetInfoForAdapterRumble(0).GetLocation());
    keys_to_delete.erase(Config::GetInfoForAdapterRumble(1).GetLocation());
    keys_to_delete.erase(Config::GetInfoForAdapterRumble(2).GetLocation());
    keys_to_delete.erase(Config::GetInfoForAdapterRumble(3).GetLocation());
    keys_to_delete.erase(Config::GetInfoForSimulateKonga(0).GetLocation());
    keys_to_delete.erase(Config::GetInfoForSimulateKonga(1).GetLocation());
    keys_to_delete.erase(Config::GetInfoForSimulateKonga(2).GetLocation());
    keys_to_delete.erase(Config::GetInfoForSimulateKonga(3).GetLocation());
    keys_to_delete.erase(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING.GetLocation());
    keys_to_delete.erase(Config::MAIN_WIIMOTE_ENABLE_SPEAKER.GetLocation());
    keys_to_delete.erase(Config::MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE.GetLocation());
    keys_to_delete.erase(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED.GetLocation());
    keys_to_delete.erase(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID.GetLocation());
    keys_to_delete.erase(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID.GetLocation());
    keys_to_delete.erase(Config::MAIN_BLUETOOTH_PASSTHROUGH_LINK_KEYS.GetLocation());
    keys_to_delete.erase(Config::MAIN_USB_PASSTHROUGH_DEVICES.GetLocation());
    keys_to_delete.erase(Config::MAIN_INPUT_BACKGROUND_INPUT.GetLocation());
    keys_to_delete.erase(Config::MAIN_FOCUSED_HOTKEYS.GetLocation());
    keys_to_delete.erase(Config::WIIMOTE_1_SOURCE.GetLocation());
    keys_to_delete.erase(Config::WIIMOTE_2_SOURCE.GetLocation());
    keys_to_delete.erase(Config::WIIMOTE_3_SOURCE.GetLocation());
    keys_to_delete.erase(Config::WIIMOTE_4_SOURCE.GetLocation());
    keys_to_delete.erase(Config::WIIMOTE_BB_SOURCE.GetLocation());
    keys_to_delete.erase(Config::FL1_CONTROL_TYPE.GetLocation());
  }

  if ((settings_to_reset & ResetSettingsEnum::Netplay) == ResetSettingsEnum::None)
  {
    keys_to_delete.erase(Config::NETPLAY_TRAVERSAL_SERVER.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_TRAVERSAL_PORT.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_TRAVERSAL_CHOICE.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_HOST_CODE.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_INDEX_URL.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_HOST_PORT.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_ADDRESS.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_CONNECT_PORT.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_LISTEN_PORT.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_NICKNAME.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_USE_UPNP.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_ENABLE_QOS.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_USE_INDEX.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_INDEX_REGION.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_INDEX_NAME.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_INDEX_PASSWORD.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_BUFFER_SIZE.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_CLIENT_BUFFER_SIZE.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_WRITE_SAVE_DATA.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_LOAD_WII_SAVE.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_SYNC_SAVES.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_SYNC_CODES.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_RECORD_INPUTS.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_STRICT_SETTINGS_SYNC.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_NETWORK_MODE.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_SYNC_ALL_WII_SAVES.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_GOLF_MODE_OVERLAY.GetLocation());
    keys_to_delete.erase(Config::NETPLAY_HIDE_REMOTE_GBAS.GetLocation());
  }

  Config::DeleteLocations(Config::LayerType::Base, keys_to_delete);
}

void SConfig::ResetRunningGameMetadata()
{
  SetRunningGameMetadata("00000000", "", 0, 0, DiscIO::Region::Unknown);
}

void SConfig::SetRunningGameMetadata(const DiscIO::Volume& volume,
                                     const DiscIO::Partition& partition)
{
  if (partition == volume.GetGamePartition())
  {
    SetRunningGameMetadata(volume.GetGameID(), volume.GetGameTDBID(),
                           volume.GetTitleID().value_or(0), volume.GetRevision().value_or(0),
                           volume.GetRegion());
  }
  else
  {
    SetRunningGameMetadata(volume.GetGameID(partition), volume.GetGameTDBID(),
                           volume.GetTitleID(partition).value_or(0),
                           volume.GetRevision(partition).value_or(0), volume.GetRegion());
  }
}

void SConfig::SetRunningGameMetadata(const IOS::ES::TMDReader& tmd, DiscIO::Platform platform)
{
  const u64 tmd_title_id = tmd.GetTitleId();

  // If we're launching a disc game, we want to read the revision from
  // the disc header instead of the TMD. They can differ.
  // (IOS HLE ES calls us with a TMDReader rather than a volume when launching
  // a disc game, because ES has no reason to be accessing the disc directly.)
  if (platform == DiscIO::Platform::WiiWAD ||
      !DVDInterface::UpdateRunningGameMetadata(tmd_title_id))
  {
    // If not launching a disc game, just read everything from the TMD.
    SetRunningGameMetadata(tmd.GetGameID(), tmd.GetGameTDBID(), tmd_title_id, tmd.GetTitleVersion(),
                           tmd.GetRegion());
  }
}

void SConfig::SetRunningGameMetadata(const std::string& game_id)
{
  SetRunningGameMetadata(game_id, "", 0, 0, DiscIO::Region::Unknown);
}

void SConfig::SetRunningGameMetadata(const std::string& game_id, const std::string& gametdb_id,
                                     u64 title_id, u16 revision, DiscIO::Region region)
{
  const bool was_changed = m_game_id != game_id || m_gametdb_id != gametdb_id ||
                           m_title_id != title_id || m_revision != revision;
  m_game_id = game_id;
  m_gametdb_id = gametdb_id;
  m_title_id = title_id;
  m_revision = revision;

  if (game_id.length() == 6)
  {
    m_debugger_game_id = game_id;
  }
  else if (title_id != 0)
  {
    m_debugger_game_id =
        fmt::format("{:08X}_{:08X}", static_cast<u32>(title_id >> 32), static_cast<u32>(title_id));
  }
  else
  {
    m_debugger_game_id.clear();
  }

  if (!was_changed)
    return;

  if (game_id == "00000000")
  {
    m_title_name.clear();
    m_title_description.clear();
    return;
  }

  const Core::TitleDatabase title_database;
  const DiscIO::Language language = GetLanguageAdjustedForRegion(bWii, region);
  m_title_name = title_database.GetTitleName(m_gametdb_id, language);
  m_title_description = title_database.Describe(m_gametdb_id, language);
  NOTICE_LOG_FMT(CORE, "Active title: {}", m_title_description);
  Host_TitleChanged();

  Config::AddLayer(ConfigLoaders::GenerateGlobalGameConfigLoader(game_id, revision));
  Config::AddLayer(ConfigLoaders::GenerateLocalGameConfigLoader(game_id, revision));

  if (Core::IsRunning())
    DolphinAnalytics::Instance().ReportGameStart();
}

void SConfig::OnNewTitleLoad()
{
  if (!Core::IsRunning())
    return;

  if (!g_symbolDB.IsEmpty())
  {
    g_symbolDB.Clear();
    Host_NotifyMapLoaded();
  }
  CBoot::LoadMapFromFilename();
  HLE::Reload();
  PatchEngine::Reload();
  HiresTexture::Update();
}

void SConfig::LoadDefaults()
{
  bAutomaticStart = false;
  bBootToPause = false;

  bWii = false;

  ResetRunningGameMetadata();
}

// Static method to make a simple game ID for elf/dol files
std::string SConfig::MakeGameID(std::string_view file_name)
{
  size_t lastdot = file_name.find_last_of(".");
  if (lastdot == std::string::npos)
    return "ID-" + std::string(file_name);
  return "ID-" + std::string(file_name.substr(0, lastdot));
}

// The reason we need this function is because some memory card code
// expects to get a non-NTSC-K region even if we're emulating an NTSC-K Wii.
DiscIO::Region SConfig::ToGameCubeRegion(DiscIO::Region region)
{
  if (region != DiscIO::Region::NTSC_K)
    return region;

  // GameCube has no NTSC-K region. No choice of replacement value is completely
  // non-arbitrary, but let's go with NTSC-J since Korean GameCubes are NTSC-J.
  return DiscIO::Region::NTSC_J;
}

const char* SConfig::GetDirectoryForRegion(DiscIO::Region region)
{
  if (region == DiscIO::Region::Unknown)
    region = ToGameCubeRegion(GetFallbackRegion());

  switch (region)
  {
  case DiscIO::Region::NTSC_J:
    return JAP_DIR;

  case DiscIO::Region::NTSC_U:
    return USA_DIR;

  case DiscIO::Region::PAL:
    return EUR_DIR;

  case DiscIO::Region::NTSC_K:
    ASSERT_MSG(BOOT, false, "NTSC-K is not a valid GameCube region");
    return JAP_DIR;  // See ToGameCubeRegion

  default:
    ASSERT_MSG(BOOT, false, "Default case should not be reached");
    return EUR_DIR;
  }
}

std::string SConfig::GetBootROMPath(const std::string& region_directory) const
{
  const std::string path =
      File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + region_directory + DIR_SEP GC_IPL;
  if (!File::Exists(path))
    return File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + region_directory + DIR_SEP GC_IPL;
  return path;
}

struct SetGameMetadata
{
  SetGameMetadata(SConfig* config_, DiscIO::Region* region_) : config(config_), region(region_) {}
  bool operator()(const BootParameters::Disc& disc) const
  {
    *region = disc.volume->GetRegion();
    config->bWii = disc.volume->GetVolumeType() == DiscIO::Platform::WiiDisc;
    config->m_disc_booted_from_game_list = true;
    config->SetRunningGameMetadata(*disc.volume, disc.volume->GetGamePartition());
    return true;
  }

  bool operator()(const BootParameters::Executable& executable) const
  {
    if (!executable.reader->IsValid())
      return false;

    *region = DiscIO::Region::Unknown;
    config->bWii = executable.reader->IsWii();

    // Strip the .elf/.dol file extension and directories before the name
    SplitPath(executable.path, nullptr, &config->m_debugger_game_id, nullptr);

    // Set DOL/ELF game ID appropriately
    std::string executable_path = executable.path;
    constexpr char BACKSLASH = '\\';
    constexpr char FORWARDSLASH = '/';
    std::replace(executable_path.begin(), executable_path.end(), BACKSLASH, FORWARDSLASH);
    config->SetRunningGameMetadata(SConfig::MakeGameID(PathToFileName(executable_path)));

    Host_TitleChanged();

    return true;
  }

  bool operator()(const DiscIO::VolumeWAD& wad) const
  {
    if (!wad.GetTMD().IsValid())
    {
      PanicAlertFmtT("This WAD is not valid.");
      return false;
    }
    if (!IOS::ES::IsChannel(wad.GetTMD().GetTitleId()))
    {
      PanicAlertFmtT("This WAD is not bootable.");
      return false;
    }

    const IOS::ES::TMDReader& tmd = wad.GetTMD();
    *region = tmd.GetRegion();
    config->bWii = true;
    config->SetRunningGameMetadata(tmd, DiscIO::Platform::WiiWAD);

    return true;
  }

  bool operator()(const BootParameters::NANDTitle& nand_title) const
  {
    IOS::HLE::Kernel ios;
    const IOS::ES::TMDReader tmd = ios.GetES()->FindInstalledTMD(nand_title.id);
    if (!tmd.IsValid() || !IOS::ES::IsChannel(nand_title.id))
    {
      PanicAlertFmtT("This title cannot be booted.");
      return false;
    }

    *region = tmd.GetRegion();
    config->bWii = true;
    config->SetRunningGameMetadata(tmd, DiscIO::Platform::WiiWAD);

    return true;
  }

  bool operator()(const BootParameters::IPL& ipl) const
  {
    *region = ipl.region;
    config->bWii = false;
    Host_TitleChanged();

    return true;
  }

  bool operator()(const BootParameters::DFF& dff) const
  {
    std::unique_ptr<FifoDataFile> dff_file(FifoDataFile::Load(dff.dff_path, true));
    if (!dff_file)
      return false;

    *region = DiscIO::Region::NTSC_U;
    config->bWii = dff_file->GetIsWii();
    Host_TitleChanged();

    return true;
  }

private:
  SConfig* config;
  DiscIO::Region* region;
};

bool SConfig::SetPathsAndGameMetadata(const BootParameters& boot)
{
  m_is_mios = false;
  m_disc_booted_from_game_list = false;
  if (!std::visit(SetGameMetadata(this, &m_region), boot.parameters))
    return false;

  if (m_region == DiscIO::Region::Unknown)
    m_region = GetFallbackRegion();

  // Set up paths
  const std::string region_dir = GetDirectoryForRegion(ToGameCubeRegion(m_region));
  m_strSRAM = File::GetUserPath(F_GCSRAM_IDX);
  m_strBootROM = GetBootROMPath(region_dir);

  return true;
}

DiscIO::Region SConfig::GetFallbackRegion()
{
  return Config::Get(Config::MAIN_FALLBACK_REGION);
}

DiscIO::Language SConfig::GetCurrentLanguage(bool wii) const
{
  DiscIO::Language language;
  if (wii)
    language = static_cast<DiscIO::Language>(Config::Get(Config::SYSCONF_LANGUAGE));
  else
    language = DiscIO::FromGameCubeLanguage(Config::Get(Config::MAIN_GC_LANGUAGE));

  // Get rid of invalid values (probably doesn't matter, but might as well do it)
  if (language > DiscIO::Language::Unknown || language < DiscIO::Language::Japanese)
    language = DiscIO::Language::Unknown;
  return language;
}

DiscIO::Language SConfig::GetLanguageAdjustedForRegion(bool wii, DiscIO::Region region) const
{
  const DiscIO::Language language = GetCurrentLanguage(wii);

  if (!wii && region == DiscIO::Region::NTSC_K)
    region = DiscIO::Region::NTSC_J;  // NTSC-K only exists on Wii, so use a fallback

  if (!wii && region == DiscIO::Region::NTSC_J && language == DiscIO::Language::English)
    return DiscIO::Language::Japanese;  // English and Japanese both use the value 0 in GC SRAM

  if (!Config::Get(Config::MAIN_OVERRIDE_REGION_SETTINGS))
  {
    if (region == DiscIO::Region::NTSC_J)
      return DiscIO::Language::Japanese;

    if (region == DiscIO::Region::NTSC_U && language != DiscIO::Language::English &&
        (!wii || (language != DiscIO::Language::French && language != DiscIO::Language::Spanish)))
    {
      return DiscIO::Language::English;
    }

    if (region == DiscIO::Region::PAL &&
        (language < DiscIO::Language::English || language > DiscIO::Language::Dutch))
    {
      return DiscIO::Language::English;
    }

    if (region == DiscIO::Region::NTSC_K)
      return DiscIO::Language::Korean;
  }

  return language;
}

IniFile SConfig::LoadDefaultGameIni() const
{
  return LoadDefaultGameIni(GetGameID(), m_revision);
}

IniFile SConfig::LoadLocalGameIni() const
{
  return LoadLocalGameIni(GetGameID(), m_revision);
}

IniFile SConfig::LoadGameIni() const
{
  return LoadGameIni(GetGameID(), m_revision);
}

IniFile SConfig::LoadDefaultGameIni(const std::string& id, std::optional<u16> revision)
{
  IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
  return game_ini;
}

IniFile SConfig::LoadLocalGameIni(const std::string& id, std::optional<u16> revision)
{
  IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
  return game_ini;
}

IniFile SConfig::LoadGameIni(const std::string& id, std::optional<u16> revision)
{
  IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
  return game_ini;
}
