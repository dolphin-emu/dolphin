// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/MainSettings.h"

#include <fmt/format.h>

#include "AudioCommon/AudioCommon.h"
#include "Common/Config/Config.h"
#include "Common/Version.h"
#include "Core/Config/DefaultLocale.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/PowerPC/PowerPC.h"
#include "DiscIO/Enums.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Config
{
// Main.Core

const Info<bool> MAIN_SKIP_IPL{{System::Main, "Core", "SkipIPL"}, true};
const Info<PowerPC::CPUCore> MAIN_CPU_CORE{{System::Main, "Core", "CPUCore"},
                                           PowerPC::DefaultCPUCore()};
const Info<bool> MAIN_JIT_FOLLOW_BRANCH{{System::Main, "Core", "JITFollowBranch"}, true};
const Info<bool> MAIN_FASTMEM{{System::Main, "Core", "Fastmem"}, true};
const Info<bool> MAIN_DSP_HLE{{System::Main, "Core", "DSPHLE"}, true};
const Info<int> MAIN_TIMING_VARIANCE{{System::Main, "Core", "TimingVariance"}, 40};
const Info<bool> MAIN_CPU_THREAD{{System::Main, "Core", "CPUThread"}, true};
const Info<bool> MAIN_SYNC_ON_SKIP_IDLE{{System::Main, "Core", "SyncOnSkipIdle"}, true};
const Info<std::string> MAIN_DEFAULT_ISO{{System::Main, "Core", "DefaultISO"}, ""};
const Info<bool> MAIN_ENABLE_CHEATS{{System::Main, "Core", "EnableCheats"}, false};
const Info<int> MAIN_GC_LANGUAGE{{System::Main, "Core", "SelectedLanguage"}, 0};
const Info<bool> MAIN_OVERRIDE_REGION_SETTINGS{{System::Main, "Core", "OverrideRegionSettings"},
                                               false};
const Info<bool> MAIN_DPL2_DECODER{{System::Main, "Core", "DPL2Decoder"}, false};
const Info<AudioCommon::DPL2Quality> MAIN_DPL2_QUALITY{{System::Main, "Core", "DPL2Quality"},
                                                       AudioCommon::GetDefaultDPL2Quality()};
const Info<int> MAIN_AUDIO_LATENCY{{System::Main, "Core", "AudioLatency"}, 20};
const Info<bool> MAIN_AUDIO_STRETCH{{System::Main, "Core", "AudioStretch"}, false};
const Info<int> MAIN_AUDIO_STRETCH_LATENCY{{System::Main, "Core", "AudioStretchMaxLatency"}, 80};
const Info<std::string> MAIN_MEMCARD_A_PATH{{System::Main, "Core", "MemcardAPath"}, ""};
const Info<std::string> MAIN_MEMCARD_B_PATH{{System::Main, "Core", "MemcardBPath"}, ""};
const Info<std::string> MAIN_AGP_CART_A_PATH{{System::Main, "Core", "AgpCartAPath"}, ""};
const Info<std::string> MAIN_AGP_CART_B_PATH{{System::Main, "Core", "AgpCartBPath"}, ""};
const Info<std::string> MAIN_GCI_FOLDER_A_PATH_OVERRIDE{
    {System::Main, "Core", "GCIFolderAPathOverride"}, ""};
const Info<std::string> MAIN_GCI_FOLDER_B_PATH_OVERRIDE{
    {System::Main, "Core", "GCIFolderBPathOverride"}, ""};
const Info<int> MAIN_SLOT_A{{System::Main, "Core", "SlotA"},
                            ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER};
const Info<int> MAIN_SLOT_B{{System::Main, "Core", "SlotB"}, ExpansionInterface::EXIDEVICE_NONE};
const Info<int> MAIN_SERIAL_PORT_1{{System::Main, "Core", "SerialPort1"},
                                   ExpansionInterface::EXIDEVICE_NONE};
const Info<std::string> MAIN_BBA_MAC{{System::Main, "Core", "BBA_MAC"}, ""};

Info<u32> GetInfoForSIDevice(u32 channel)
{
  return {{System::Main, "Core", fmt::format("SIDevice{}", channel)},
          static_cast<u32>(channel == 0 ? SerialInterface::SIDEVICE_GC_CONTROLLER :
                                          SerialInterface::SIDEVICE_NONE)};
}

Info<bool> GetInfoForAdapterRumble(u32 channel)
{
  return {{System::Main, "Core", fmt::format("AdapterRumble{}", channel)}, true};
}

Info<bool> GetInfoForSimulateKonga(u32 channel)
{
  return {{System::Main, "Core", fmt::format("SimulateKonga{}", channel)}, false};
}

const Info<bool> MAIN_WII_SD_CARD{{System::Main, "Core", "WiiSDCard"}, true};
const Info<bool> MAIN_WII_KEYBOARD{{System::Main, "Core", "WiiKeyboard"}, false};
const Info<bool> MAIN_WIIMOTE_CONTINUOUS_SCANNING{
    {System::Main, "Core", "WiimoteContinuousScanning"}, false};
const Info<bool> MAIN_WIIMOTE_ENABLE_SPEAKER{{System::Main, "Core", "WiimoteEnableSpeaker"}, false};
const Info<bool> MAIN_RUN_COMPARE_SERVER{{System::Main, "Core", "RunCompareServer"}, false};
const Info<bool> MAIN_RUN_COMPARE_CLIENT{{System::Main, "Core", "RunCompareClient"}, false};
const Info<bool> MAIN_MMU{{System::Main, "Core", "MMU"}, false};
const Info<int> MAIN_BB_DUMP_PORT{{System::Main, "Core", "BBDumpPort"}, -1};
const Info<bool> MAIN_SYNC_GPU{{System::Main, "Core", "SyncGPU"}, false};
const Info<int> MAIN_SYNC_GPU_MAX_DISTANCE{{System::Main, "Core", "SyncGpuMaxDistance"}, 200000};
const Info<int> MAIN_SYNC_GPU_MIN_DISTANCE{{System::Main, "Core", "SyncGpuMinDistance"}, -200000};
const Info<float> MAIN_SYNC_GPU_OVERCLOCK{{System::Main, "Core", "SyncGpuOverclock"}, 1.0f};
const Info<bool> MAIN_FAST_DISC_SPEED{{System::Main, "Core", "FastDiscSpeed"}, false};
const Info<bool> MAIN_LOW_DCBZ_HACK{{System::Main, "Core", "LowDCBZHack"}, false};
const Info<bool> MAIN_FLOAT_EXCEPTIONS{{System::Main, "Core", "FloatExceptions"}, false};
const Info<bool> MAIN_DIVIDE_BY_ZERO_EXCEPTIONS{{System::Main, "Core", "DivByZeroExceptions"},
                                                false};
const Info<bool> MAIN_FPRF{{System::Main, "Core", "FPRF"}, false};
const Info<bool> MAIN_ACCURATE_NANS{{System::Main, "Core", "AccurateNaNs"}, false};
const Info<bool> MAIN_DISABLE_ICACHE{{System::Main, "Core", "DisableICache"}, false};
const Info<float> MAIN_EMULATION_SPEED{{System::Main, "Core", "EmulationSpeed"}, 1.0f};
const Info<float> MAIN_OVERCLOCK{{System::Main, "Core", "Overclock"}, 1.0f};
const Info<bool> MAIN_OVERCLOCK_ENABLE{{System::Main, "Core", "OverclockEnable"}, false};
const Info<bool> MAIN_RAM_OVERRIDE_ENABLE{{System::Main, "Core", "RAMOverrideEnable"}, false};
const Info<u32> MAIN_MEM1_SIZE{{System::Main, "Core", "MEM1Size"}, Memory::MEM1_SIZE_RETAIL};
const Info<u32> MAIN_MEM2_SIZE{{System::Main, "Core", "MEM2Size"}, Memory::MEM2_SIZE_RETAIL};
const Info<std::string> MAIN_GFX_BACKEND{{System::Main, "Core", "GFXBackend"},
                                         VideoBackendBase::GetDefaultBackendName()};
const Info<std::string> MAIN_GPU_DETERMINISM_MODE{{System::Main, "Core", "GPUDeterminismMode"},
                                                  "auto"};
const Info<std::string> MAIN_PERF_MAP_DIR{{System::Main, "Core", "PerfMapDir"}, ""};
const Info<bool> MAIN_CUSTOM_RTC_ENABLE{{System::Main, "Core", "EnableCustomRTC"}, false};
// Default to seconds between 1.1.1970 and 1.1.2000
const Info<u32> MAIN_CUSTOM_RTC_VALUE{{System::Main, "Core", "CustomRTCValue"}, 946684800};
const Info<DiscIO::Region> MAIN_FALLBACK_REGION{{System::Main, "Core", "FallbackRegion"},
                                                GetDefaultRegion()};
const Info<bool> MAIN_AUTO_DISC_CHANGE{{System::Main, "Core", "AutoDiscChange"}, false};
const Info<bool> MAIN_ALLOW_SD_WRITES{{System::Main, "Core", "WiiSDCardAllowWrites"}, true};
const Info<bool> MAIN_ENABLE_SAVESTATES{{System::Main, "Core", "EnableSaveStates"}, false};
const Info<bool> MAIN_REAL_WII_REMOTE_REPEAT_REPORTS{
    {System::Main, "Core", "RealWiiRemoteRepeatReports"}, true};

// Main.Display

const Info<std::string> MAIN_FULLSCREEN_DISPLAY_RES{
    {System::Main, "Display", "FullscreenDisplayRes"}, "Auto"};
const Info<bool> MAIN_FULLSCREEN{{System::Main, "Display", "Fullscreen"}, false};
const Info<bool> MAIN_RENDER_TO_MAIN{{System::Main, "Display", "RenderToMain"}, false};
const Info<int> MAIN_RENDER_WINDOW_XPOS{{System::Main, "Display", "RenderWindowXPos"}, -1};
const Info<int> MAIN_RENDER_WINDOW_YPOS{{System::Main, "Display", "RenderWindowYPos"}, -1};
const Info<int> MAIN_RENDER_WINDOW_WIDTH{{System::Main, "Display", "RenderWindowWidth"}, 640};
const Info<int> MAIN_RENDER_WINDOW_HEIGHT{{System::Main, "Display", "RenderWindowHeight"}, 480};
const Info<bool> MAIN_RENDER_WINDOW_AUTOSIZE{{System::Main, "Display", "RenderWindowAutoSize"},
                                             false};
const Info<bool> MAIN_KEEP_WINDOW_ON_TOP{{System::Main, "Display", "KeepWindowOnTop"}, false};
const Info<bool> MAIN_DISABLE_SCREENSAVER{{System::Main, "Display", "DisableScreenSaver"}, true};

// Main.DSP

const Info<bool> MAIN_DSP_THREAD{{System::Main, "DSP", "DSPThread"}, false};
const Info<bool> MAIN_DSP_CAPTURE_LOG{{System::Main, "DSP", "CaptureLog"}, false};
const Info<bool> MAIN_DSP_JIT{{System::Main, "DSP", "EnableJIT"}, true};
const Info<bool> MAIN_DUMP_AUDIO{{System::Main, "DSP", "DumpAudio"}, false};
const Info<bool> MAIN_DUMP_AUDIO_SILENT{{System::Main, "DSP", "DumpAudioSilent"}, false};
const Info<bool> MAIN_DUMP_UCODE{{System::Main, "DSP", "DumpUCode"}, false};
const Info<std::string> MAIN_AUDIO_BACKEND{{System::Main, "DSP", "Backend"},
                                           AudioCommon::GetDefaultSoundBackend()};
const Info<int> MAIN_AUDIO_VOLUME{{System::Main, "DSP", "Volume"}, 100};
const Info<bool> MAIN_AUDIO_MUTED{{System::Main, "DSP", "Muted"}, false};
#ifdef _WIN32
const Info<std::string> MAIN_WASAPI_DEVICE{{System::Main, "DSP", "WASAPIDevice"}, "Default"};
#endif

bool ShouldUseDPL2Decoder()
{
  return Get(MAIN_DPL2_DECODER) && !Get(MAIN_DSP_HLE);
}

// Main.General

const Info<std::string> MAIN_DUMP_PATH{{System::Main, "General", "DumpPath"}, ""};
const Info<std::string> MAIN_LOAD_PATH{{System::Main, "General", "LoadPath"}, ""};
const Info<std::string> MAIN_RESOURCEPACK_PATH{{System::Main, "General", "ResourcePackPath"}, ""};
const Info<std::string> MAIN_FS_PATH{{System::Main, "General", "NANDRootPath"}, ""};
const Info<std::string> MAIN_SD_PATH{{System::Main, "General", "WiiSDCardPath"}, ""};
const Info<std::string> MAIN_WFS_PATH{{System::Main, "General", "WFSPath"}, ""};

// Main.GBA

const Info<std::string> MAIN_GBA_BIOS_PATH{{System::Main, "GBA", "BIOS"}, ""};
const std::array<Info<std::string>, 4> MAIN_GBA_ROM_PATHS{
    Info<std::string>{{System::Main, "GBA", "Rom1"}, ""},
    Info<std::string>{{System::Main, "GBA", "Rom2"}, ""},
    Info<std::string>{{System::Main, "GBA", "Rom3"}, ""},
    Info<std::string>{{System::Main, "GBA", "Rom4"}, ""}};
const Info<std::string> MAIN_GBA_SAVES_PATH{{System::Main, "GBA", "SavesPath"}, ""};
const Info<bool> MAIN_GBA_SAVES_IN_ROM_PATH{{System::Main, "GBA", "SavesInRomPath"}, false};
const Info<bool> MAIN_GBA_THREADS{{System::Main, "GBA", "Threads"}, true};

// Main.Network

const Info<bool> MAIN_NETWORK_SSL_DUMP_READ{{System::Main, "Network", "SSLDumpRead"}, false};
const Info<bool> MAIN_NETWORK_SSL_DUMP_WRITE{{System::Main, "Network", "SSLDumpWrite"}, false};
const Info<bool> MAIN_NETWORK_SSL_VERIFY_CERTIFICATES{
    {System::Main, "Network", "SSLVerifyCertificates"}, true};
const Info<bool> MAIN_NETWORK_SSL_DUMP_ROOT_CA{{System::Main, "Network", "SSLDumpRootCA"}, false};
const Info<bool> MAIN_NETWORK_SSL_DUMP_PEER_CERT{{System::Main, "Network", "SSLDumpPeerCert"},
                                                 false};
const Info<bool> MAIN_NETWORK_DUMP_AS_PCAP{{System::Main, "Network", "DumpAsPCAP"}, false};
// Default value based on:
//  - [RFC 1122] 4.2.3.5 TCP Connection Failures (at least 3 minutes)
//  - https://dolp.in/pr8759 hwtest (3 minutes and 10 seconds)
const Info<int> MAIN_NETWORK_TIMEOUT{{System::Main, "Network", "NetworkTimeout"}, 190};

// Main.Interface

const Info<bool> MAIN_USE_HIGH_CONTRAST_TOOLTIPS{
    {System::Main, "Interface", "UseHighContrastTooltips"}, true};
const Info<bool> MAIN_USE_PANIC_HANDLERS{{System::Main, "Interface", "UsePanicHandlers"}, true};
const Info<bool> MAIN_ABORT_ON_PANIC_ALERT{{System::Main, "Interface", "AbortOnPanicAlert"}, false};
const Info<bool> MAIN_OSD_MESSAGES{{System::Main, "Interface", "OnScreenDisplayMessages"}, true};
const Info<bool> MAIN_SKIP_NKIT_WARNING{{System::Main, "Interface", "SkipNKitWarning"}, false};

// Main.Analytics

const Info<std::string> MAIN_ANALYTICS_ID{{System::Main, "Analytics", "ID"}, ""};
const Info<bool> MAIN_ANALYTICS_ENABLED{{System::Main, "Analytics", "Enabled"}, false};
const Info<bool> MAIN_ANALYTICS_PERMISSION_ASKED{{System::Main, "Analytics", "PermissionAsked"},
                                                 false};

// Main.GameList

const Info<bool> MAIN_GAMELIST_LIST_DRIVES{{System::Main, "GameList", "ListDrives"}, false};
const Info<bool> MAIN_GAMELIST_LIST_WAD{{System::Main, "GameList", "ListWad"}, true};
const Info<bool> MAIN_GAMELIST_LIST_ELF_DOL{{System::Main, "GameList", "ListElfDol"}, true};
const Info<bool> MAIN_GAMELIST_LIST_WII{{System::Main, "GameList", "ListWii"}, true};
const Info<bool> MAIN_GAMELIST_LIST_GC{{System::Main, "GameList", "ListGC"}, true};
const Info<bool> MAIN_GAMELIST_LIST_JPN{{System::Main, "GameList", "ListJap"}, true};
const Info<bool> MAIN_GAMELIST_LIST_PAL{{System::Main, "GameList", "ListPal"}, true};
const Info<bool> MAIN_GAMELIST_LIST_USA{{System::Main, "GameList", "ListUsa"}, true};
const Info<bool> MAIN_GAMELIST_LIST_AUSTRALIA{{System::Main, "GameList", "ListAustralia"}, true};
const Info<bool> MAIN_GAMELIST_LIST_FRANCE{{System::Main, "GameList", "ListFrance"}, true};
const Info<bool> MAIN_GAMELIST_LIST_GERMANY{{System::Main, "GameList", "ListGermany"}, true};
const Info<bool> MAIN_GAMELIST_LIST_ITALY{{System::Main, "GameList", "ListItaly"}, true};
const Info<bool> MAIN_GAMELIST_LIST_KOREA{{System::Main, "GameList", "ListKorea"}, true};
const Info<bool> MAIN_GAMELIST_LIST_NETHERLANDS{{System::Main, "GameList", "ListNetherlands"},
                                                true};
const Info<bool> MAIN_GAMELIST_LIST_RUSSIA{{System::Main, "GameList", "ListRussia"}, true};
const Info<bool> MAIN_GAMELIST_LIST_SPAIN{{System::Main, "GameList", "ListSpain"}, true};
const Info<bool> MAIN_GAMELIST_LIST_TAIWAN{{System::Main, "GameList", "ListTaiwan"}, true};
const Info<bool> MAIN_GAMELIST_LIST_WORLD{{System::Main, "GameList", "ListWorld"}, true};
const Info<bool> MAIN_GAMELIST_LIST_UNKNOWN{{System::Main, "GameList", "ListUnknown"}, true};
const Info<int> MAIN_GAMELIST_LIST_SORT{{System::Main, "GameList", "ListSort"}, 3};
const Info<int> MAIN_GAMELIST_LIST_SORT_SECONDARY{{System::Main, "GameList", "ListSortSecondary"},
                                                  0};
const Info<bool> MAIN_GAMELIST_COLUMN_PLATFORM{{System::Main, "GameList", "ColumnPlatform"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_DESCRIPTION{{System::Main, "GameList", "ColumnDescription"},
                                                  false};
const Info<bool> MAIN_GAMELIST_COLUMN_BANNER{{System::Main, "GameList", "ColumnBanner"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_TITLE{{System::Main, "GameList", "ColumnTitle"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_MAKER{{System::Main, "GameList", "ColumnNotes"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_FILE_NAME{{System::Main, "GameList", "ColumnFileName"},
                                                false};
const Info<bool> MAIN_GAMELIST_COLUMN_FILE_PATH{{System::Main, "GameList", "ColumnFilePath"},
                                                false};
const Info<bool> MAIN_GAMELIST_COLUMN_GAME_ID{{System::Main, "GameList", "ColumnID"}, false};
const Info<bool> MAIN_GAMELIST_COLUMN_REGION{{System::Main, "GameList", "ColumnRegion"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_FILE_SIZE{{System::Main, "GameList", "ColumnSize"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_FILE_FORMAT{{System::Main, "GameList", "ColumnFileFormat"},
                                                  false};
const Info<bool> MAIN_GAMELIST_COLUMN_BLOCK_SIZE{{System::Main, "GameList", "ColumnBlockSize"},
                                                 false};
const Info<bool> MAIN_GAMELIST_COLUMN_COMPRESSION{{System::Main, "GameList", "ColumnCompression"},
                                                  false};
const Info<bool> MAIN_GAMELIST_COLUMN_TAGS{{System::Main, "GameList", "ColumnTags"}, false};

// Main.FifoPlayer

const Info<bool> MAIN_FIFOPLAYER_LOOP_REPLAY{{System::Main, "FifoPlayer", "LoopReplay"}, true};

// Main.AutoUpdate

const Info<std::string> MAIN_AUTOUPDATE_UPDATE_TRACK{{System::Main, "AutoUpdate", "UpdateTrack"},
                                                     Common::scm_update_track_str};
const Info<std::string> MAIN_AUTOUPDATE_HASH_OVERRIDE{{System::Main, "AutoUpdate", "HashOverride"},
                                                      ""};

// Main.Movie

const Info<bool> MAIN_MOVIE_PAUSE_MOVIE{{System::Main, "Movie", "PauseMovie"}, false};
const Info<std::string> MAIN_MOVIE_MOVIE_AUTHOR{{System::Main, "Movie", "Author"}, ""};
const Info<bool> MAIN_MOVIE_DUMP_FRAMES{{System::Main, "Movie", "DumpFrames"}, false};
const Info<bool> MAIN_MOVIE_DUMP_FRAMES_SILENT{{System::Main, "Movie", "DumpFramesSilent"}, false};
const Info<bool> MAIN_MOVIE_SHOW_INPUT_DISPLAY{{System::Main, "Movie", "ShowInputDisplay"}, false};
const Info<bool> MAIN_MOVIE_SHOW_RTC{{System::Main, "Movie", "ShowRTC"}, false};
const Info<bool> MAIN_MOVIE_SHOW_RERECORD{{System::Main, "Movie", "ShowRerecord"}, false};

// Main.Input

const Info<bool> MAIN_INPUT_BACKGROUND_INPUT{{System::Main, "Input", "BackgroundInput"}, false};

// Main.Debug

const Info<bool> MAIN_DEBUG_JIT_OFF{{System::Main, "Debug", "JitOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_OFF{{System::Main, "Debug", "JitLoadStoreOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_LXZ_OFF{{System::Main, "Debug", "JitLoadStorelXzOff"},
                                                   false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_LWZ_OFF{{System::Main, "Debug", "JitLoadStorelwzOff"},
                                                   false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_LBZX_OFF{{System::Main, "Debug", "JitLoadStorelbzxOff"},
                                                    false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_FLOATING_OFF{
    {System::Main, "Debug", "JitLoadStoreFloatingOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_PAIRED_OFF{
    {System::Main, "Debug", "JitLoadStorePairedOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_FLOATING_POINT_OFF{{System::Main, "Debug", "JitFloatingPointOff"},
                                                   false};
const Info<bool> MAIN_DEBUG_JIT_INTEGER_OFF{{System::Main, "Debug", "JitIntegerOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_PAIRED_OFF{{System::Main, "Debug", "JitPairedOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_SYSTEM_REGISTERS_OFF{
    {System::Main, "Debug", "JitSystemRegistersOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_BRANCH_OFF{{System::Main, "Debug", "JitBranchOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_REGISTER_CACHE_OFF{{System::Main, "Debug", "JitRegisterCacheOff"},
                                                   false};

// Main.BluetoothPassthrough

const Info<bool> MAIN_BLUETOOTH_PASSTHROUGH_ENABLED{
    {System::Main, "BluetoothPassthrough", "Enabled"}, false};
const Info<int> MAIN_BLUETOOTH_PASSTHROUGH_VID{{System::Main, "BluetoothPassthrough", "VID"}, -1};
const Info<int> MAIN_BLUETOOTH_PASSTHROUGH_PID{{System::Main, "BluetoothPassthrough", "PID"}, -1};
const Info<std::string> MAIN_BLUETOOTH_PASSTHROUGH_LINK_KEYS{
    {System::Main, "BluetoothPassthrough", "LinkKeys"}, ""};

}  // namespace Config
