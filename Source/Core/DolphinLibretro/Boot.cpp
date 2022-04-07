#include <cstdio>
#include <libretro.h>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/VideoInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinLibretro/Input.h"
#include "DolphinLibretro/Log.h"
#include "DolphinLibretro/Options.h"
#include "DolphinLibretro/Video.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "UICommon/DiscordPresence.h"
#include "UICommon/UICommon.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#ifdef _MSC_VER
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace Libretro
{
extern retro_environment_t environ_cb;

// Disk swapping
static void InitDiskControlInterface();
static std::string NormalizePath(const std::string& path);
static std::string DenormalizePath(const std::string& path);
static unsigned disk_index = 0;
static bool eject_state;
static std::vector<std::string> disk_paths;
}  // namespace Libretro

bool retro_load_game(const struct retro_game_info* game)
{
  const char* save_dir = NULL;
  const char* system_dir = NULL;
  const char* core_assets_dir = NULL;
  std::string user_dir;
  std::string sys_dir;

  Libretro::environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir);
  Libretro::environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir);
  Libretro::environ_cb(RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY, &core_assets_dir);
  Libretro::InitDiskControlInterface();

  if (save_dir && *save_dir)
    user_dir = std::string(save_dir) + DIR_SEP "User";
  else if (system_dir && *system_dir)
    user_dir = std::string(system_dir) + DIR_SEP "dolphin-emu" DIR_SEP "User";

  if (system_dir && *system_dir)
    sys_dir = std::string(system_dir) + DIR_SEP "dolphin-emu" DIR_SEP "Sys";
  else if (core_assets_dir && *core_assets_dir)
    sys_dir = std::string(core_assets_dir) + DIR_SEP "dolphin-emu" DIR_SEP "Sys";
  else if (save_dir && *save_dir)
    sys_dir = std::string(save_dir) + DIR_SEP "Sys";
  else
    sys_dir = "dolphin-emu" DIR_SEP "Sys";

  File::SetSysDirectory(sys_dir);
  UICommon::SetUserDirectory(user_dir);
  UICommon::CreateDirectories();
  UICommon::Init();
  Libretro::Log::Init();
  Discord::SetDiscordPresenceEnabled(false);
  Common::SetEnableAlert(false);

  INFO_LOG(COMMON, "User Directory set to '%s'", user_dir.c_str());
  INFO_LOG(COMMON, "System Directory set to '%s'", sys_dir.c_str());

  /* disable throttling emulation to match GetTargetRefreshRate() */
  Core::SetIsThrottlerTempDisabled(true);
  SConfig::GetInstance().m_EmulationSpeed = 0.0f;

#if defined(_DEBUG)
  SConfig::GetInstance().bFastmem = false;
#else
  SConfig::GetInstance().bFastmem = Libretro::Options::fastmem;
