// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Directory separators, do we need this?
#define DIR_SEP "/"
#define DIR_SEP_CHR '/'

// The current working directory
#define ROOT_DIR "."

// The normal user directory
#ifndef STEAM
#ifdef _WIN32
#define NORMAL_USER_DIR "Dolphin Emulator"
#elif defined(__APPLE__)
#define NORMAL_USER_DIR "Library/Application Support/Dolphin"
#elif defined(ANDROID)
#define NORMAL_USER_DIR "/sdcard/dolphin-emu"
#else
#define NORMAL_USER_DIR "dolphin-emu"
#endif
#else  // ifndef STEAM
#ifdef _WIN32
#define NORMAL_USER_DIR "Dolphin Emulator (Steam)"
#elif defined(__APPLE__)
#define NORMAL_USER_DIR "Library/Application Support/Dolphin (Steam)"
#else
#define NORMAL_USER_DIR "dolphin-emu-steam"
#endif
#endif

// The portable user directory
#ifdef _WIN32
#define PORTABLE_USER_DIR "User"
#elif defined(__APPLE__)
#define PORTABLE_USER_DIR "User"
#define EMBEDDED_USER_DIR "Contents/Resources/User"
#else
#define PORTABLE_USER_DIR "user"
#define EMBEDDED_USER_DIR PORTABLE_USER_DIR
#endif

// Flag file to prevent media scanning from indexing a directory
#define NOMEDIA_FILE ".nomedia"

// Dirs in both User and Sys
// Legacy setups used /JAP/ while newer setups use /JPN/ by default.
#define EUR_DIR "EUR"
#define USA_DIR "USA"
#define JAP_DIR "JAP"
#define JPN_DIR "JPN"

// Subdirs in the User dir returned by GetUserPath(D_USER_IDX)
#define GC_USER_DIR "GC"
#define GBA_USER_DIR "GBA"
#define WII_USER_DIR "Wii"
#define CONFIG_DIR "Config"
#define GAMESETTINGS_DIR "GameSettings"
#define MAPS_DIR "Maps"
#define CACHE_DIR "Cache"
#define COVERCACHE_DIR "GameCovers"
#define REDUMPCACHE_DIR "Redump"
#define SHADERCACHE_DIR "Shaders"
#define STATESAVES_DIR "StateSaves"
#define SCREENSHOTS_DIR "ScreenShots"
#define LOAD_DIR "Load"
#define HIRES_TEXTURES_DIR "Textures"
#define RIIVOLUTION_DIR "Riivolution"
#define DUMP_DIR "Dump"
#define DUMP_TEXTURES_DIR "Textures"
#define DUMP_FRAMES_DIR "Frames"
#define DUMP_OBJECTS_DIR "Objects"
#define DUMP_AUDIO_DIR "Audio"
#define DUMP_DSP_DIR "DSP"
#define DUMP_SSL_DIR "SSL"
#define DUMP_DEBUG_DIR "Debug"
#define DUMP_DEBUG_BRANCHWATCH_DIR "BranchWatch"
#define DUMP_DEBUG_JITBLOCKS_DIR "JitBlocks"
#define LOGS_DIR "Logs"
#define MAIL_LOGS_DIR "Mail"
#define SHADERS_DIR "Shaders"
#define WII_SYSCONF_DIR "shared2" DIR_SEP "sys"
#define WII_WC24CONF_DIR "shared2" DIR_SEP "wc24"
#define RESOURCES_DIR "Resources"
#define THEMES_DIR "Themes"
#define STYLES_DIR "Styles"
#define GBASAVES_DIR "Saves"
#define ANAGLYPH_DIR "Anaglyph"
#define PASSIVE_DIR "Passive"
#define PIPES_DIR "Pipes"
#define MEMORYWATCHER_DIR "MemoryWatcher"
#define WFSROOT_DIR "WFS"
#define BACKUP_DIR "Backup"
#define RESOURCEPACK_DIR "ResourcePacks"
#define DYNAMICINPUT_DIR "DynamicInputTextures"
#define GRAPHICSMOD_DIR "GraphicMods"
#define WIISDSYNC_DIR "WiiSDSync"
#define ASSEMBLY_DIR "SavedAssembly"

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
#define LOGGER_CONFIG "Logger.ini"
#define DUALSHOCKUDPCLIENT_CONFIG "DSUClient.ini"
#define FREELOOK_CONFIG "FreeLook.ini"
#define RETROACHIEVEMENTS_CONFIG "RetroAchievements.ini"

// Files in the directory returned by GetUserPath(D_LOGS_IDX)
#define MAIN_LOG "dolphin.log"

// Files in the directory returned by GetUserPath(D_WIISYSCONF_IDX)
#define WII_SYSCONF "SYSCONF"

// Files in the directory returned by GetUserPath(D_DUMP_IDX)
#define MEM1_DUMP "mem1.raw"
#define MEM2_DUMP "mem2.raw"
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
#define GC_MEMCARD_NETPLAY "NetPlayTemp"

#define GBA_BIOS "gba_bios.bin"
#define GBA_SAVE_NETPLAY "NetPlayTemp"

#define WII_STATE "state.dat"

#define WII_SD_CARD_IMAGE "WiiSD.raw"
#define WII_BTDINF_BACKUP "btdinf.bak"

#define WII_SETTING "setting.txt"

#define GECKO_CODE_HANDLER "codehandler.bin"

// Subdirs in Sys
#define GC_SYS_DIR "GC"
#define WII_SYS_DIR "Wii"

// Subdirs in Config
#define GRAPHICSMOD_CONFIG_DIR "GraphicMods"

// GPU drivers
#define GPU_DRIVERS "GpuDrivers"
#define GPU_DRIVERS_EXTRACTED "Extracted"
#define GPU_DRIVERS_TMP "Tmp"
#define GPU_DRIVERS_HOOK "Hook"
#define GPU_DRIVERS_FILE_REDIRECT "FileRedirect"
