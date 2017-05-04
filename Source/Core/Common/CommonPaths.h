// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Directory separators, do we need this?
#define DIR_SEP "/"
#define DIR_SEP_CHR '/'

// The user data dir
#define ROOT_DIR "."
#ifdef _WIN32
#define USERDATA_DIR "User"
#define DOLPHIN_DATA_DIR "Dolphin"
#elif defined __APPLE__
// On OS X, USERDATA_DIR exists within the .app, but *always* reference
// the copy in Application Support instead! (Copied on first run)
// You can use the File::GetUserPath() util for this
#define USERDATA_DIR "Contents/Resources/User"
#define DOLPHIN_DATA_DIR "Library/Application Support/Dolphin"
#elif defined ANDROID
#define USERDATA_DIR "user"
#define DOLPHIN_DATA_DIR "/sdcard/dolphin-emu"
#else
#define USERDATA_DIR "user"
#define DOLPHIN_DATA_DIR "dolphin-emu"
#endif

// Shared data dirs (Sys and shared User for Linux)
#if defined(_WIN32) || defined(LINUX_LOCAL_DEV)
#define SYSDATA_DIR "Sys"
#elif defined __APPLE__
#define SYSDATA_DIR "Contents/Resources/Sys"
#elif defined ANDROID
#define SYSDATA_DIR "/sdcard/dolphin-emu"
#else
#ifdef DATA_DIR
#define SYSDATA_DIR DATA_DIR "sys"
#else
#define SYSDATA_DIR "sys"
#endif
#endif

// Dirs in both User and Sys
#define EUR_DIR "EUR"
#define USA_DIR "USA"
#define JAP_DIR "JAP"

// Subdirs in the User dir returned by GetUserPath(D_USER_IDX)
#define GC_USER_DIR "GC"
#define WII_USER_DIR "Wii"
#define CONFIG_DIR "Config"
#define GAMESETTINGS_DIR "GameSettings"
#define MAPS_DIR "Maps"
#define CACHE_DIR "Cache"
#define SHADERCACHE_DIR "Shaders"
#define STATESAVES_DIR "StateSaves"
#define SCREENSHOTS_DIR "ScreenShots"
#define LOAD_DIR "Load"
#define HIRES_TEXTURES_DIR "Textures"
#define DUMP_DIR "Dump"
#define DUMP_TEXTURES_DIR "Textures"
#define DUMP_FRAMES_DIR "Frames"
#define DUMP_AUDIO_DIR "Audio"
#define DUMP_DSP_DIR "DSP"
#define DUMP_SSL_DIR "SSL"
#define LOGS_DIR "Logs"
#define MAIL_LOGS_DIR "Mail"
#define SHADERS_DIR "Shaders"
#define WII_SYSCONF_DIR "shared2" DIR_SEP "sys"
#define WII_WC24CONF_DIR "shared2" DIR_SEP "wc24"
#define RESOURCES_DIR "Resources"
#define THEMES_DIR "Themes"
#define ANAGLYPH_DIR "Anaglyph"
#define PIPES_DIR "Pipes"
#define MEMORYWATCHER_DIR "MemoryWatcher"
#define WFSROOT_DIR "WFS"
#define BACKUP_DIR "Backup"

// This one is only used to remove it if it was present
#define SHADERCACHE_LEGACY_DIR "ShaderCache"

// The theme directory used by default
#define DEFAULT_THEME_DIR "Clean"

// Filenames
// Files in the directory returned by GetUserPath(D_CONFIG_IDX)
#define DOLPHIN_CONFIG "Dolphin.ini"
#define GCPAD_CONFIG "GCPadNew.ini"
#define WIIPAD_CONFIG "WiimoteNew.ini"
#define GCKEYBOARD_CONFIG "GCKeyNew.ini"
#define GFX_CONFIG "GFX.ini"
#define DEBUGGER_CONFIG "Debugger.ini"
#define LOGGER_CONFIG "Logger.ini"
#define UI_CONFIG "UI.ini"

// Files in the directory returned by GetUserPath(D_LOGS_IDX)
#define MAIN_LOG "dolphin.log"

// Files in the directory returned by GetUserPath(D_WIISYSCONF_IDX)
#define WII_SYSCONF "SYSCONF"

// Files in the directory returned by GetUserPath(D_DUMP_IDX)
#define RAM_DUMP "ram.raw"
#define ARAM_DUMP "aram.raw"
#define FAKEVMEM_DUMP "fakevmem.raw"

// Files in the directory returned by GetUserPath(D_MEMORYWATCHER_IDX)
#define MEMORYWATCHER_LOCATIONS "Locations.txt"
#define MEMORYWATCHER_SOCKET "MemoryWatcher"

// Sys files
#define TOTALDB "totaldb.dsy"

#define FONT_WINDOWS_1252 "font_western.bin"
#define FONT_SHIFT_JIS "font_japanese.bin"

#define DSP_IROM "dsp_rom.bin"
#define DSP_COEF "dsp_coef.bin"

#define GC_IPL "IPL.bin"
#define GC_SRAM "SRAM.raw"
#define GC_MEMCARDA "MemoryCardA"
#define GC_MEMCARDB "MemoryCardB"

#define WII_STATE "state.dat"

#define WII_SDCARD "sd.raw"
#define WII_BTDINF_BACKUP "btdinf.bak"

#define WII_SETTING "setting.txt"

#define GECKO_CODE_HANDLER "codehandler.bin"

// Subdirs in Sys
#define GC_SYS_DIR "GC"
#define WII_SYS_DIR "Wii"

#include <string>
#include "Common/FileUtil.h"

namespace Paths
{
using namespace File;
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
}