#endif
  SConfig::GetInstance().bDSPHLE = Libretro::Options::DSPHLE;
  SConfig::GetInstance().m_DSPEnableJIT = Libretro::Options::DSPEnableJIT;
  SConfig::GetInstance().cpu_core = Libretro::Options::cpu_core;
  SConfig::GetInstance().SelectedLanguage = (int)(DiscIO::Language)Libretro::Options::Language - 1;
  SConfig::GetInstance().bCPUThread = true;
  SConfig::GetInstance().bEMUThread = false;
  SConfig::GetInstance().bBootToPause = true;
  SConfig::GetInstance().m_OCFactor = Libretro::Options::cpuClockRate;
  SConfig::GetInstance().m_OCEnable = Libretro::Options::cpuClockRate != 1.0;
  SConfig::GetInstance().sBackend = BACKEND_NULLSOUND;
  SConfig::GetInstance().m_DumpAudio = false;
  SConfig::GetInstance().bDPL2Decoder = false;
  SConfig::GetInstance().iLatency = 0;
  SConfig::GetInstance().m_audio_stretch = false;
  SConfig::GetInstance().m_WiimoteContinuousScanning = Libretro::Options::WiimoteContinuousScanning;
  SConfig::GetInstance().bEnableCheats = Libretro::Options::cheatsEnabled;
  SConfig::GetInstance().bOnScreenDisplayMessages = Libretro::Options::osdEnabled;

  Config::SetBase(Config::SYSCONF_LANGUAGE, (u32)(DiscIO::Language)Libretro::Options::Language);
  Config::SetBase(Config::SYSCONF_WIDESCREEN, Libretro::Options::Widescreen);
  Config::SetBase(Config::SYSCONF_PROGRESSIVE_SCAN, Libretro::Options::progressiveScan);
  Config::SetBase(Config::SYSCONF_PAL60, Libretro::Options::pal60);
  Config::SetBase(Config::SYSCONF_SENSOR_BAR_POSITION, Libretro::Options::sensorBarPosition);
  Config::SetBase(Config::SYSCONF_WIIMOTE_MOTOR, Libretro::Options::enableRumble);

  Config::SetBase(Config::GFX_WIDESCREEN_HACK, Libretro::Options::WidescreenHack);
  Config::SetBase(Config::GFX_EFB_SCALE, Libretro::Options::efbScale);
  Config::SetBase(Config::GFX_ASPECT_RATIO, AspectMode::Stretch);
  Config::SetBase(Config::GFX_BACKEND_MULTITHREADING, false);
  Config::SetBase(Config::GFX_SHADER_COMPILATION_MODE, Libretro::Options::shaderCompilationMode);
  Config::SetBase(Config::GFX_ENHANCE_MAX_ANISOTROPY, Libretro::Options::maxAnisotropy);
  Config::SetBase(Config::GFX_HACK_SKIP_DUPLICATE_XFBS, Libretro::Options::skipDupeFrames);
  Config::SetBase(Config::GFX_HACK_COPY_EFB_SCALED, Libretro::Options::efbScaledCopy);
  Config::SetBase(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, Libretro::Options::efbToTexture);
  Config::SetBase(Config::GFX_HACK_DISABLE_COPY_TO_VRAM, Libretro::Options::efbToVram);
  Config::SetBase(Config::GFX_FAST_DEPTH_CALC, Libretro::Options::fastDepthCalc);
  Config::SetBase(Config::GFX_HACK_BBOX_ENABLE, Libretro::Options::bboxEnabled);
  Config::SetBase(Config::GFX_ENABLE_GPU_TEXTURE_DECODING, Libretro::Options::gpuTextureDecoding);
  Config::SetBase(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING, Libretro::Options::waitForShaders);
  Config::SetBase(Config::GFX_ENHANCE_FORCE_FILTERING, Libretro::Options::forceTextureFiltering);
  Config::SetBase(Config::GFX_HIRES_TEXTURES, Libretro::Options::loadCustomTextures);
  Config::SetBase(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, Libretro::Options::textureCacheAccuracy);
#if 0
  Config::SetBase(Config::GFX_SHADER_COMPILER_THREADS, 1);
  Config::SetBase(Config::GFX_SHADER_PRECOMPILER_THREADS, 1);
#endif

  Libretro::Video::Init();
  VideoBackendBase::PopulateBackendInfo();
  NOTICE_LOG(VIDEO, "Using GFX backend: %s", Config::Get(Config::MAIN_GFX_BACKEND).c_str());

  WindowSystemInfo wsi(WindowSystemType::Libretro, nullptr, nullptr, nullptr);

  std::vector<std::string> normalized_game_paths;
  normalized_game_paths.push_back(Libretro::NormalizePath(game->path));
  std::string folder_path;
  std::string extension;
  SplitPath(normalized_game_paths.front(), &folder_path, nullptr, &extension);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  if (extension == ".m3u" || extension == ".m3u8")
  {
    normalized_game_paths = ReadM3UFile(normalized_game_paths.front(), folder_path);
    if (normalized_game_paths.empty())
    {
      ERROR_LOG(BOOT, "Could not boot %s. M3U contains no paths\n", game->path);
      return false;
    }
  }

  for (auto& normalized_game_path : normalized_game_paths)
    Libretro::disk_paths.push_back(Libretro::DenormalizePath(normalized_game_path));

  if (!BootManager::BootCore(BootParameters::GenerateFromFile(normalized_game_paths), wsi))
  {
    ERROR_LOG(BOOT, "Could not boot %s\n", game->path);
    return false;
  }

  Libretro::Input::Init();

  return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info,
                             size_t num_info)
{
  return false;
}

