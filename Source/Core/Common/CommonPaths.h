// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Directory separators, do we need this?
#define DIR_SEP "/"
#define DIR_SEP_CHR '/'

// The user data dir
#define ROOT_DIR "."
#if defined _WIN32
#define USERDATA_DIR "User"
#define DOLPHIN_DATA_DIR "Dolphin"
#elif defined __APPLE__
// On OS X, USERDATA_DIR exists within the .app, but *always* reference
// the copy in Application Support instead! (Copied on first run)
#define USERDATA_DIR "Contents/Resources/User"
#define DOLPHIN_DATA_DIR "Library/Application Support/Dolphin"
#elif defined ANDROID
#define USERDATA_DIR "user"
#define DOLPHIN_DATA_DIR "/sdcard/dolphin-emu"
#else
#define USERDATA_DIR "user"
#define DOLPHIN_DATA_DIR "dolphin-emu"
#endif

// In the User dir
#define ANAGLYPH_DIR "Anaglyph"
#define GAMESETTINGS_DIR "GameSettings"
#define MAPS_DIR "Maps"
#define RESOURCES_DIR "Resources"
#define SHADERS_DIR "Shaders"
#define THEMES_DIR "Themes"

// Dirs in both User and Sys
#define EUR_DIR "EUR"
#define USA_DIR "USA"
#define JAP_DIR "JAP"

// The theme directory used by default
#define DEFAULT_THEME_DIR "Clean"

// Sys files
#define TOTALDB "totaldb.dsy"

#define FONT_WINDOWS_1252 "font_western.bin"
#define FONT_SHIFT_JIS "font_japanese.bin"

#define DSP_IROM "dsp_rom.bin"
#define DSP_COEF "dsp_coef.bin"

#define GC_IPL "IPL.bin"
#define GC_MEMCARDA "MemoryCardA"
#define GC_MEMCARDB "MemoryCardB"

#define WII_SDCARD "sd.raw"

#define GECKO_CODE_HANDLER "codehandler.bin"

#include <string>

namespace Paths
{
// Returns the path to where the sys file are
std::string GetSysDirectory();
std::string GetGCSysDirectory();
std::string GetWiiSysDirectory();
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
