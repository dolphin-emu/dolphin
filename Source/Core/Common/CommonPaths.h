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

namespace Paths
{
// Returns the path to where the sys file are
std::string GetSysDirectory();
std::string& GetExeDirectory();

#ifdef __APPLE__
std::string GetBundleDirectory();
#endif

// Sets a user directory path
// Rebuilds internal directory structure to compensate for the new directory
void SetUserDir(std::string dir);
void SetConfigDir(std::string dir);
void SetCacheDir(std::string dir);
void SetDumpDir(std::string dir);
void SetWiiRootDir(std::string dir);
void SetSessionWiiRootDir(std::string dir);
void SetWiiSDCardFile(std::string file);

// Gets a set user directory path
// Don't call prior to setting the base user directory
const std::string& GetUserDir();
const std::string& GetGCUserDir();
// always points to User/Wii or global user-configured directory
const std::string& GetWiiRootDir();
// may point to minimal temporary directory for determinism
const std::string& GetSessionWiiRootDir();
// global settings
const std::string& GetConfigDir();
// user-specified settings which override both the global and the default settings (per game)
const std::string& GetGameSettingsDir();
const std::string& GetMapsDir();
const std::string& GetCacheDir();
const std::string& GetShaderCacheDir();
const std::string& GetShadersDir();
const std::string& GetStateSavesDir();
const std::string& GetScreenshotsDir();
const std::string& GetHiresTexturesDir();
const std::string& GetDumpDir();
const std::string& GetDumpFramesDir();
const std::string& GetDumpAudioDir();
const std::string& GetDumpTexturesDir();
const std::string& GetDumpDSPDir();
const std::string& GetDumpSSLDir();
const std::string& GetLoadDir();
const std::string& GetLogsDir();
const std::string& GetMailLogsDir();
const std::string& GetThemesDir();
const std::string& GetPipesDir();
const std::string& GetMemoryWatcherDir();
const std::string& GetWFSRootDir();
const std::string& GetBackupDir();
const std::string& GetDolphinConfigFile();
const std::string& GetGCPadConfigFile();
const std::string& GetWiiPadConfigFile();
const std::string& GetGCKeyboardConfigFile();
const std::string& GetGFXConfigFile();
const std::string& GetDebuggerConfigFile();
const std::string& GetLoggerConfigFile();
const std::string& GetUIConfigFile();
const std::string& GetMainLogFile();
const std::string& GetRAMDumpFile();
const std::string& GetARAMDumpFile();
const std::string& GetFakeVMEMDumpFile();
const std::string& GetGCSRAMFile();
const std::string& GetMemoryWatcherLocationsFile();
const std::string& GetMemoryWatcherSocketFile();
const std::string& GetWiiSDCardFile();

std::string GetThemeDir(const std::string& theme);
}  // namespace Paths