void retro_unload_game(void)
{
  Core::Stop();
  Core::Shutdown();
  g_video_backend->ShutdownShared();
  Libretro::Input::Shutdown();
  Libretro::Log::Shutdown();
  UICommon::Shutdown();
}

namespace Libretro
{
// Disk swapping

// Dolphin expects to be able to use "/" (DIR_SEP) everywhere.
// RetroArch uses the OS separator.
// Convert between them when switching between systems.
std::string NormalizePath(const std::string& path)
{
  std::string newPath = path;
#ifdef _MSC_VER
  constexpr fs::path::value_type os_separator = fs::path::preferred_separator;
  static_assert(os_separator == DIR_SEP_CHR || os_separator == '\\', "Unsupported path separator");
  if (os_separator != DIR_SEP_CHR)
    std::replace(newPath.begin(), newPath.end(), '\\', DIR_SEP_CHR);
#endif

  return newPath;
}

std::string DenormalizePath(const std::string& path)
{
  std::string newPath = path;
#ifdef _MSC_VER
  constexpr fs::path::value_type os_separator = fs::path::preferred_separator;
  static_assert(os_separator == DIR_SEP_CHR || os_separator == '\\', "Unsupported path separator");
  if (os_separator != DIR_SEP_CHR)
    std::replace(newPath.begin(), newPath.end(), DIR_SEP_CHR, '\\');
#endif

  return newPath;
}

static bool retro_set_eject_state(bool ejected)
{
  if (eject_state == ejected)
    return false;

  eject_state = ejected;

  if (!ejected)
  {
    if (disk_index >= 0 && disk_index < (int)disk_paths.size())
    {
      Core::RunAsCPUThread([] { DVDInterface::ChangeDisc(NormalizePath(disk_paths[disk_index])); });
    }
  }

  return true;
}

static bool retro_get_eject_state()
{
  return eject_state;
}

static unsigned retro_get_image_index()
{
  return disk_index;
}

static bool retro_set_image_index(unsigned index)
{
  if (eject_state)
    disk_index = index;
  
  return eject_state;
}

static unsigned retro_get_num_images()
{
  return (unsigned)disk_paths.size();
}

static bool retro_add_image_index()
{
  disk_paths.push_back("");

  return true;
}

static bool retro_replace_image_index(unsigned index, const struct retro_game_info* info)
{
  if (index >= disk_paths.size())
    return false;

  if (!info->path)
  {
    disk_paths.erase(disk_paths.begin() + index);
    if (!disk_paths.size())
      disk_index = -1;
    else if (disk_index > (int)index)
      disk_index--;
  }
  else
    disk_paths[index] = info->path;

  return true;
}

static bool RETRO_CALLCONV retro_set_initial_image(unsigned index, const char* path)
{
  if (index >= disk_paths.size())
    index = 0;

  disk_index = index;

  return true;
}

static bool RETRO_CALLCONV retro_get_image_path(unsigned index, char* path, size_t len)
{
  if (index >= disk_paths.size())
    return false;

  if (disk_paths[index].empty())
    return false;

  strncpy(path, disk_paths[index].c_str(), len);
  return true;
}
static bool RETRO_CALLCONV retro_get_image_label(unsigned index, char* label, size_t len)
{
  if (index >= disk_paths.size())
    return false;

  if (disk_paths[index].empty())
    return false;

  strncpy(label, disk_paths[index].c_str(), len);
  return true;
}

static void InitDiskControlInterface()
{
  static retro_disk_control_ext_callback disk_control = {
      retro_set_eject_state,
      retro_get_eject_state,
      retro_get_image_index,
      retro_set_image_index,
      retro_get_num_images,
      retro_replace_image_index,
      retro_add_image_index,
      retro_set_initial_image,
      retro_get_image_path,
      retro_get_image_label,
  };

  environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &disk_control);
}
}  // namespace Libretro
