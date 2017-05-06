// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"

#ifdef _WIN32
#include <windows.h>
#include "Common/StringUtil.h"
#else
#include <limits.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <sys/param.h>
#endif

// Shared data dirs (Sys and shared User for Linux)
constexpr auto SYSDATA_DIR =
#if defined _WIN32 || defined LINUX_LOCAL_DEV
    "Sys"
#elif defined __APPLE__
    "Contents/Resources/Sys"
#elif defined ANDROID
    "/sdcard/dolphin-emu"
#elif defined DATA_DIR
    DATA_DIR "sys"
#else
    "sys"
#endif
    ;

// Subdirs in the Sys dir
constexpr auto GC_SYS_DIR = "GC";
constexpr auto WII_SYS_DIR = "Wii";

// Subdirs in the User dir
constexpr auto GC_USER_DIR = "GC";
constexpr auto WII_USER_DIR = "Wii";
constexpr auto CONFIG_DIR = "Config";
constexpr auto CACHE_DIR = "Cache";
constexpr auto SHADERCACHE_DIR = "Shaders";
constexpr auto STATESAVES_DIR = "StateSaves";
constexpr auto SCREENSHOTS_DIR = "ScreenShots";
constexpr auto LOAD_DIR = "Load";
constexpr auto HIRES_TEXTURES_DIR = "Textures";
constexpr auto DUMP_DIR = "Dump";
constexpr auto DUMP_TEXTURES_DIR = "Textures";
constexpr auto DUMP_FRAMES_DIR = "Frames";
constexpr auto DUMP_AUDIO_DIR = "Audio";
constexpr auto DUMP_DSP_DIR = "DSP";
constexpr auto DUMP_SSL_DIR = "SSL";
constexpr auto LOGS_DIR = "Logs";
constexpr auto MAIL_LOGS_DIR = "Mail";
constexpr auto PIPES_DIR = "Pipes";
constexpr auto MEMORYWATCHER_DIR = "MemoryWatcher";
constexpr auto WFSROOT_DIR = "WFS";
constexpr auto BACKUP_DIR = "Backup";

// This one is only used to remove it if it was present
constexpr auto SHADERCACHE_LEGACY_DIR = "ShaderCache";

// Filenames
// Files in the user directory
constexpr auto GC_SRAM = "SRAM.raw";

// Files in the config directory
constexpr auto DOLPHIN_CONFIG = "Dolphin.ini";
constexpr auto GCPAD_CONFIG = "GCPadNew.ini";
constexpr auto WIIPAD_CONFIG = "WiimoteNew.ini";
constexpr auto GCKEYBOARD_CONFIG = "GCKeyNew.ini";
constexpr auto GFX_CONFIG = "GFX.ini";
constexpr auto DEBUGGER_CONFIG = "Debugger.ini";
constexpr auto LOGGER_CONFIG = "Logger.ini";
constexpr auto UI_CONFIG = "UI.ini";

// Files in the logs directory
constexpr auto MAIN_LOG = "dolphin.log";

// Files in the dump directory
constexpr auto RAM_DUMP = "ram.raw";
constexpr auto ARAM_DUMP = "aram.raw";
constexpr auto FAKEVMEM_DUMP = "fakevmem.raw";

// Files in the memorywatcher directory
constexpr auto MEMORYWATCHER_LOCATIONS = "Locations.txt";
constexpr auto MEMORYWATCHER_SOCKET = "MemoryWatcher";

static std::string s_user_dir;
static std::string s_gcuser_dir;
static std::string s_wiiroot_dir;
static std::string s_session_wiiroot_dir;
static std::string s_config_dir;
static std::string s_gamesettings_dir;
static std::string s_maps_dir;
static std::string s_cache_dir;
static std::string s_shadercache_dir;
static std::string s_shaders_dir;
static std::string s_statesaves_dir;
static std::string s_screenshots_dir;
static std::string s_hirestextures_dir;
static std::string s_dump_dir;
static std::string s_dumpframes_dir;
static std::string s_dumpaudio_dir;
static std::string s_dumptextures_dir;
static std::string s_dumpdsp_dir;
static std::string s_dumpssl_dir;
static std::string s_load_dir;
static std::string s_logs_dir;
static std::string s_maillogs_dir;
static std::string s_themes_dir;
static std::string s_pipes_dir;
static std::string s_memorywatcher_dir;
static std::string s_wfsroot_dir;
static std::string s_backup_dir;
static std::string s_dolphinconfig_file;
static std::string s_gcpadconfig_file;
static std::string s_wiipadconfig_file;
static std::string s_gckeyboardconfig_file;
static std::string s_gfxconfig_file;
static std::string s_debuggerconfig_file;
static std::string s_loggerconfig_file;
static std::string s_uiconfig_file;
static std::string s_mainlog_file;
static std::string s_ramdump_file;
static std::string s_aramdump_file;
static std::string s_fakevmemdump_file;
static std::string s_gcsram_file;
static std::string s_memorywatcherlocations_file;
static std::string s_memorywatchersocket_file;
static std::string s_wiisdcard_file;

