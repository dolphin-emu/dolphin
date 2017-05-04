// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"

// User directory indices for GetUserPath
enum
{
  D_USER_IDX,
  D_GCUSER_IDX,
  D_WIIROOT_IDX,
  D_SESSION_WIIROOT_IDX,
  D_CONFIG_IDX,
  D_GAMESETTINGS_IDX,
  D_MAPS_IDX,
  D_CACHE_IDX,
  D_SHADERCACHE_IDX,
  D_SHADERS_IDX,
  D_STATESAVES_IDX,
  D_SCREENSHOTS_IDX,
  D_HIRESTEXTURES_IDX,
  D_DUMP_IDX,
  D_DUMPFRAMES_IDX,
  D_DUMPAUDIO_IDX,
  D_DUMPTEXTURES_IDX,
  D_DUMPDSP_IDX,
  D_DUMPSSL_IDX,
  D_LOAD_IDX,
  D_LOGS_IDX,
  D_MAILLOGS_IDX,
  D_THEMES_IDX,
  D_PIPES_IDX,
  D_MEMORYWATCHER_IDX,
  D_WFSROOT_IDX,
  D_BACKUP_IDX,
  F_DOLPHINCONFIG_IDX,
  F_GCPADCONFIG_IDX,
  F_WIIPADCONFIG_IDX,
  F_GCKEYBOARDCONFIG_IDX,
  F_GFXCONFIG_IDX,
  F_DEBUGGERCONFIG_IDX,
  F_LOGGERCONFIG_IDX,
  F_UICONFIG_IDX,
  F_MAINLOG_IDX,
  F_RAMDUMP_IDX,
  F_ARAMDUMP_IDX,
  F_FAKEVMEMDUMP_IDX,
  F_GCSRAM_IDX,
  F_MEMORYWATCHERLOCATIONS_IDX,
  F_MEMORYWATCHERSOCKET_IDX,
  F_WIISDCARD_IDX,
  NUM_PATH_INDICES
};

