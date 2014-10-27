// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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
	#ifdef USER_DIR
		#define DOLPHIN_DATA_DIR USER_DIR
	#else
		#define DOLPHIN_DATA_DIR ".dolphin"
	#endif
#endif

// Shared data dirs (Sys and shared User for linux)
#ifdef _WIN32
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
#define GC_USER_DIR         "GC"
#define WII_USER_DIR        "Wii"
#define CONFIG_DIR          "Config"
#define GAMESETTINGS_DIR    "GameSettings"
#define MAPS_DIR            "Maps"
#define CACHE_DIR           "Cache"
#define SHADERCACHE_DIR     "ShaderCache"
#define STATESAVES_DIR      "StateSaves"
#define SCREENSHOTS_DIR     "ScreenShots"
#define LOAD_DIR            "Load"
#define HIRES_TEXTURES_DIR  "Textures"
#define DUMP_DIR            "Dump"
#define DUMP_TEXTURES_DIR   "Textures"
#define DUMP_FRAMES_DIR     "Frames"
#define DUMP_AUDIO_DIR      "Audio"
#define DUMP_DSP_DIR        "DSP"
#define LOGS_DIR            "Logs"
#define MAIL_LOGS_DIR       "Mail"
#define SHADERS_DIR         "Shaders"
#define WII_SYSCONF_DIR     "shared2" DIR_SEP "sys"
#define WII_WC24CONF_DIR    "shared2" DIR_SEP "wc24"
#define THEMES_DIR          "Themes"

// Filenames
// Files in the directory returned by GetUserPath(D_CONFIG_IDX)
#define DOLPHIN_CONFIG  "Dolphin.ini"
#define DEBUGGER_CONFIG "Debugger.ini"
#define LOGGER_CONFIG   "Logger.ini"

// Files in the directory returned by GetUserPath(D_LOGS_IDX)
#define MAIN_LOG    "dolphin.log"

// Files in the directory returned by GetUserPath(D_WIISYSCONF_IDX)
#define WII_SYSCONF "SYSCONF"

// Files in the directory returned by GetUserPath(D_DUMP_IDX)
#define RAM_DUMP      "ram.raw"
#define ARAM_DUMP     "aram.raw"
#define FAKEVMEM_DUMP "fakevmem.raw"

// Sys files
#define TOTALDB     "totaldb.dsy"

#define FONT_ANSI   "font_ansi.bin"
#define FONT_SJIS   "font_sjis.bin"

#define DSP_IROM    "dsp_rom.bin"
#define DSP_COEF    "dsp_coef.bin"

#define GC_IPL      "IPL.bin"
#define GC_SRAM     "SRAM.raw"
#define GC_MEMCARDA "MemoryCardA"
#define GC_MEMCARDB "MemoryCardB"

#define WII_STATE   "state.dat"

#define WII_SETTING "setting.txt"

#define GECKO_CODE_HANDLER "codehandler.bin"

// Subdirs in Sys
#define GC_SYS_DIR "GC"
#define WII_SYS_DIR "Wii"
