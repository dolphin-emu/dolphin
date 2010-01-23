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

// Library suffix/prefix
#ifdef _WIN32
#define PLUGIN_PREFIX ""
#define PLUGIN_SUFFIX ".dll"
#elif defined __APPLE__
#define PLUGIN_PREFIX "lib"
#define PLUGIN_SUFFIX ".dylib"
#else
#define PLUGIN_PREFIX "lib"
#define PLUGIN_SUFFIX ".so"
#endif

// Directory seperators, do we need this?
#define DIR_SEP "/"
#define DIR_SEP_CHR '/'

#if defined __APPLE__
#define PLUGINS_DIR "Contents/PlugIns"
#define SYSDATA_DIR "Contents/Sys"
#else
#define PLUGINS_DIR "Plugins"
#define SYSDATA_DIR "Sys"
#endif
#define ROOT_DIR "."
#define USERDATA_DIR "User"

// Where data directory is
#ifdef _WIN32
#define DOLPHIN_DATA_DIR "Dolphin"
#elif defined __APPLE__
#define DOLPHIN_DATA_DIR "Library/Application Support/Dolphin"
#else
#define DOLPHIN_DATA_DIR ".dolphin"
#endif

// Dirs in both User and Sys
#define EUR_DIR "EUR"
#define USA_DIR "USA"
#define JAP_DIR "JAP"

// Dirs in User
#define GC_USER_DIR "GC"
#define WII_USER_DIR "Wii"
#define WII_SYSCONF_DIR "shared2/sys"
#define CONFIG_DIR "Config"
#define GAMECONFIG_DIR "GameConfig"
#define MAPS_DIR "Maps"
#define CACHE_DIR "Cache"
#define SHADERCACHE_DIR "ShaderCache"
#define STATESAVES_DIR "StateSaves"
#define SCREENSHOTS_DIR "ScreenShots"
#define DUMP_DIR "Dump"
#define DUMP_TEXTURES_DIR "Textures"
#define LOAD_DIR "Load"
#define HIRES_TEXTURES_DIR "Textures"
#define DUMP_FRAMES_DIR "Frames"
#define DUMP_DSP_DIR "DSP"
#define LOGS_DIR "Logs"
#define MAIL_LOGS_DIR "Mail"

// Dirs in Sys
#define GC_SYS_DIR "GC"
#define WII_SYS_DIR "Wii"

// Filenames
#define DOLPHIN_CONFIG "Dolphin.ini"
#define DEBUGGER_CONFIG "Debugger.ini"
#define LOGGER_CONFIG "Logger.ini"
#define TOTALDB "totaldb.dsy"
#define MAIN_LOG "dolphin.log"

#define DEFAULT_GFX_PLUGIN	PLUGIN_PREFIX "Plugin_VideoOGL" PLUGIN_SUFFIX
#define DEFAULT_DSP_PLUGIN	PLUGIN_PREFIX "Plugin_DSP_HLE" PLUGIN_SUFFIX
#define DEFAULT_PAD_PLUGIN	PLUGIN_PREFIX "Plugin_GCPad" PLUGIN_SUFFIX
#define DEFAULT_WIIMOTE_PLUGIN	PLUGIN_PREFIX "Plugin_Wiimote" PLUGIN_SUFFIX

#define FONT_ANSI "font_ansi.bin"
#define FONT_SJIS "font_sjis.bin"

#define DSP_IROM "dsp_rom.bin"
#define DSP_COEF "dsp_coef.bin"

#define GC_IPL "IPL.bin"
#define GC_SRAM "SRAM.raw"
#define GC_MEMCARDA "MemoryCardA"
#define GC_MEMCARDB "MemoryCardB"

#define WII_EUR_SETTING "setting-eur.txt"
#define WII_USA_SETTING "setting-usa.txt"
#define WII_JAP_SETTING "setting-jpn.txt"
#define WII_SYSCONF "SYSCONF"

#define RAM_DUMP "ram.raw"
#define ARAM_DUMP "aram.raw"

// Shorts - dirs
// User dirs
#define FULL_USERDATA_DIR	ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP
#define T_FULLUSERDATA_DIR	_T(ROOT_DIR) _T(DIR_SEP) _T(USERDATA_DIR) _T(DIR_SEP)

#define FULL_GC_USER_DIR	FULL_USERDATA_DIR GC_USER_DIR DIR_SEP
#define T_FULL_GC_USER_DIR	T_FULLUSERDATA_DIR _T(GC_USER_DIR) _T(DIR_SEP)

#define FULL_WII_USER_DIR	FULL_USERDATA_DIR WII_USER_DIR DIR_SEP
#define FULL_WII_ROOT_DIR	FULL_USERDATA_DIR WII_USER_DIR // This is the "root" for Wii fs, so that it may be used with created devices