// Rebuilds internal directory structure to compensate for the new directory
static void RebuildUserDirectories()
{
  s_gamesettings_dir = s_user_dir + GAMESETTINGS_DIR + DIR_SEP;
  s_backup_dir = s_user_dir + BACKUP_DIR + DIR_SEP;
  s_maps_dir = s_user_dir + MAPS_DIR + DIR_SEP;
  s_pipes_dir = s_user_dir + PIPES_DIR + DIR_SEP;
  s_screenshots_dir = s_user_dir + SCREENSHOTS_DIR + DIR_SEP;
  s_shaders_dir = s_user_dir + SHADERS_DIR + DIR_SEP;
  s_statesaves_dir = s_user_dir + STATESAVES_DIR + DIR_SEP;
  s_themes_dir = s_user_dir + THEMES_DIR + DIR_SEP;
  s_wfsroot_dir = s_user_dir + WFSROOT_DIR + DIR_SEP;

  s_gcuser_dir = s_user_dir + GC_USER_DIR + DIR_SEP;
  s_gcsram_file = s_gcuser_dir + GC_SRAM;

  s_wiiroot_dir = s_user_dir + WII_USER_DIR;
  s_wiisdcard_file = s_wiiroot_dir + DIR_SEP + WII_SDCARD;

  s_cache_dir = s_user_dir + CACHE_DIR + DIR_SEP;
  s_shadercache_dir = s_cache_dir + SHADERCACHE_DIR + DIR_SEP;
  // The shader cache has moved to the cache directory, so remove the old one.
  // TODO: remove that someday.
  File::DeleteDirRecursively(s_user_dir + SHADERCACHE_LEGACY_DIR + DIR_SEP);

  s_load_dir = s_user_dir + LOAD_DIR + DIR_SEP;
  s_hirestextures_dir = s_load_dir + HIRES_TEXTURES_DIR + DIR_SEP;

  s_dump_dir = s_user_dir + DUMP_DIR + DIR_SEP;
  s_dumpaudio_dir = s_dump_dir + DUMP_AUDIO_DIR + DIR_SEP;
  s_dumpdsp_dir = s_dump_dir + DUMP_DSP_DIR + DIR_SEP;
  s_dumpframes_dir = s_dump_dir + DUMP_FRAMES_DIR + DIR_SEP;
  s_dumpssl_dir = s_dump_dir + DUMP_SSL_DIR + DIR_SEP;
  s_dumptextures_dir = s_dump_dir + DUMP_TEXTURES_DIR + DIR_SEP;
  s_aramdump_file = s_dump_dir + ARAM_DUMP;
  s_fakevmemdump_file = s_dump_dir + FAKEVMEM_DUMP;
  s_ramdump_file = s_dump_dir + RAM_DUMP;

  s_logs_dir = s_user_dir + LOGS_DIR + DIR_SEP;
  s_maillogs_dir = s_logs_dir + MAIL_LOGS_DIR + DIR_SEP;
  s_mainlog_file = s_logs_dir + MAIN_LOG;

  s_config_dir = s_user_dir + CONFIG_DIR + DIR_SEP;
  s_debuggerconfig_file = s_config_dir + DEBUGGER_CONFIG;
  s_dolphinconfig_file = s_config_dir + DOLPHIN_CONFIG;
  s_gckeyboardconfig_file = s_config_dir + GCKEYBOARD_CONFIG;
  s_gcpadconfig_file = s_config_dir + GCPAD_CONFIG;
  s_gfxconfig_file = s_config_dir + GFX_CONFIG;
  s_loggerconfig_file = s_config_dir + LOGGER_CONFIG;
  s_uiconfig_file = s_config_dir + UI_CONFIG;
  s_wiipadconfig_file = s_config_dir + WIIPAD_CONFIG;

  s_memorywatcher_dir = s_user_dir + MEMORYWATCHER_DIR + DIR_SEP;
  s_memorywatcherlocations_file = s_memorywatcher_dir + MEMORYWATCHER_LOCATIONS;
  s_memorywatchersocket_file = s_memorywatcher_dir + MEMORYWATCHER_SOCKET;
}

