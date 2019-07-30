// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Config/MainSettings.h"

#include "AudioCommon/AudioCommon.h"
#include "Common/Config/Config.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/PowerPC/PowerPC.h"

namespace Config
{
// Main.Core

const ConfigInfo<bool> MAIN_SKIP_IPL{{System::Main, "Core", "SkipIPL"}, true};
const ConfigInfo<bool> MAIN_LOAD_IPL_DUMP{{System::Main, "Core", "LoadIPLDump"}, true};
const ConfigInfo<PowerPC::CPUCore> MAIN_CPU_CORE{{System::Main, "Core", "CPUCore"},
                                                 PowerPC::DefaultCPUCore()};
const ConfigInfo<bool> MAIN_JIT_FOLLOW_BRANCH{{System::Main, "Core", "JITFollowBranch"}, true};
const ConfigInfo<bool> MAIN_FASTMEM{{System::Main, "Core", "Fastmem"}, true};
const ConfigInfo<bool> MAIN_DSP_HLE{{System::Main, "Core", "DSPHLE"}, true};
const ConfigInfo<int> MAIN_TIMING_VARIANCE{{System::Main, "Core", "TimingVariance"}, 40};
const ConfigInfo<bool> MAIN_CPU_THREAD{{System::Main, "Core", "CPUThread"}, true};
const ConfigInfo<bool> MAIN_SYNC_ON_SKIP_IDLE{{System::Main, "Core", "SyncOnSkipIdle"}, true};
const ConfigInfo<std::string> MAIN_DEFAULT_ISO{{System::Main, "Core", "DefaultISO"}, ""};
const ConfigInfo<bool> MAIN_ENABLE_CHEATS{{System::Main, "Core", "EnableCheats"}, false};
const ConfigInfo<int> MAIN_GC_LANGUAGE{{System::Main, "Core", "SelectedLanguage"}, 0};
const ConfigInfo<bool> MAIN_OVERRIDE_GC_LANGUAGE{{System::Main, "Core", "OverrideGCLang"}, false};
const ConfigInfo<bool> MAIN_DPL2_DECODER{{System::Main, "Core", "DPL2Decoder"}, false};
const ConfigInfo<int> MAIN_AUDIO_LATENCY{{System::Main, "Core", "AudioLatency"}, 20};
const ConfigInfo<bool> MAIN_AUDIO_STRETCH{{System::Main, "Core", "AudioStretch"}, false};
const ConfigInfo<int> MAIN_AUDIO_STRETCH_LATENCY{{System::Main, "Core", "AudioStretchMaxLatency"},
                                                 80};
const ConfigInfo<std::string> MAIN_MEMCARD_A_PATH{{System::Main, "Core", "MemcardAPath"}, ""};
const ConfigInfo<std::string> MAIN_MEMCARD_B_PATH{{System::Main, "Core", "MemcardBPath"}, ""};
const ConfigInfo<std::string> MAIN_AGP_CART_A_PATH{{System::Main, "Core", "AgpCartAPath"}, ""};
const ConfigInfo<std::string> MAIN_AGP_CART_B_PATH{{System::Main, "Core", "AgpCartBPath"}, ""};
const ConfigInfo<std::string> MAIN_GCI_FOLDER_A_PATH_OVERRIDE{
    {System::Main, "Core", "GCIFolderAPathOverride"}, ""};
const ConfigInfo<std::string> MAIN_GCI_FOLDER_B_PATH_OVERRIDE{
    {System::Main, "Core", "GCIFolderBPathOverride"}, ""};
const ConfigInfo<bool> MAIN_GCI_FOLDER_CURRENT_GAME_ONLY{
    {System::Main, "Core", "GCIFolderCurrentGameOnly"}, false};
const ConfigInfo<bool> MAIN_CODE_SYNC_OVERRIDE{{System::Main, "Core", "CheatSyncOverride"}, false};
const ConfigInfo<int> MAIN_SLOT_A{{System::Main, "Core", "SlotA"},
                                  ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER};
const ConfigInfo<int> MAIN_SLOT_B{{System::Main, "Core", "SlotB"},
                                  ExpansionInterface::EXIDEVICE_NONE};
const ConfigInfo<int> MAIN_SERIAL_PORT_1{{System::Main, "Core", "SerialPort1"},
                                         ExpansionInterface::EXIDEVICE_NONE};
const ConfigInfo<std::string> MAIN_BBA_MAC{{System::Main, "Core", "BBA_MAC"}, ""};

ConfigInfo<u32> GetInfoForSIDevice(u32 channel)
{
  return {{System::Main, "Core", StringFromFormat("SIDevice%u", channel)},
          static_cast<u32>(channel == 0 ? SerialInterface::SIDEVICE_GC_CONTROLLER :
                                          SerialInterface::SIDEVICE_NONE)};
}

ConfigInfo<bool> GetInfoForAdapterRumble(u32 channel)
{
  return {{System::Main, "Core", StringFromFormat("AdapterRumble%u", channel)}, true};
}

ConfigInfo<bool> GetInfoForSimulateKonga(u32 channel)
{
  return {{System::Main, "Core", StringFromFormat("SimulateKonga%u", channel)}, false};
}

const ConfigInfo<bool> MAIN_WII_SD_CARD{{System::Main, "Core", "WiiSDCard"}, false};
const ConfigInfo<bool> MAIN_WII_SD_CARD_WRITABLE{{System::Main, "Core", "WiiSDCardWritable"}, true};
const ConfigInfo<bool> MAIN_WII_KEYBOARD{{System::Main, "Core", "WiiKeyboard"}, false};
const ConfigInfo<bool> MAIN_WIIMOTE_CONTINUOUS_SCANNING{
    {System::Main, "Core", "WiimoteContinuousScanning"}, false};
const ConfigInfo<bool> MAIN_WIIMOTE_ENABLE_SPEAKER{{System::Main, "Core", "WiimoteEnableSpeaker"},
                                                   false};
const ConfigInfo<bool> MAIN_RUN_COMPARE_SERVER{{System::Main, "Core", "RunCompareServer"}, false};
const ConfigInfo<bool> MAIN_RUN_COMPARE_CLIENT{{System::Main, "Core", "RunCompareClient"}, false};
const ConfigInfo<bool> MAIN_MMU{{System::Main, "Core", "MMU"}, false};
const ConfigInfo<int> MAIN_BB_DUMP_PORT{{System::Main, "Core", "BBDumpPort"}, -1};
const ConfigInfo<bool> MAIN_SYNC_GPU{{System::Main, "Core", "SyncGPU"}, false};
const ConfigInfo<int> MAIN_SYNC_GPU_MAX_DISTANCE{{System::Main, "Core", "SyncGpuMaxDistance"},
                                                 200000};
const ConfigInfo<int> MAIN_SYNC_GPU_MIN_DISTANCE{{System::Main, "Core", "SyncGpuMinDistance"},
                                                 -200000};
const ConfigInfo<float> MAIN_SYNC_GPU_OVERCLOCK{{System::Main, "Core", "SyncGpuOverclock"}, 1.0f};
const ConfigInfo<bool> MAIN_FAST_DISC_SPEED{{System::Main, "Core", "FastDiscSpeed"}, false};
const ConfigInfo<bool> MAIN_LOW_DCBZ_HACK{{System::Main, "Core", "LowDCBZHack"}, false};
const ConfigInfo<bool> MAIN_FPRF{{System::Main, "Core", "FPRF"}, false};
const ConfigInfo<bool> MAIN_ACCURATE_NANS{{System::Main, "Core", "AccurateNaNs"}, false};
const ConfigInfo<float> MAIN_EMULATION_SPEED{{System::Main, "Core", "EmulationSpeed"}, 1.0f};
const ConfigInfo<float> MAIN_OVERCLOCK{{System::Main, "Core", "Overclock"}, 1.0f};
const ConfigInfo<bool> MAIN_OVERCLOCK_ENABLE{{System::Main, "Core", "OverclockEnable"}, false};
const ConfigInfo<std::string> MAIN_GFX_BACKEND{{System::Main, "Core", "GFXBackend"}, ""};
const ConfigInfo<std::string> MAIN_GPU_DETERMINISM_MODE{
    {System::Main, "Core", "GPUDeterminismMode"}, "auto"};
const ConfigInfo<std::string> MAIN_PERF_MAP_DIR{{System::Main, "Core", "PerfMapDir"}, ""};
const ConfigInfo<bool> MAIN_CUSTOM_RTC_ENABLE{{System::Main, "Core", "EnableCustomRTC"}, false};
// Default to seconds between 1.1.1970 and 1.1.2000
const ConfigInfo<u32> MAIN_CUSTOM_RTC_VALUE{{System::Main, "Core", "CustomRTCValue"}, 946684800};
const ConfigInfo<bool> MAIN_ENABLE_SIGNATURE_CHECKS{{System::Main, "Core", "EnableSignatureChecks"},
                                                    true};
const ConfigInfo<bool> MAIN_REDUCE_POLLING_RATE{{System::Main, "Core", "ReducePollingRate"}, false};
const ConfigInfo<bool> MAIN_AUTO_DISC_CHANGE{{System::Main, "Core", "AutoDiscChange"}, false};

// Main.Display

const ConfigInfo<std::string> MAIN_FULLSCREEN_DISPLAY_RES{
    {System::Main, "Display", "FullscreenDisplayRes"}, "Auto"};
const ConfigInfo<bool> MAIN_FULLSCREEN{{System::Main, "Display", "Fullscreen"}, false};
const ConfigInfo<bool> MAIN_RENDER_TO_MAIN{{System::Main, "Display", "RenderToMain"}, false};
const ConfigInfo<int> MAIN_RENDER_WINDOW_XPOS{{System::Main, "Display", "RenderWindowXPos"}, -1};
const ConfigInfo<int> MAIN_RENDER_WINDOW_YPOS{{System::Main, "Display", "RenderWindowYPos"}, -1};
const ConfigInfo<int> MAIN_RENDER_WINDOW_WIDTH{{System::Main, "Display", "RenderWindowWidth"}, 640};
const ConfigInfo<int> MAIN_RENDER_WINDOW_HEIGHT{{System::Main, "Display", "RenderWindowHeight"},
                                                480};
const ConfigInfo<bool> MAIN_RENDER_WINDOW_AUTOSIZE{
    {System::Main, "Display", "RenderWindowAutoSize"}, false};
const ConfigInfo<bool> MAIN_KEEP_WINDOW_ON_TOP{{System::Main, "Display", "KeepWindowOnTop"}, false};
const ConfigInfo<bool> MAIN_DISABLE_SCREENSAVER{{System::Main, "Display", "DisableScreenSaver"},
                                                false};

// Main.DSP

const ConfigInfo<bool> MAIN_DSP_CAPTURE_LOG{{System::Main, "DSP", "CaptureLog"}, false};
const ConfigInfo<bool> MAIN_DSP_JIT{{System::Main, "DSP", "EnableJIT"}, true};
const ConfigInfo<bool> MAIN_DUMP_AUDIO{{System::Main, "DSP", "DumpAudio"}, false};
const ConfigInfo<bool> MAIN_DUMP_AUDIO_SILENT{{System::Main, "DSP", "DumpAudioSilent"}, false};
const ConfigInfo<bool> MAIN_DUMP_UCODE{{System::Main, "DSP", "DumpUCode"}, false};
const ConfigInfo<std::string> MAIN_AUDIO_BACKEND{{System::Main, "DSP", "Backend"},
                                                 AudioCommon::GetDefaultSoundBackend()};
const ConfigInfo<int> MAIN_AUDIO_VOLUME{{System::Main, "DSP", "Volume"}, 100};

// Main.General

const ConfigInfo<std::string> MAIN_DUMP_PATH{{System::Main, "General", "DumpPath"}, ""};
const ConfigInfo<std::string> MAIN_FS_PATH{{System::Main, "General", "NANDRootPath"}, ""};
const ConfigInfo<std::string> MAIN_SD_PATH{{System::Main, "General", "WiiSDCardPath"}, ""};

// Main.Controls

const ConfigInfo<int> MAIN_IR_PITCH{{System::Main, "Controls", "IRTotalPitch"}, 15};
const ConfigInfo<int> MAIN_IR_YAW{{System::Main, "Controls", "IRTotalYaw"}, 15};
const ConfigInfo<int> MAIN_IR_VERTICAL_OFFSET{{System::Main, "Controls", "IRVerticalOffset"}, 10};

// Main.Interface

const ConfigInfo<std::string> MAIN_SYSTEM_BACK_BIND{{System::Main, "Interface", "SystemBackBind"}, ""};

}  // namespace Config