static std::string s_user_paths[NUM_PATH_INDICES];
static void RebuildUserDirectories(unsigned int dir_index)
{
  switch (dir_index)
  {
  case D_USER_IDX:
    s_user_paths[D_GCUSER_IDX] = s_user_paths[D_USER_IDX] + GC_USER_DIR DIR_SEP;
    s_user_paths[D_WIIROOT_IDX] = s_user_paths[D_USER_IDX] + WII_USER_DIR;
    s_user_paths[D_CONFIG_IDX] = s_user_paths[D_USER_IDX] + CONFIG_DIR DIR_SEP;
    s_user_paths[D_GAMESETTINGS_IDX] = s_user_paths[D_USER_IDX] + GAMESETTINGS_DIR DIR_SEP;
    s_user_paths[D_MAPS_IDX] = s_user_paths[D_USER_IDX] + MAPS_DIR DIR_SEP;
    s_user_paths[D_CACHE_IDX] = s_user_paths[D_USER_IDX] + CACHE_DIR DIR_SEP;
    s_user_paths[D_SHADERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + SHADERCACHE_DIR DIR_SEP;
    s_user_paths[D_SHADERS_IDX] = s_user_paths[D_USER_IDX] + SHADERS_DIR DIR_SEP;
    s_user_paths[D_STATESAVES_IDX] = s_user_paths[D_USER_IDX] + STATESAVES_DIR DIR_SEP;
    s_user_paths[D_SCREENSHOTS_IDX] = s_user_paths[D_USER_IDX] + SCREENSHOTS_DIR DIR_SEP;
    s_user_paths[D_LOAD_IDX] = s_user_paths[D_USER_IDX] + LOAD_DIR DIR_SEP;
    s_user_paths[D_HIRESTEXTURES_IDX] = s_user_paths[D_LOAD_IDX] + HIRES_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_DUMP_IDX] = s_user_paths[D_USER_IDX] + DUMP_DIR DIR_SEP;
    s_user_paths[D_DUMPFRAMES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_FRAMES_DIR DIR_SEP;
    s_user_paths[D_DUMPAUDIO_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_AUDIO_DIR DIR_SEP;
    s_user_paths[D_DUMPTEXTURES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_DUMPDSP_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_DSP_DIR DIR_SEP;
    s_user_paths[D_DUMPSSL_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_SSL_DIR DIR_SEP;
    s_user_paths[D_LOGS_IDX] = s_user_paths[D_USER_IDX] + LOGS_DIR DIR_SEP;
    s_user_paths[D_MAILLOGS_IDX] = s_user_paths[D_LOGS_IDX] + MAIL_LOGS_DIR DIR_SEP;
    s_user_paths[D_THEMES_IDX] = s_user_paths[D_USER_IDX] + THEMES_DIR DIR_SEP;
    s_user_paths[D_PIPES_IDX] = s_user_paths[D_USER_IDX] + PIPES_DIR DIR_SEP;
    s_user_paths[D_WFSROOT_IDX] = s_user_paths[D_USER_IDX] + WFSROOT_DIR DIR_SEP;
    s_user_paths[D_BACKUP_IDX] = s_user_paths[D_USER_IDX] + BACKUP_DIR DIR_SEP;
    s_user_paths[F_DOLPHINCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DOLPHIN_CONFIG;
    s_user_paths[F_GCPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GCPAD_CONFIG;
    s_user_paths[F_WIIPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + WIIPAD_CONFIG;
    s_user_paths[F_GCKEYBOARDCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GCKEYBOARD_CONFIG;
    s_user_paths[F_GFXCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GFX_CONFIG;
    s_user_paths[F_DEBUGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DEBUGGER_CONFIG;
    s_user_paths[F_LOGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + LOGGER_CONFIG;
    s_user_paths[F_UICONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + UI_CONFIG;
    s_user_paths[F_MAINLOG_IDX] = s_user_paths[D_LOGS_IDX] + MAIN_LOG;
    s_user_paths[F_RAMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + RAM_DUMP;
    s_user_paths[F_ARAMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + ARAM_DUMP;
    s_user_paths[F_FAKEVMEMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + FAKEVMEM_DUMP;
    s_user_paths[F_GCSRAM_IDX] = s_user_paths[D_GCUSER_IDX] + GC_SRAM;
    s_user_paths[F_WIISDCARD_IDX] = s_user_paths[D_WIIROOT_IDX] + DIR_SEP WII_SDCARD;

    s_user_paths[D_MEMORYWATCHER_IDX] = s_user_paths[D_USER_IDX] + MEMORYWATCHER_DIR DIR_SEP;
    s_user_paths[F_MEMORYWATCHERLOCATIONS_IDX] =
        s_user_paths[D_MEMORYWATCHER_IDX] + MEMORYWATCHER_LOCATIONS;
    s_user_paths[F_MEMORYWATCHERSOCKET_IDX] =
        s_user_paths[D_MEMORYWATCHER_IDX] + MEMORYWATCHER_SOCKET;

    // The shader cache has moved to the cache directory, so remove the old one.
    // TODO: remove that someday.
    File::DeleteDirRecursively(s_user_paths[D_USER_IDX] + SHADERCACHE_LEGACY_DIR DIR_SEP);
    break;

  case D_CONFIG_IDX:
    s_user_paths[F_DOLPHINCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DOLPHIN_CONFIG;
    s_user_paths[F_GCPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GCPAD_CONFIG;
    s_user_paths[F_WIIPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + WIIPAD_CONFIG;
    s_user_paths[F_GFXCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GFX_CONFIG;
    s_user_paths[F_DEBUGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DEBUGGER_CONFIG;
    s_user_paths[F_LOGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + LOGGER_CONFIG;
    s_user_paths[F_UICONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + UI_CONFIG;
    break;

  case D_CACHE_IDX:
    s_user_paths[D_SHADERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + SHADERCACHE_DIR DIR_SEP;
    break;

  case D_GCUSER_IDX:
    s_user_paths[F_GCSRAM_IDX] = s_user_paths[D_GCUSER_IDX] + GC_SRAM;
    break;

  case D_DUMP_IDX:
    s_user_paths[D_DUMPFRAMES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_FRAMES_DIR DIR_SEP;
    s_user_paths[D_DUMPAUDIO_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_AUDIO_DIR DIR_SEP;
    s_user_paths[D_DUMPTEXTURES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_DUMPDSP_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_DSP_DIR DIR_SEP;
    s_user_paths[D_DUMPSSL_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_SSL_DIR DIR_SEP;
    s_user_paths[F_RAMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + RAM_DUMP;
    s_user_paths[F_ARAMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + ARAM_DUMP;
    s_user_paths[F_FAKEVMEMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + FAKEVMEM_DUMP;
    break;

  case D_LOGS_IDX:
    s_user_paths[D_MAILLOGS_IDX] = s_user_paths[D_LOGS_IDX] + MAIL_LOGS_DIR DIR_SEP;
    s_user_paths[F_MAINLOG_IDX] = s_user_paths[D_LOGS_IDX] + MAIN_LOG;
    break;

  case D_LOAD_IDX:
    s_user_paths[D_HIRESTEXTURES_IDX] = s_user_paths[D_LOAD_IDX] + HIRES_TEXTURES_DIR DIR_SEP;
    break;
  }
}

// Gets a set user directory path
// Don't call prior to setting the base user directory
static const std::string& GetUserPath(unsigned int dir_index)
{
  return s_user_paths[dir_index];
}

// Sets a user directory path
// Rebuilds internal directory structure to compensate for the new directory
static void SetUserPath(unsigned int dir_index, std::string path)
{
  if (path.empty())
    return;

  s_user_paths[dir_index] = path;
  RebuildUserDirectories(dir_index);
}

namespace Paths
{
void SetUserDir(std::string dir)
{
  SetUserPath(D_USER_IDX, dir);
}

void SetConfigDir(std::string dir)
{
  SetUserPath(D_CONFIG_IDX, dir);
}

void SetCacheDir(std::string dir)
{
  SetUserPath(D_CACHE_IDX, dir);
}

void SetDumpDir(std::string dir)
{
  SetUserPath(D_DUMP_IDX, dir);
}

void SetWiiRootDir(std::string dir)
{
  SetUserPath(D_WIIROOT_IDX, dir);
}

void SetSessionWiiRootDir(std::string dir)
{
  SetUserPath(D_SESSION_WIIROOT_IDX, dir);
}

void SetWiiSDCardFile(std::string file)
{
  SetUserPath(F_WIISDCARD_IDX, file);
}

const std::string& GetUserDir()
{
  return GetUserPath(D_USER_IDX);
};

const std::string& GetGCUserDir()
{
  return GetUserPath(D_GCUSER_IDX);
}
const std::string& GetWiiRootDir()
{
  return GetUserPath(D_WIIROOT_IDX);
}

const std::string& GetSessionWiiRootDir()
{
  return GetUserPath(D_SESSION_WIIROOT_IDX);
}

const std::string& GetConfigDir()
{
  return GetUserPath(D_CONFIG_IDX);
}

const std::string& GetGameSettingsDir()
{
  return GetUserPath(D_GAMESETTINGS_IDX);
}

const std::string& GetMapsDir()
{
  return GetUserPath(D_MAPS_IDX);
}

const std::string& GetCacheDir()
{
  return GetUserPath(D_CACHE_IDX);
}

const std::string& GetShaderCacheDir()
{
  return GetUserPath(D_SHADERCACHE_IDX);
}

const std::string& GetShadersDir()
{
  return GetUserPath(D_SHADERS_IDX);
}
const std::string& GetStateSavesDir()
{
  return GetUserPath(D_STATESAVES_IDX);
}

const std::string& GetScreenshotsDir()
{
  return GetUserPath(D_SCREENSHOTS_IDX);
}

const std::string& GetHiresTexturesDir()
{
  return GetUserPath(D_HIRESTEXTURES_IDX);
}

const std::string& GetDumpDir()
{
  return GetUserPath(D_DUMP_IDX);
}

const std::string& GetDumpFramesDir()
{
  return GetUserPath(D_DUMPFRAMES_IDX);
}

const std::string& GetDumpAudioDir()
{
  return GetUserPath(D_DUMPAUDIO_IDX);
}

const std::string& GetDumpTexturesDir()
{
  return GetUserPath(D_DUMPTEXTURES_IDX);
}

const std::string& GetDumpDSPDir()
{
  return GetUserPath(D_DUMPDSP_IDX);
}

const std::string& GetDumpSSLDir()
{
  return GetUserPath(D_DUMPSSL_IDX);
}

const std::string& GetLoadDir()
{
  return GetUserPath(D_LOAD_IDX);
}

const std::string& GetLogsDir()
{
  return GetUserPath(D_LOGS_IDX);
}

const std::string& GetMailLogsDir()
{
  return GetUserPath(D_MAILLOGS_IDX);
}

const std::string& GetThemesDir()
{
  return GetUserPath(D_THEMES_IDX);
}

const std::string& GetPipesDir()
{
  return GetUserPath(D_PIPES_IDX);
}

const std::string& GetMemoryWatcherDir()
{
  return GetUserPath(D_MEMORYWATCHER_IDX);
}

const std::string& GetWFSRootDir()
{
  return GetUserPath(D_WFSROOT_IDX);
}

const std::string& GetBackupDir()
{
  return GetUserPath(D_BACKUP_IDX);
}

const std::string& GetDolphinConfigFile()
{
  return GetUserPath(F_DOLPHINCONFIG_IDX);
}

const std::string& GetGCPadConfigFile()
{
  return GetUserPath(F_GCPADCONFIG_IDX);
}

const std::string& GetWiiPadConfigFile()
{
  return GetUserPath(F_WIIPADCONFIG_IDX);
}

const std::string& GetGCKeyboardConfigFile()
{
  return GetUserPath(F_GCKEYBOARDCONFIG_IDX);
}

const std::string& GetGFXConfigFile()
{
  return GetUserPath(F_GFXCONFIG_IDX);
}

const std::string& GetDebuggerConfigFile()
{
  return GetUserPath(F_DEBUGGERCONFIG_IDX);
}

const std::string& GetLoggerConfigFile()
{
  return GetUserPath(F_LOGGERCONFIG_IDX);
}

const std::string& GetUIConfigFile()
{
  return GetUserPath(F_UICONFIG_IDX);
}

const std::string& GetMainLogFile()
{
  return GetUserPath(F_MAINLOG_IDX);
}

const std::string& GetRAMDumpFile()
{
  return GetUserPath(F_RAMDUMP_IDX);
}

const std::string& GetARAMDumpFile()
{
  return GetUserPath(F_ARAMDUMP_IDX);
}

const std::string& GetFakeVMEMDumpFile()
{
  return GetUserPath(F_FAKEVMEMDUMP_IDX);
}

const std::string& GetGCSRAMFile()
{
  return GetUserPath(F_GCSRAM_IDX);
}

const std::string& GetMemoryWatcherLocationsFile()
{
  return GetUserPath(F_MEMORYWATCHERLOCATIONS_IDX);
}

const std::string& GetMemoryWatcherSocketFile()
{
  return GetUserPath(F_MEMORYWATCHERSOCKET_IDX);
}

const std::string& GetWiiSDCardFile()
{
  return GetUserPath(F_WIISDCARD_IDX);
}

}  // namespace Paths
