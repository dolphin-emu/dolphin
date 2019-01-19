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

namespace Libretro
{
extern retro_environment_t environ_cb;
static void InitDiskControlInterface();
}

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

  Config::SetBase(Config::SYSCONF_LANGUAGE, (u32)(DiscIO::Language)Libretro::Options::Language);
  Config::SetBase(Config::SYSCONF_WIDESCREEN, Libretro::Options::Widescreen);
  Config::SetBase(Config::SYSCONF_PROGRESSIVE_SCAN, Libretro::Options::progressiveScan);
  Config::SetBase(Config::SYSCONF_PAL60, Libretro::Options::pal60);
  Config::SetBase(Config::SYSCONF_SENSOR_BAR_POSITION, Libretro::Options::sensorBarPosition);

  Config::SetBase(Config::GFX_WIDESCREEN_HACK, Libretro::Options::WidescreenHack);
  Config::SetBase(Config::GFX_EFB_SCALE, Libretro::Options::efbScale);
  Config::SetBase(Config::GFX_ASPECT_RATIO, AspectMode::Stretch);
  Config::SetBase(Config::GFX_BACKEND_MULTITHREADING, false);
  Config::SetBase(Config::GFX_SHADER_COMPILATION_MODE, Libretro::Options::shaderCompilationMode);
  Config::SetBase(Config::GFX_ENHANCE_MAX_ANISOTROPY, Libretro::Options::maxAnisotropy);
  Config::SetBase(Config::GFX_HACK_COPY_EFB_SCALED, Libretro::Options::efbScaledCopy);
  Config::SetBase(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, Libretro::Options::efbToTexture);
  Config::SetBase(Config::GFX_HACK_DISABLE_COPY_TO_VRAM, Libretro::Options::efbToVram);
  Config::SetBase(Config::GFX_ENABLE_GPU_TEXTURE_DECODING, Libretro::Options::gpuTextureDecoding);
  Config::SetBase(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING, Libretro::Options::waitForShaders);
  Config::SetBase(Config::GFX_HIRES_TEXTURES, Libretro::Options::loadCustomTextures);
#if 0
  Config::SetBase(Config::GFX_SHADER_COMPILER_THREADS, 1);
  Config::SetBase(Config::GFX_SHADER_PRECOMPILER_THREADS, 1);
#endif

  Libretro::Video::Init();
  VideoBackendBase::PopulateBackendInfo();
  NOTICE_LOG(VIDEO, "Using GFX backend: %s", Config::Get(Config::MAIN_GFX_BACKEND).c_str());

  WindowSystemInfo wsi(WindowSystemType::Libretro, nullptr, nullptr, nullptr);
  if (!BootManager::BootCore(BootParameters::GenerateFromFile(game->path), wsi))
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
static struct retro_disk_control_callback retro_disk_control_cb;
static unsigned disk_index = 0;
static std::vector<std::string> disk_paths;

static bool retro_set_eject_state(bool ejected)
{
  return true;
}

static bool retro_get_eject_state()
{
  return false;
}

static unsigned retro_get_image_index()
{
  return disk_index;
}

static bool retro_set_image_index(unsigned index)
{
  disk_index = index;
  if (disk_index >= disk_paths.size())
  {
    // No disk in drive
    return true;
  }
  Core::RunAsCPUThread([] { DVDInterface::ChangeDisc(disk_paths[disk_index]); });

  return true;
}

static unsigned retro_get_num_images()
{
  return disk_paths.size();
}

static bool retro_add_image_index()
{
  disk_paths.push_back("");

  return true;
}

static bool retro_replace_image_index(unsigned index, const struct retro_game_info *info)
{
  if (info == nullptr)
  {
    if (index < disk_paths.size())
    {
      disk_paths.erase(disk_paths.begin() + index);
      if (disk_index >= index && disk_index > 0)
        disk_index--;
    }
  }
  else
    disk_paths[index] = info->path;

  return true;
}

static void InitDiskControlInterface()
{
  retro_disk_control_cb.set_eject_state = retro_set_eject_state;
  retro_disk_control_cb.get_eject_state = retro_get_eject_state;
  retro_disk_control_cb.set_image_index = retro_set_image_index;
  retro_disk_control_cb.get_image_index = retro_get_image_index;
  retro_disk_control_cb.get_num_images  = retro_get_num_images;
  retro_disk_control_cb.add_image_index = retro_add_image_index;
  retro_disk_control_cb.replace_image_index = retro_replace_image_index;

  environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &retro_disk_control_cb);
}
}   // namespace Libretro
