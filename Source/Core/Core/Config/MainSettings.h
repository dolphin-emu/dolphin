// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Config/Config.h"

namespace PowerPC
{
enum class CPUCore;
}

namespace AudioCommon
{
enum class DPL2Quality;
}

namespace Config
{
// Main.Core

extern const Info<bool> MAIN_SKIP_IPL;
extern const Info<bool> MAIN_LOAD_IPL_DUMP;
extern const Info<PowerPC::CPUCore> MAIN_CPU_CORE;
extern const Info<bool> MAIN_JIT_FOLLOW_BRANCH;
extern const Info<bool> MAIN_FASTMEM;
// Should really be in the DSP section, but we're kind of stuck with bad decisions made in the past.
extern const Info<bool> MAIN_DSP_HLE;
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
extern const Info<std::string> MAIN_AGP_CART_A_PATH;
extern const Info<std::string> MAIN_AGP_CART_B_PATH;
extern const Info<std::string> MAIN_GCI_FOLDER_A_PATH_OVERRIDE;
extern const Info<std::string> MAIN_GCI_FOLDER_B_PATH_OVERRIDE;
extern const Info<bool> MAIN_CODE_SYNC_OVERRIDE;
extern const Info<bool> MAIN_GCI_FOLDER_CURRENT_GAME_ONLY;
extern const Info<int> MAIN_SLOT_A;
extern const Info<int> MAIN_SLOT_B;
extern const Info<int> MAIN_SERIAL_PORT_1;
extern const Info<std::string> MAIN_BBA_MAC;
Info<u32> GetInfoForSIDevice(u32 channel);
Info<bool> GetInfoForAdapterRumble(u32 channel);
Info<bool> GetInfoForSimulateKonga(u32 channel);
extern const Info<bool> MAIN_WII_SD_CARD;
extern const Info<bool> MAIN_WII_SD_CARD_WRITABLE;
extern const Info<bool> MAIN_WII_KEYBOARD;
extern const Info<bool> MAIN_WIIMOTE_CONTINUOUS_SCANNING;
extern const Info<bool> MAIN_WIIMOTE_ENABLE_SPEAKER;
extern const Info<bool> MAIN_RUN_COMPARE_SERVER;
extern const Info<bool> MAIN_RUN_COMPARE_CLIENT;
extern const Info<bool> MAIN_MMU;
extern const Info<int> MAIN_BB_DUMP_PORT;
extern const Info<bool> MAIN_SYNC_GPU;
extern const Info<int> MAIN_SYNC_GPU_MAX_DISTANCE;
extern const Info<int> MAIN_SYNC_GPU_MIN_DISTANCE;
extern const Info<float> MAIN_SYNC_GPU_OVERCLOCK;
extern const Info<bool> MAIN_FAST_DISC_SPEED;
extern const Info<bool> MAIN_LOW_DCBZ_HACK;
extern const Info<bool> MAIN_FPRF;
extern const Info<bool> MAIN_ACCURATE_NANS;
extern const Info<float> MAIN_EMULATION_SPEED;
extern const Info<float> MAIN_OVERCLOCK;
extern const Info<bool> MAIN_OVERCLOCK_ENABLE;
extern const Info<bool> MAIN_RAM_OVERRIDE_ENABLE;
extern const Info<u32> MAIN_MEM1_SIZE;
extern const Info<u32> MAIN_MEM2_SIZE;
// Should really be part of System::GFX, but again, we're stuck with past mistakes.
extern const Info<std::string> MAIN_GFX_BACKEND;
extern const Info<std::string> MAIN_GPU_DETERMINISM_MODE;
extern const Info<std::string> MAIN_PERF_MAP_DIR;
extern const Info<bool> MAIN_CUSTOM_RTC_ENABLE;
extern const Info<u32> MAIN_CUSTOM_RTC_VALUE;
extern const Info<bool> MAIN_AUTO_DISC_CHANGE;
extern const Info<bool> MAIN_ALLOW_SD_WRITES;

// Main.DSP

extern const Info<bool> MAIN_DSP_CAPTURE_LOG;
extern const Info<bool> MAIN_DSP_JIT;
extern const Info<bool> MAIN_DUMP_AUDIO;
extern const Info<bool> MAIN_DUMP_AUDIO_SILENT;
extern const Info<bool> MAIN_DUMP_UCODE;
extern const Info<std::string> MAIN_AUDIO_BACKEND;
extern const Info<int> MAIN_AUDIO_VOLUME;

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
extern const Info<std::string> MAIN_SD_PATH;

// Main.Network

extern const Info<bool> MAIN_NETWORK_SSL_DUMP_READ;
extern const Info<bool> MAIN_NETWORK_SSL_DUMP_WRITE;
extern const Info<bool> MAIN_NETWORK_SSL_VERIFY_CERTIFICATES;
extern const Info<bool> MAIN_NETWORK_SSL_DUMP_ROOT_CA;
extern const Info<bool> MAIN_NETWORK_SSL_DUMP_PEER_CERT;

// Main.Interface

extern const Info<bool> MAIN_SKIP_NKIT_WARNING;

// Main.Debug

extern const Info<bool> MAIN_DEBUG_LOAD_STORE_FLOATING_OFF;
extern const Info<bool> MAIN_DEBUG_LOAD_STORE_PAIRED_OFF;
extern const Info<bool> MAIN_DEBUG_BRANCH_OFF;

}  // namespace Config
