// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _COMMON_PATHS_H_
#define _COMMON_PATHS_H_

#ifdef __linux__
#include "Config.h"
#endif

// Library suffix/prefix
#ifdef _WIN32
	#define PLUGIN_PREFIX ""
	#define PLUGIN_SUFFIX ".dll"
#elif defined __APPLE__
	#define PLUGIN_PREFIX "lib"
	#define PLUGIN_SUFFIX ".dylib"
#elif defined __linux__
	#define PLUGIN_PREFIX "lib"
	#define PLUGIN_SUFFIX ".so"
#endif

// Directory seperators, do we need this?
#define DIR_SEP "/"
#define DIR_SEP_CHR '/'

// Location of the plugins
#ifdef _WIN32
	#define PLUGINS_DIR "Plugins"
#elif defined __APPLE__
	#define PLUGINS_DIR "Contents/PlugIns"
#elif defined __linux__
	#ifdef LIBS_DIR
		#define PLUGINS_DIR LIBS_DIR "dolphin-emu"
	#else
		#define PLUGINS_DIR "plugins"
	#endif
#endif

// The user data dir
#define ROOT_DIR "."
#ifdef _WIN32
	#define USERDATA_DIR "User"
	#define DOLPHIN_DATA_DIR "Dolphin"
#elif defined __APPLE__
	// On OS X, USERDATA_DIR exists within the .app, but *always* reference
	// the copy in Application Support instead! (Copied on first run)
	// You can use the File::GetUserPath() util for this
	#define USERDATA_DIR "Contents/User"
	#define DOLPHIN_DATA_DIR "Library/Application Support/Dolphin"
#elif defined __linux__
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
	#define SYSDATA_DIR "Contents/Sys"
#elif defined __linux__
	#ifdef DATA_DIR
		#define SYSDATA_DIR DATA_DIR "sys"
		#define SHARED_USER_DIR  DATA_DIR USERDATA_DIR DIR_SEP
	#else
		#define SYSDATA_DIR "sys"
		#define SHARED_USER_DIR  ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP
	#endif
#endif

// Dirs in both User and Sys
#define EUR_DIR "EUR"
#define USA_DIR "USA"
#define JAP_DIR "JAP"

// Subdirs in the User dir returned by GetUserPath(D_USER_IDX)
#define GC_USER_DIR		"GC"
#define WII_USER_DIR		"Wii"
#define CONFIG_DIR		"Config"
#define GAMECONFIG_DIR		"GameConfig"
#define MAPS_DIR		"Maps"
#define CACHE_DIR		"Cache"
#define SHADERCACHE_DIR		"ShaderCache"
#define STATESAVES_DIR		"StateSaves"
#define SCREENSHOTS_DIR		"ScreenShots"
#define LOAD_DIR 		"Load"
#define HIRES_TEXTURES_DIR	LOAD_DIR DIR_SEP "Textures"
#define DUMP_DIR		"Dump"
#define DUMP_TEXTURES_DIR	DUMP_DIR DIR_SEP "Textures"
#define DUMP_FRAMES_DIR		DUMP_DIR DIR_SEP "Frames"
#define DUMP_DSP_DIR		DUMP_DIR DIR_SEP "DSP"
#define LOGS_DIR		"Logs"
#define MAIL_LOGS_DIR		LOGS_DIR DIR_SEP "Mail"
#define SHADERS_DIR 		"Shaders"
#define WII_SYSCONF_DIR		WII_USER_DIR DIR_SEP "shared2" DIR_SEP "sys"
#define WII_MENU_DIR		WII_USER_DIR DIR_SEP "title" DIR_SEP "00000001" DIR_SEP "00000002" DIR_SEP "content"

// Filenames
// Files in the directory returned by GetUserPath(D_CONFIG_IDX)
#define DOLPHIN_CONFIG	"Dolphin.ini"
#define DEBUGGER_CONFIG	"Debugger.ini"
#define LOGGER_CONFIG	"Logger.ini"

// Files in the directory returned by GetUserPath(D_LOGS_IDX)
#define MAIN_LOG	"dolphin.log"

// Files in the directory returned by GetUserPath(D_WIISYSCONF_IDX)
#define WII_SYSCONF	"SYSCONF"

// Files in the directory returned by GetUserPath(D_DUMP_IDX)
#define RAM_DUMP	"ram.raw"
#define ARAM_DUMP	"aram.raw"

// Plugin files
#define DEFAULT_GFX_PLUGIN	PLUGIN_PREFIX "Plugin_VideoOGL" PLUGIN_SUFFIX
#define DEFAULT_DSP_PLUGIN	PLUGIN_PREFIX "Plugin_DSP_HLE" PLUGIN_SUFFIX
#define DEFAULT_WIIMOTE_PLUGIN	PLUGIN_PREFIX "Plugin_Wiimote" PLUGIN_SUFFIX

// Sys files
#define TOTALDB		"totaldb.dsy"

#define FONT_ANSI	"font_ansi.bin"
#define FONT_SJIS	"font_sjis.bin"

#define DSP_IROM	"dsp_rom.bin"
#define DSP_COEF	"dsp_coef.bin"

#define GC_IPL		"IPL.bin"
#define GC_SRAM		"SRAM.raw"
#define GC_MEMCARDA	"MemoryCardA"
#define GC_MEMCARDB	"MemoryCardB"

#define WII_EUR_SETTING "setting-eur.txt"
#define WII_USA_SETTING "setting-usa.txt"
#define WII_JAP_SETTING "setting-jpn.txt"

// Subdirs in Sys
#define GC_SYS_DIR "GC"
#define WII_SYS_DIR "Wii"

// Full Sys dirs
#define FULL_SYSDATA_DIR	ROOT_DIR DIR_SEP SYSDATA_DIR DIR_SEP
#define FULL_GC_SYS_DIR		FULL_SYSDATA_DIR GC_SYS_DIR DIR_SEP
//#define GC_SYS_EUR_DIR	FULL_GC_SYS_DIR EUR_DIR
//#define GC_SYS_USA_DIR	FULL_GC_SYS_DIR USA_DIR
//#define GC_SYS_JAP_DIR	FULL_GC_SYS_DIR JAP_DIR
#define FULL_WII_SYS_DIR	FULL_SYSDATA_DIR WII_SYS_DIR DIR_SEP

// Sys files with full path
#define TOTALDB_FILE	FULL_SYSDATA_DIR TOTALDB

#define FONT_ANSI_FILE	FULL_GC_SYS_DIR FONT_ANSI
#define FONT_SJIS_FILE	FULL_GC_SYS_DIR FONT_SJIS

#define DSP_IROM_FILE	FULL_GC_SYS_DIR DSP_IROM
#define DSP_COEF_FILE	FULL_GC_SYS_DIR DSP_COEF

#define WII_EUR_SETTING_FILE	FULL_WII_SYS_DIR WII_EUR_SETTING
#define WII_USA_SETTING_FILE	FULL_WII_SYS_DIR WII_USA_SETTING
#define WII_JAP_SETTING_FILE	FULL_WII_SYS_DIR WII_JAP_SETTING

#endif // _COMMON_PATHS_H_