namespace Paths
{
#if defined(__APPLE__)
std::string GetBundleDirectory()
{
  CFURLRef BundleRef;
  char AppBundlePath[MAXPATHLEN];
  // Get the main bundle for the app
  BundleRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
  CFStringRef BundlePath = CFURLCopyFileSystemPath(BundleRef, kCFURLPOSIXPathStyle);
  CFStringGetFileSystemRepresentation(BundlePath, AppBundlePath, sizeof(AppBundlePath));
  CFRelease(BundleRef);
  CFRelease(BundlePath);

  return AppBundlePath;
}
#endif

std::string& GetExeDirectory()
{
  static std::string DolphinPath;
  if (DolphinPath.empty())
  {
#ifdef _WIN32
    TCHAR Dolphin_exe_Path[2048];
    TCHAR Dolphin_exe_Clean_Path[MAX_PATH];
    GetModuleFileName(nullptr, Dolphin_exe_Path, 2048);
    if (_tfullpath(Dolphin_exe_Clean_Path, Dolphin_exe_Path, MAX_PATH) != nullptr)
      DolphinPath = TStrToUTF8(Dolphin_exe_Clean_Path);
    else
      DolphinPath = TStrToUTF8(Dolphin_exe_Path);
    DolphinPath = DolphinPath.substr(0, DolphinPath.find_last_of('\\'));
#else
    char Dolphin_exe_Path[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", Dolphin_exe_Path, sizeof(Dolphin_exe_Path));
    if (len == -1 || len == sizeof(Dolphin_exe_Path))
    {
      len = 0;
    }
    Dolphin_exe_Path[len] = '\0';
    DolphinPath = Dolphin_exe_Path;
    DolphinPath = DolphinPath.substr(0, DolphinPath.rfind('/'));
#endif
  }
  return DolphinPath;
}

std::string GetSysDirectory()
{
  std::string sysDir;

#if defined(__APPLE__)
  sysDir = GetBundleDirectory() + DIR_SEP + SYSDATA_DIR;
#elif defined(_WIN32) || defined(LINUX_LOCAL_DEV)
  sysDir = GetExeDirectory() + DIR_SEP + SYSDATA_DIR;
#else
  sysDir = SYSDATA_DIR;
#endif
  sysDir += DIR_SEP;

  INFO_LOG(COMMON, "GetSysDirectory: Setting to %s:", sysDir.c_str());
  return sysDir;
}

std::string GetGCSysDirectory()
{
  return GetSysDirectory() + GC_SYS_DIR + DIR_SEP;
}

std::string GetWiiSysDirectory()
{
  return GetSysDirectory() + WII_SYS_DIR + DIR_SEP;
}

void SetUserDir(std::string dir)
{
  if (dir.empty())
    return;
  s_user_dir = dir;
  RebuildUserDirectories();
}

void SetConfigDir(std::string dir)
{
  if (dir.empty())
    return;
  s_config_dir = dir;
  RebuildUserDirectories();
}

void SetCacheDir(std::string dir)
{
  if (dir.empty())
    return;
  s_cache_dir = dir;
  RebuildUserDirectories();
}

void SetDumpDir(std::string dir)
{
  if (dir.empty())
    return;
  s_dump_dir = dir;
  RebuildUserDirectories();
}

void SetWiiRootDir(std::string dir)
{
  if (dir.empty())
    return;
  s_wiiroot_dir = dir;
  RebuildUserDirectories();
}

void SetSessionWiiRootDir(std::string dir)
{
  if (dir.empty())
    return;
  s_session_wiiroot_dir = dir;
  RebuildUserDirectories();
}

void SetWiiSDCardFile(std::string file)
{
  if (file.empty())
    return;
  s_wiisdcard_file = file;
  RebuildUserDirectories();
}

const std::string& GetUserDir()
{
  return s_user_dir;
};

const std::string& GetGCUserDir()
{
  return s_gcuser_dir;
}
const std::string& GetWiiRootDir()
{
  return s_wiiroot_dir;
}

const std::string& GetSessionWiiRootDir()
{
  return s_session_wiiroot_dir;
}

const std::string& GetConfigDir()
{
  return s_config_dir;
}

const std::string& GetGameSettingsDir()
{
  return s_gamesettings_dir;
}

const std::string& GetMapsDir()
{
  return s_maps_dir;
}

const std::string& GetCacheDir()
{
  return s_cache_dir;
}

const std::string& GetShaderCacheDir()
{
  return s_shadercache_dir;
}

const std::string& GetShadersDir()
{
  return s_shaders_dir;
}
const std::string& GetStateSavesDir()
{
  return s_statesaves_dir;
}

const std::string& GetScreenshotsDir()
{
  return s_screenshots_dir;
}

const std::string& GetHiresTexturesDir()
{
  return s_hirestextures_dir;
}

const std::string& GetDumpDir()
{
  return s_dump_dir;
}

const std::string& GetDumpFramesDir()
{
  return s_dumpframes_dir;
}

const std::string& GetDumpAudioDir()
{
  return s_dumpaudio_dir;
}

const std::string& GetDumpTexturesDir()
{
  return s_dumptextures_dir;
}

const std::string& GetDumpDSPDir()
{
  return s_dumpdsp_dir;
}

const std::string& GetDumpSSLDir()
{
  return s_dumpssl_dir;
}

const std::string& GetLoadDir()
{
  return s_load_dir;
}

const std::string& GetLogsDir()
{
  return s_logs_dir;
}

const std::string& GetMailLogsDir()
{
  return s_maillogs_dir;
}

const std::string& GetThemesDir()
{
  return s_themes_dir;
}

const std::string& GetPipesDir()
{
  return s_pipes_dir;
}

const std::string& GetMemoryWatcherDir()
{
  return s_memorywatcher_dir;
}

const std::string& GetWFSRootDir()
{
  return s_wfsroot_dir;
}

const std::string& GetBackupDir()
{
  return s_backup_dir;
}

const std::string& GetDolphinConfigFile()
{
  return s_dolphinconfig_file;
}

const std::string& GetGCPadConfigFile()
{
  return s_gcpadconfig_file;
}

const std::string& GetWiiPadConfigFile()
{
  return s_wiipadconfig_file;
}

const std::string& GetGCKeyboardConfigFile()
{
  return s_gckeyboardconfig_file;
}

const std::string& GetGFXConfigFile()
{
  return s_gfxconfig_file;
}

const std::string& GetDebuggerConfigFile()
{
  return s_debuggerconfig_file;
}

const std::string& GetLoggerConfigFile()
{
  return s_loggerconfig_file;
}

const std::string& GetUIConfigFile()
{
  return s_uiconfig_file;
}

const std::string& GetMainLogFile()
{
  return s_mainlog_file;
}

const std::string& GetRAMDumpFile()
{
  return s_ramdump_file;
}

const std::string& GetARAMDumpFile()
{
  return s_aramdump_file;
}

const std::string& GetFakeVMEMDumpFile()
{
  return s_fakevmemdump_file;
}

const std::string& GetGCSRAMFile()
{
  return s_gcsram_file;
}

const std::string& GetMemoryWatcherLocationsFile()
{
  return s_memorywatcherlocations_file;
}

const std::string& GetMemoryWatcherSocketFile()
{
  return s_memorywatchersocket_file;
}

const std::string& GetWiiSDCardFile()
{
  return s_wiisdcard_file;
}

std::string GetThemeDir(const std::string& theme_name)
{
  std::string dir = GetThemesDir() + theme_name + DIR_SEP;
  if (File::Exists(dir))
    return dir;

  // If the theme doesn't exist in the user dir, load from shared directory
  dir = GetSysDirectory() + THEMES_DIR + DIR_SEP + theme_name + DIR_SEP;
  if (File::Exists(dir))
    return dir;

  // If the theme doesn't exist at all, load the default theme
  return GetSysDirectory() + THEMES_DIR + DIR_SEP + DEFAULT_THEME_DIR + DIR_SEP;
}

}  // namespace Paths
