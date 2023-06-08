// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <set>
#include <string>
#include <utility>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "DiscIO/Enums.h"

// DSP Backend Types
#define BACKEND_NULLSOUND _trans("No Audio Output")
#define BACKEND_ALSA "ALSA"
#define BACKEND_CUBEB "Cubeb"
#define BACKEND_OPENAL "OpenAL"
#define BACKEND_PULSEAUDIO "Pulse"
#define BACKEND_OPENSLES "OpenSLES"
#define BACKEND_WASAPI _trans("WASAPI (Exclusive Mode)")

namespace PowerPC
{
enum class CPUCore;
}

namespace AudioCommon
{
enum class DPL2Quality;
}

namespace ExpansionInterface
{
enum class EXIDeviceType : int;
enum class Slot : int;
}  // namespace ExpansionInterface

namespace SerialInterface
{
enum SIDevices : int;
}

namespace HSP
{
enum class HSPDeviceType : int;
}

namespace Config
{
// Main.Core

extern const Info<bool> MAIN_SKIP_IPL;
extern const Info<PowerPC::CPUCore> MAIN_CPU_CORE;
extern const Info<bool> MAIN_JIT_FOLLOW_BRANCH;
extern const Info<bool> MAIN_FASTMEM;
extern const Info<bool> MAIN_FASTMEM_ARENA;
extern const Info<bool> MAIN_ACCURATE_CPU_CACHE;
// Should really be in the DSP section, but we're kind of stuck with bad decisions made in the past.
extern const Info<bool> MAIN_DSP_HLE;
extern const Info<int> MAIN_MAX_FALLBACK;
extern const Info<int> MAIN_TIMING_VARIANCE;
extern const Info<bool> MAIN_CPU_THREAD;
extern const Info<bool> MAIN_SYNC_ON_SKIP_IDLE;
extern const Info<std::string> MAIN_DEFAULT_ISO;
extern const Info<bool> MAIN_ENABLE_CHEATS;
extern const Info<int> MAIN_GC_LANGUAGE;
extern const Info<bool> MAIN_OVERRIDE_REGION_SETTINGS;
extern const Info<bool> MAIN_DPL2_DECODER;
extern const Info<AudioCommon::DPL2Quality> MAIN_DPL2_QUALITY;
extern const Info<int> MAIN_AUDIO_LATENCY;
extern const Info<bool> MAIN_AUDIO_STRETCH;
extern const Info<int> MAIN_AUDIO_STRETCH_LATENCY;
extern const Info<std::string> MAIN_MEMCARD_A_PATH;
extern const Info<std::string> MAIN_MEMCARD_B_PATH;
const Info<std::string>& GetInfoForMemcardPath(ExpansionInterface::Slot slot);
extern const Info<std::string> MAIN_AGP_CART_A_PATH;
extern const Info<std::string> MAIN_AGP_CART_B_PATH;
const Info<std::string>& GetInfoForAGPCartPath(ExpansionInterface::Slot slot);
extern const Info<std::string> MAIN_GCI_FOLDER_A_PATH;
extern const Info<std::string> MAIN_GCI_FOLDER_B_PATH;
const Info<std::string>& GetInfoForGCIPath(ExpansionInterface::Slot slot);
extern const Info<std::string> MAIN_GCI_FOLDER_A_PATH_OVERRIDE;
extern const Info<std::string> MAIN_GCI_FOLDER_B_PATH_OVERRIDE;
const Info<std::string>& GetInfoForGCIPathOverride(ExpansionInterface::Slot slot);
extern const Info<int> MAIN_MEMORY_CARD_SIZE;
extern const Info<ExpansionInterface::EXIDeviceType> MAIN_SLOT_A;
extern const Info<ExpansionInterface::EXIDeviceType> MAIN_SLOT_B;
extern const Info<ExpansionInterface::EXIDeviceType> MAIN_SERIAL_PORT_1;
const Info<ExpansionInterface::EXIDeviceType>& GetInfoForEXIDevice(ExpansionInterface::Slot slot);
extern const Info<std::string> MAIN_BBA_MAC;
extern const Info<std::string> MAIN_BBA_XLINK_IP;
extern const Info<bool> MAIN_BBA_XLINK_CHAT_OSD;
extern const Info<std::string> MAIN_BBA_BUILTIN_DNS;
extern const Info<std::string> MAIN_BBA_BUILTIN_IP;
const Info<SerialInterface::SIDevices>& GetInfoForSIDevice(int channel);
const Info<bool>& GetInfoForAdapterRumble(int channel);
const Info<bool>& GetInfoForSimulateKonga(int channel);
extern const Info<bool> MAIN_WII_SD_CARD;
extern const Info<bool> MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC;
extern const Info<u64> MAIN_WII_SD_CARD_FILESIZE;
extern const Info<bool> MAIN_WII_KEYBOARD;
extern const Info<bool> MAIN_WIIMOTE_CONTINUOUS_SCANNING;
extern const Info<bool> MAIN_WIIMOTE_ENABLE_SPEAKER;
extern const Info<bool> MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE;
extern const Info<bool> MAIN_MMU;
extern const Info<bool> MAIN_PAUSE_ON_PANIC;
extern const Info<int> MAIN_BB_DUMP_PORT;
extern const Info<bool> MAIN_SYNC_GPU;
extern const Info<int> MAIN_SYNC_GPU_MAX_DISTANCE;
extern const Info<int> MAIN_SYNC_GPU_MIN_DISTANCE;
extern const Info<float> MAIN_SYNC_GPU_OVERCLOCK;
extern const Info<bool> MAIN_FAST_DISC_SPEED;
extern const Info<bool> MAIN_LOW_DCBZ_HACK;
extern const Info<bool> MAIN_FLOAT_EXCEPTIONS;
extern const Info<bool> MAIN_DIVIDE_BY_ZERO_EXCEPTIONS;
extern const Info<bool> MAIN_FPRF;
extern const Info<bool> MAIN_ACCURATE_NANS;
extern const Info<bool> MAIN_DISABLE_ICACHE;
extern const Info<float> MAIN_EMULATION_SPEED;
extern const Info<float> MAIN_OVERCLOCK;
extern const Info<bool> MAIN_OVERCLOCK_ENABLE;
extern const Info<bool> MAIN_RAM_OVERRIDE_ENABLE;
extern const Info<u32> MAIN_MEM1_SIZE;
extern const Info<u32> MAIN_MEM2_SIZE;
// Should really be part of System::GFX, but again, we're stuck with past mistakes.
extern const Info<std::string> MAIN_GFX_BACKEND;
extern const Info<HSP::HSPDeviceType> MAIN_HSP_DEVICE;
extern const Info<u32> MAIN_ARAM_EXPANSION_SIZE;

enum class GPUDeterminismMode
{
  Auto,
  Disabled,
  // This is currently the only mode.  There will probably be at least
  // one more at some point.
  FakeCompletion,
};
extern const Info<std::string> MAIN_GPU_DETERMINISM_MODE;
GPUDeterminismMode GetGPUDeterminismMode();

extern const Info<std::string> MAIN_PERF_MAP_DIR;
extern const Info<bool> MAIN_CUSTOM_RTC_ENABLE;
extern const Info<u32> MAIN_CUSTOM_RTC_VALUE;
extern const Info<bool> MAIN_AUTO_DISC_CHANGE;
extern const Info<bool> MAIN_ALLOW_SD_WRITES;
extern const Info<bool> MAIN_ENABLE_SAVESTATES;
extern const Info<DiscIO::Region> MAIN_FALLBACK_REGION;
extern const Info<bool> MAIN_REAL_WII_REMOTE_REPEAT_REPORTS;
extern const Info<s32> MAIN_OVERRIDE_BOOT_IOS;
extern const Info<std::string> MAIN_WII_NUS_SHOP_URL;
extern const Info<bool> MAIN_WII_WIILINK_ENABLE;

// Main.DSP

extern const Info<bool> MAIN_DSP_THREAD;
extern const Info<bool> MAIN_DSP_CAPTURE_LOG;
extern const Info<bool> MAIN_DSP_JIT;
extern const Info<bool> MAIN_DUMP_AUDIO;
extern const Info<bool> MAIN_DUMP_AUDIO_SILENT;
extern const Info<bool> MAIN_DUMP_UCODE;
extern const Info<std::string> MAIN_AUDIO_BACKEND;
extern const Info<int> MAIN_AUDIO_VOLUME;
extern const Info<bool> MAIN_AUDIO_MUTED;
#ifdef _WIN32
extern const Info<std::string> MAIN_WASAPI_DEVICE;
#endif

bool ShouldUseDPL2Decoder();

// Main.Display

extern const Info<std::string> MAIN_FULLSCREEN_DISPLAY_RES;
extern const Info<bool> MAIN_FULLSCREEN;
extern const Info<bool> MAIN_RENDER_TO_MAIN;
extern const Info<int> MAIN_RENDER_WINDOW_XPOS;
extern const Info<int> MAIN_RENDER_WINDOW_YPOS;
extern const Info<int> MAIN_RENDER_WINDOW_WIDTH;
extern const Info<int> MAIN_RENDER_WINDOW_HEIGHT;
extern const Info<bool> MAIN_RENDER_WINDOW_AUTOSIZE;
extern const Info<bool> MAIN_KEEP_WINDOW_ON_TOP;
extern const Info<bool> MAIN_DISABLE_SCREENSAVER;

// Main.General

extern const Info<std::string> MAIN_DUMP_PATH;
extern const Info<std::string> MAIN_LOAD_PATH;
extern const Info<std::string> MAIN_RESOURCEPACK_PATH;
extern const Info<std::string> MAIN_FS_PATH;
extern const Info<std::string> MAIN_WII_SD_CARD_IMAGE_PATH;
extern const Info<std::string> MAIN_WII_SD_CARD_SYNC_FOLDER_PATH;
extern const Info<std::string> MAIN_WFS_PATH;
extern const Info<bool> MAIN_SHOW_LAG;
extern const Info<bool> MAIN_SHOW_FRAME_COUNT;
extern const Info<std::string> MAIN_WIRELESS_MAC;
extern const Info<std::string> MAIN_GDB_SOCKET;
extern const Info<int> MAIN_GDB_PORT;
extern const Info<int> MAIN_ISO_PATH_COUNT;
extern const Info<std::string> MAIN_SKYLANDERS_PATH;
std::vector<std::string> GetIsoPaths();
void SetIsoPaths(const std::vector<std::string>& paths);

// Main.GBA

#ifdef HAS_LIBMGBA
extern const Info<std::string> MAIN_GBA_BIOS_PATH;
extern const std::array<Info<std::string>, 4> MAIN_GBA_ROM_PATHS;
extern const Info<std::string> MAIN_GBA_SAVES_PATH;
extern const Info<bool> MAIN_GBA_SAVES_IN_ROM_PATH;
extern const Info<bool> MAIN_GBA_THREADS;
#endif

// Main.Network

extern const Info<bool> MAIN_NETWORK_SSL_DUMP_READ;
extern const Info<bool> MAIN_NETWORK_SSL_DUMP_WRITE;
extern const Info<bool> MAIN_NETWORK_SSL_VERIFY_CERTIFICATES;
extern const Info<bool> MAIN_NETWORK_SSL_DUMP_ROOT_CA;
extern const Info<bool> MAIN_NETWORK_SSL_DUMP_PEER_CERT;
extern const Info<bool> MAIN_NETWORK_DUMP_BBA;
extern const Info<bool> MAIN_NETWORK_DUMP_AS_PCAP;
extern const Info<int> MAIN_NETWORK_TIMEOUT;

// Main.Interface

extern const Info<bool> MAIN_USE_HIGH_CONTRAST_TOOLTIPS;
extern const Info<bool> MAIN_USE_PANIC_HANDLERS;
extern const Info<bool> MAIN_ABORT_ON_PANIC_ALERT;
extern const Info<bool> MAIN_OSD_MESSAGES;
extern const Info<bool> MAIN_SKIP_NKIT_WARNING;
extern const Info<bool> MAIN_CONFIRM_ON_STOP;

enum class ShowCursor
{
  Never,
  Constantly,
  OnMovement,
};
extern const Info<ShowCursor> MAIN_SHOW_CURSOR;

extern const Info<bool> MAIN_LOCK_CURSOR;
extern const Info<std::string> MAIN_INTERFACE_LANGUAGE;
extern const Info<bool> MAIN_SHOW_ACTIVE_TITLE;
extern const Info<bool> MAIN_USE_BUILT_IN_TITLE_DATABASE;
extern const Info<std::string> MAIN_THEME_NAME;
extern const Info<bool> MAIN_PAUSE_ON_FOCUS_LOST;
extern const Info<bool> MAIN_ENABLE_DEBUGGING;

// Main.Analytics

extern const Info<std::string> MAIN_ANALYTICS_ID;
extern const Info<bool> MAIN_ANALYTICS_ENABLED;
extern const Info<bool> MAIN_ANALYTICS_PERMISSION_ASKED;

// Main.GameList

extern const Info<bool> MAIN_GAMELIST_LIST_DRIVES;
extern const Info<bool> MAIN_GAMELIST_LIST_WAD;
extern const Info<bool> MAIN_GAMELIST_LIST_ELF_DOL;
extern const Info<bool> MAIN_GAMELIST_LIST_WII;
extern const Info<bool> MAIN_GAMELIST_LIST_GC;
extern const Info<bool> MAIN_GAMELIST_LIST_JPN;
extern const Info<bool> MAIN_GAMELIST_LIST_PAL;
extern const Info<bool> MAIN_GAMELIST_LIST_USA;
extern const Info<bool> MAIN_GAMELIST_LIST_AUSTRALIA;
extern const Info<bool> MAIN_GAMELIST_LIST_FRANCE;
extern const Info<bool> MAIN_GAMELIST_LIST_GERMANY;
extern const Info<bool> MAIN_GAMELIST_LIST_ITALY;
extern const Info<bool> MAIN_GAMELIST_LIST_KOREA;
extern const Info<bool> MAIN_GAMELIST_LIST_NETHERLANDS;
extern const Info<bool> MAIN_GAMELIST_LIST_RUSSIA;
extern const Info<bool> MAIN_GAMELIST_LIST_SPAIN;
extern const Info<bool> MAIN_GAMELIST_LIST_TAIWAN;
extern const Info<bool> MAIN_GAMELIST_LIST_WORLD;
extern const Info<bool> MAIN_GAMELIST_LIST_UNKNOWN;
extern const Info<int> MAIN_GAMELIST_LIST_SORT;
extern const Info<int> MAIN_GAMELIST_LIST_SORT_SECONDARY;

extern const Info<bool> MAIN_GAMELIST_COLUMN_PLATFORM;
extern const Info<bool> MAIN_GAMELIST_COLUMN_DESCRIPTION;
extern const Info<bool> MAIN_GAMELIST_COLUMN_BANNER;
extern const Info<bool> MAIN_GAMELIST_COLUMN_TITLE;
extern const Info<bool> MAIN_GAMELIST_COLUMN_MAKER;
extern const Info<bool> MAIN_GAMELIST_COLUMN_FILE_NAME;
extern const Info<bool> MAIN_GAMELIST_COLUMN_FILE_PATH;
extern const Info<bool> MAIN_GAMELIST_COLUMN_GAME_ID;
extern const Info<bool> MAIN_GAMELIST_COLUMN_REGION;
extern const Info<bool> MAIN_GAMELIST_COLUMN_FILE_SIZE;
extern const Info<bool> MAIN_GAMELIST_COLUMN_FILE_FORMAT;
extern const Info<bool> MAIN_GAMELIST_COLUMN_BLOCK_SIZE;
extern const Info<bool> MAIN_GAMELIST_COLUMN_COMPRESSION;
extern const Info<bool> MAIN_GAMELIST_COLUMN_TAGS;

// Main.FifoPlayer

extern const Info<bool> MAIN_FIFOPLAYER_LOOP_REPLAY;
extern const Info<bool> MAIN_FIFOPLAYER_EARLY_MEMORY_UPDATES;

// Main.AutoUpdate

extern const Info<std::string> MAIN_AUTOUPDATE_UPDATE_TRACK;
extern const Info<std::string> MAIN_AUTOUPDATE_HASH_OVERRIDE;

// Main.Movie

extern const Info<bool> MAIN_MOVIE_PAUSE_MOVIE;
extern const Info<std::string> MAIN_MOVIE_MOVIE_AUTHOR;
extern const Info<bool> MAIN_MOVIE_DUMP_FRAMES;
extern const Info<bool> MAIN_MOVIE_DUMP_FRAMES_SILENT;
extern const Info<bool> MAIN_MOVIE_SHOW_INPUT_DISPLAY;
extern const Info<bool> MAIN_MOVIE_SHOW_RTC;
extern const Info<bool> MAIN_MOVIE_SHOW_RERECORD;

// Main.Input

extern const Info<bool> MAIN_INPUT_BACKGROUND_INPUT;

// Main.Debug

extern const Info<bool> MAIN_DEBUG_JIT_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_LXZ_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_LWZ_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_LBZX_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_FLOATING_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_PAIRED_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_FLOATING_POINT_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_INTEGER_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_PAIRED_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_SYSTEM_REGISTERS_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_BRANCH_OFF;
extern const Info<bool> MAIN_DEBUG_JIT_REGISTER_CACHE_OFF;

// Main.BluetoothPassthrough

extern const Info<bool> MAIN_BLUETOOTH_PASSTHROUGH_ENABLED;
extern const Info<int> MAIN_BLUETOOTH_PASSTHROUGH_VID;
extern const Info<int> MAIN_BLUETOOTH_PASSTHROUGH_PID;
extern const Info<std::string> MAIN_BLUETOOTH_PASSTHROUGH_LINK_KEYS;

// Main.USBPassthrough

extern const Info<std::string> MAIN_USB_PASSTHROUGH_DEVICES;
std::set<std::pair<u16, u16>> GetUSBDeviceWhitelist();
void SetUSBDeviceWhitelist(const std::set<std::pair<u16, u16>>& devices);

// Main.EmulatedUSBDevices

extern const Info<bool> MAIN_EMULATE_SKYLANDER_PORTAL;
extern const Info<bool> MAIN_EMULATE_INFINITY_BASE;

// GameCube path utility functions

// Replaces NTSC-K with some other region, and doesn't replace non-NTSC-K regions
DiscIO::Region ToGameCubeRegion(DiscIO::Region region);

// The region argument must be valid for GameCube (i.e. must not be NTSC-K)
enum class RegionDirectoryStyle
{
  Legacy,
  Modern,
};
const char* GetDirectoryForRegion(DiscIO::Region region,
                                  RegionDirectoryStyle style = RegionDirectoryStyle::Legacy);

std::string GetBootROMPath(const std::string& region_directory);
// Builds the memory card according to the configuration with the given region and size. If the
// given region is std::nullopt, the region in the configured path is used if there is one, or the
// fallback region otherwise.
std::string GetMemcardPath(ExpansionInterface::Slot slot, std::optional<DiscIO::Region> region,
                           u16 size_mb = 0x80);
std::string GetMemcardPath(std::string configured_filename, ExpansionInterface::Slot slot,
                           std::optional<DiscIO::Region> region, u16 size_mb = 0x80);
bool IsDefaultMemcardPathConfigured(ExpansionInterface::Slot slot);
std::string GetGCIFolderPath(ExpansionInterface::Slot slot, std::optional<DiscIO::Region> region);
std::string GetGCIFolderPath(std::string configured_folder, ExpansionInterface::Slot slot,
                             std::optional<DiscIO::Region> region);
bool IsDefaultGCIFolderPathConfigured(ExpansionInterface::Slot slot);
bool AreCheatsEnabled();
bool IsDebuggingEnabled();
}  // namespace Config