#define FULL_GAMECONFIG_DIR	FULL_USERDATA_DIR GAMECONFIG_DIR DIR_SEP
#define T_FULL_GAMECONFIG_DIR T_FULLUSERDATA_DIR _T(GAMECONFIG_DIR) _T(DIR_SEP)

#define FULL_CONFIG_DIR		FULL_USERDATA_DIR CONFIG_DIR DIR_SEP
#define FULL_CACHE_DIR		FULL_USERDATA_DIR CACHE_DIR DIR_SEP 
#define FULL_SHADERCACHE_DIR FULL_USERDATA_DIR SHADERCACHE_DIR DIR_SEP 
#define FULL_STATESAVES_DIR	FULL_USERDATA_DIR STATESAVES_DIR DIR_SEP
#define FULL_SCREENSHOTS_DIR FULL_USERDATA_DIR SCREENSHOTS_DIR DIR_SEP
#define FULL_FRAMES_DIR		FULL_USERDATA_DIR DUMP_DIR DIR_SEP DUMP_FRAMES_DIR 
#define FULL_DUMP_DIR		FULL_USERDATA_DIR DUMP_DIR DIR_SEP
#define FULL_DUMP_TEXTURES_DIR FULL_USERDATA_DIR DUMP_DIR DIR_SEP DUMP_TEXTURES_DIR DIR_SEP
#define FULL_HIRES_TEXTURES_DIR FULL_USERDATA_DIR LOAD_DIR DIR_SEP HIRES_TEXTURES_DIR DIR_SEP
#define FULL_DSP_DUMP_DIR	FULL_USERDATA_DIR DUMP_DIR DIR_SEP DUMP_DSP_DIR DIR_SEP
#define FULL_LOGS_DIR		FULL_USERDATA_DIR LOGS_DIR DIR_SEP
#define FULL_MAIL_LOGS_DIR	FULL_LOGS_DIR MAIL_LOGS_DIR DIR_SEP
#define FULL_MAPS_DIR		FULL_USERDATA_DIR MAPS_DIR DIR_SEP
#define FULL_WII_SYSCONF_DIR 	FULL_WII_USER_DIR WII_SYSCONF_DIR DIR_SEP

// Sys dirs
#define FULL_SYSDATA_DIR	ROOT_DIR DIR_SEP SYSDATA_DIR DIR_SEP

#define FULL_GC_SYS_DIR	FULL_SYSDATA_DIR GC_SYS_DIR DIR_SEP
//#define GC_SYS_EUR_DIR FULL_GC_SYS_DIR EUR_DIR
//#define GC_SYS_USA_DIR FULL_GC_SYS_DIR USA_DIR
//#define GC_SYS_JAP_DIR FULL_GC_SYS_DIR JAP_DIR

#define FULL_WII_SYS_DIR	FULL_SYSDATA_DIR WII_SYS_DIR DIR_SEP

// Shorts - files
// User files
#define CONFIG_FILE				FULL_CONFIG_DIR DOLPHIN_CONFIG
#define DEBUGGER_CONFIG_FILE	FULL_CONFIG_DIR DEBUGGER_CONFIG
#define LOGGER_CONFIG_FILE		FULL_CONFIG_DIR LOGGER_CONFIG

#define TOTALDB_FILE			FULL_SYSDATA_DIR TOTALDB
#define MAINRAM_DUMP_FILE		FULL_DUMP_DIR RAM_DUMP
#define ARAM_DUMP_FILE			FULL_DUMP_DIR ARAM_DUMP
#define GC_SRAM_FILE	FULL_USERDATA_DIR GC_USER_DIR DIR_SEP GC_SRAM

#define MAIN_LOG_FILE FULL_LOGS_DIR MAIN_LOG

// Sys files
#define FONT_ANSI_FILE	FULL_GC_SYS_DIR FONT_ANSI
#define FONT_SJIS_FILE	FULL_GC_SYS_DIR FONT_SJIS

#define DSP_IROM_FILE	FULL_GC_SYS_DIR DSP_IROM
#define DSP_COEF_FILE	FULL_GC_SYS_DIR DSP_COEF

#define WII_EUR_SETTING_FILE	FULL_WII_SYS_DIR WII_EUR_SETTING
#define WII_USA_SETTING_FILE	FULL_WII_SYS_DIR WII_USA_SETTING
#define WII_JAP_SETTING_FILE	FULL_WII_SYS_DIR WII_JAP_SETTING
#define WII_SYSCONF_FILE        FULL_WII_SYSCONF_DIR WII_SYSCONF

#define FULL_WII_MENU_DIR       FULL_WII_USER_DIR "title" DIR_SEP "00000001" DIR_SEP "00000002" DIR_SEP "content"

#endif // _COMMON_PATHS_H_
