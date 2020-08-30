// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Config/MainSettings.h"

#include <fmt/format.h>

#include "AudioCommon/AudioCommon.h"
#include "Common/Config/Config.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/PowerPC/PowerPC.h"

namespace Config
{
// Main.Core

const Info<bool> MAIN_SKIP_IPL{{System::Main, "Core", "SkipIPL"}, true};
const Info<bool> MAIN_LOAD_IPL_DUMP{{System::Main, "Core", "LoadIPLDump"}, true};
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
const Info<bool> MAIN_GCI_FOLDER_CURRENT_GAME_ONLY{
    {System::Main, "Core", "GCIFolderCurrentGameOnly"}, false};
const Info<bool> MAIN_CODE_SYNC_OVERRIDE{{System::Main, "Core", "CheatSyncOverride"}, false};
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
const Info<bool> MAIN_WII_SD_CARD_WRITABLE{{System::Main, "Core", "WiiSDCardWritable"}, true};
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
const Info<bool> MAIN_FPRF{{System::Main, "Core", "FPRF"}, false};
const Info<bool> MAIN_ACCURATE_NANS{{System::Main, "Core", "AccurateNaNs"}, false};
const Info<float> MAIN_EMULATION_SPEED{{System::Main, "Core", "EmulationSpeed"}, 1.0f};
const Info<float> MAIN_OVERCLOCK{{System::Main, "Core", "Overclock"}, 1.0f};
const Info<bool> MAIN_OVERCLOCK_ENABLE{{System::Main, "Core", "OverclockEnable"}, false};
const Info<bool> MAIN_RAM_OVERRIDE_ENABLE{{System::Main, "Core", "RAMOverrideEnable"}, false};
const Info<u32> MAIN_MEM1_SIZE{{System::Main, "Core", "MEM1Size"}, Memory::MEM1_SIZE_RETAIL};
const Info<u32> MAIN_MEM2_SIZE{{System::Main, "Core", "MEM2Size"}, Memory::MEM2_SIZE_RETAIL};
const Info<std::string> MAIN_GFX_BACKEND{{System::Main, "Core", "GFXBackend"}, ""};
const Info<std::string> MAIN_GPU_DETERMINISM_MODE{{System::Main, "Core", "GPUDeterminismMode"},
                                                  "auto"};
const Info<std::string> MAIN_PERF_MAP_DIR{{System::Main, "Core", "PerfMapDir"}, ""};
const Info<bool> MAIN_CUSTOM_RTC_ENABLE{{System::Main, "Core", "EnableCustomRTC"}, false};
// Default to seconds between 1.1.1970 and 1.1.2000
const Info<u32> MAIN_CUSTOM_RTC_VALUE{{System::Main, "Core", "CustomRTCValue"}, 946684800};
const Info<bool> MAIN_AUTO_DISC_CHANGE{{System::Main, "Core", "AutoDiscChange"}, false};
const Info<bool> MAIN_ALLOW_SD_WRITES{{System::Main, "Core", "WiiSDCardAllowWrites"}, true};

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
const Info<bool> MAIN_DISABLE_SCREENSAVER{{System::Main, "Display", "DisableScreenSaver"}, false};

// Main.DSP

const Info<bool> MAIN_DSP_CAPTURE_LOG{{System::Main, "DSP", "CaptureLog"}, false};
const Info<bool> MAIN_DSP_JIT{{System::Main, "DSP", "EnableJIT"}, true};
const Info<bool> MAIN_DUMP_AUDIO{{System::Main, "DSP", "DumpAudio"}, false};
const Info<bool> MAIN_DUMP_AUDIO_SILENT{{System::Main, "DSP", "DumpAudioSilent"}, false};
const Info<bool> MAIN_DUMP_UCODE{{System::Main, "DSP", "DumpUCode"}, false};
const Info<std::string> MAIN_AUDIO_BACKEND{{System::Main, "DSP", "Backend"},
                                           AudioCommon::GetDefaultSoundBackend()};
const Info<int> MAIN_AUDIO_VOLUME{{System::Main, "DSP", "Volume"}, 100};

// Main.General

const Info<std::string> MAIN_DUMP_PATH{{System::Main, "General", "DumpPath"}, ""};
const Info<std::string> MAIN_LOAD_PATH{{System::Main, "General", "LoadPath"}, ""};
const Info<std::string> MAIN_RESOURCEPACK_PATH{{System::Main, "General", "ResourcePackPath"}, ""};
const Info<std::string> MAIN_FS_PATH{{System::Main, "General", "NANDRootPath"}, ""};
const Info<std::string> MAIN_SD_PATH{{System::Main, "General", "WiiSDCardPath"}, ""};

// Main.Network

const Info<bool> MAIN_NETWORK_SSL_DUMP_READ{{System::Main, "Network", "SSLDumpRead"}, false};
const Info<bool> MAIN_NETWORK_SSL_DUMP_WRITE{{System::Main, "Network", "SSLDumpWrite"}, false};
const Info<bool> MAIN_NETWORK_SSL_VERIFY_CERTIFICATES{
    {System::Main, "Network", "SSLVerifyCertificates"}, true};
const Info<bool> MAIN_NETWORK_SSL_DUMP_ROOT_CA{{System::Main, "Network", "SSLDumpRootCA"}, false};
const Info<bool> MAIN_NETWORK_SSL_DUMP_PEER_CERT{{System::Main, "Network", "SSLDumpPeerCert"},
                                                 false};

// Main.Interface

const Info<bool> MAIN_SKIP_NKIT_WARNING{{System::Main, "Interface", "SkipNKitWarning"}, false};

// Main.Debug

const Info<bool> MAIN_DEBUG_LOAD_STORE_FLOATING_OFF{{System::Main, "Debug", "JitLoadStoreFloatingOff"}, false};
const Info<bool> MAIN_DEBUG_LOAD_STORE_PAIRED_OFF{{System::Main, "Debug", "JitLoadStorePairedOff"}, false};
const Info<bool> MAIN_DEBUG_BRANCH_OFF{{System::Main, "Debug", "JitBranchOff"}, false};

}  // namespace Config
