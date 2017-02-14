// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// File description
// -------------
// Purpose of this file: Collect boot settings for Core::Init()

// Call sequence: This file has one of the first function called when a game is
// booted,
// the boot sequence in the code is:

// DolphinWX:    FrameTools.cpp         StartGame
// Core          BootManager.cpp        BootCore
//               Core.cpp               Init                     Thread creation
//                                      EmuThread                Calls
//                                      CBoot::BootUp
//               Boot.cpp               CBoot::BootUp()
//                                      CBoot::EmulatedBS2_Wii() / GC() or
//                                      Load_BS2()

// Includes
// ----------------
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"

#include "DiscIO/Enums.h"

#include "VideoCommon/VideoBackendBase.h"

namespace BootManager
{
// Boot the ISO or file
bool BootCore(const std::string& _rFilename)
{
  SConfig& StartUp = SConfig::GetInstance();

  // Use custom settings for debugging mode
  Host_SetStartupDebuggingParameters();

  StartUp.m_BootType = SConfig::BOOT_ISO;
  StartUp.m_strFilename = _rFilename;
  StartUp.m_LastFilename = _rFilename;
  StartUp.bRunCompareClient = false;
  StartUp.bRunCompareServer = false;

  // If for example the ISO file is bad we return here
  if (!StartUp.AutoSetup(SConfig::BOOT_DEFAULT))
    return false;

  if (!NetPlay::IsNetPlayRunning())
    g_SRAM_netplay_initialized = false;

  const bool ntsc = DiscIO::IsNTSC(StartUp.m_region);

  // Apply overrides
  // Some NTSC GameCube games such as Baten Kaitos react strangely to
  // language settings that would be invalid on an NTSC system
  if (!StartUp.bOverrideGCLanguage && ntsc)
    StartUp.SelectedLanguage = 0;

  // Some NTSC Wii games such as Doc Louis's Punch-Out!! and
  // 1942 (Virtual Console) crash if the PAL60 option is enabled
  if (StartUp.bWii && ntsc)
    StartUp.bPAL60 = false;

  if (StartUp.bWii)
    StartUp.SaveSettingsToSysconf();

  // Run the game
  // Init the core
  if (!Core::Init())
  {
    PanicAlertT("Couldn't init the core.\nCheck your configuration.");
    return false;
  }

  return true;
}

void Stop()
{
  Core::Stop();
  RestoreConfig();
}

void RestoreConfig()
{
  SConfig::GetInstance().LoadSettingsFromSysconf();
  SConfig::GetInstance().m_strGameID = "00000000";
  SConfig::GetInstance().m_title_id = 0;
}

}  // namespace
