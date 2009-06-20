// Copyright (C) 2003-2008 Dolphin Project.

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




// =======================================================
// File description
// -------------
/* Purpose of this file: Collect boot settings for Core::Init()

   Call sequence: This file has one of the first function called when a game is booted,
   the boot sequence in the code is:
   
	DolphinWX:	GameListCtrl.cpp	OnActivated
				BootManager.cpp		BootCore
	Core		Core.cpp			Init		Thread creation
									EmuThread	Calls CBoot::BootUp
				Boot.cpp			CBoot::BootUp()
									CBoot::EmulatedBIOS_Wii() / GC() or Load_BIOS()
 */
// =============




///////////////////////////////////////////////////////////////////////////////////
// Includes
// ----------------
#include <string>
#include <vector>

#include "Globals.h"
#include "Common.h"
#include "IniFile.h"
#include "BootManager.h"
#include "ISOFile.h"
#include "Volume.h"
#include "VolumeCreator.h"
#include "ConfigManager.h"
#include "Core.h"
#if defined(HAVE_WX) && HAVE_WX
	#include "ConfigMain.h"
	#include "Frame.h"
	#include "CodeWindow.h"
	#include "Setup.h"
	// MusicMod
	#ifdef MUSICMOD
		#include "../../../Externals/MusicMod/Main/Src/Main.h"
	#endif
#endif


#if defined(HAVE_WX) && HAVE_WX
extern CFrame* main_frame;
extern CCodeWindow* g_pCodeWindow;
#endif

namespace BootManager
{
#ifdef _WIN32
	extern "C" HINSTANCE wxGetInstance();
#endif
/////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////
// Boot the ISO or file
// ----------------
bool BootCore(const std::string& _rFilename)
{
	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	// Use custom settings for debugging mode
	#if defined(HAVE_WX) && HAVE_WX		
		if (g_pCodeWindow)
		{
	//		StartUp.bUseDualCore = code_frame->UseDualCore();
			StartUp.bUseJIT = !g_pCodeWindow->UseInterpreter();
			StartUp.bBootToPause = g_pCodeWindow->BootToPause();
			StartUp.bAutomaticStart = g_pCodeWindow->AutomaticStart();
			StartUp.bJITUnlimitedCache = g_pCodeWindow->UnlimitedJITCache();
			StartUp.bJITBlockLinking = g_pCodeWindow->JITBlockLinking();
		}
		else
		{
	//		StartUp.bUseDualCore = false;
	//		StartUp.bUseJIT = true;
		}		
		StartUp.bEnableDebugging = g_pCodeWindow ? true : false; // RUNNING_DEBUG
	#endif 

	StartUp.m_BootType = SCoreStartupParameter::BOOT_ISO;
	StartUp.m_strFilename = _rFilename;
	SConfig::GetInstance().m_LastFilename = _rFilename;
	StartUp.bRunCompareClient = false;
	StartUp.bRunCompareServer = false;

	#ifdef _WIN32
		StartUp.hInstance = wxGetInstance();
		#ifdef _M_X64
			StartUp.bUseFastMem = true;
		#endif
	#endif

	// If for example the ISO file is bad we return here
	if (!StartUp.AutoSetup(SCoreStartupParameter::BOOT_DEFAULT)) return false;

	// ====================================================
	// Load game specific settings
	// ----------------
	// Released when you LoadDefaults
	IniFile *ini = new IniFile();
	std::string unique_id = StartUp.GetUniqueID();
	if (unique_id.size() == 6 && ini->Load((FULL_GAMECONFIG_DIR + unique_id + ".ini").c_str()))
	{
		StartUp.gameIni = ini;
		// ------------------------------------------------
		// General settings
		// ----------------
		ini->Get("Core", "UseDualCore", &StartUp.bUseDualCore, StartUp.bUseDualCore);
		ini->Get("Core", "SkipIdle", &StartUp.bSkipIdle, StartUp.bSkipIdle);
		ini->Get("Core", "OptimizeQuantizers", &StartUp.bOptimizeQuantizers, StartUp.bOptimizeQuantizers);
		ini->Get("Core", "EnableFPRF", &StartUp.bEnableFPRF, StartUp.bEnableFPRF);
		ini->Get("Core", "TLBHack", &StartUp.iTLBHack, StartUp.iTLBHack);

		// ------------------------------------------------
		// Wii settings
		// ----------------
		if (StartUp.bWii)
		{
			//bRefreshList = false;
			FILE* pStream; // file handle
			u16 IPL_PGS = 0x17CC; // progressive scan
			u16 IPL_AR = 0x04D9; // widescreen

			ini->Get("Wii", "ProgressiveScan", &StartUp.bProgressiveScan, StartUp.bProgressiveScan);
			ini->Get("Wii", "Widescreen", &StartUp.bWidescreen, StartUp.bWidescreen);

			// Save the update Wii SYSCONF settings
			pStream = NULL;
			pStream = fopen(WII_SYSCONF_FILE, "r+b");
			if (pStream != NULL)
			{
				fseek(pStream, IPL_PGS, 0);
				fputc(StartUp.bProgressiveScan ? 1 : 0, pStream);
				fseek(pStream, IPL_AR, 0);
				fputc(StartUp.bWidescreen ? 1 : 0, pStream);
				fclose(pStream);
			}	
			else
			{
				PanicAlert("Could not write to %s", WII_SYSCONF_FILE);
			}
		}
		// ---------------
	} else {
		delete ini;
		ini = NULL;
	}
	// =====================


	// =================================================================
	// Run the game
	// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#if defined(HAVE_WX) && HAVE_WX
	if(main_frame)
	{
		// Save the window handle of the eventual parent to the rendering window
		StartUp.hMainWindow = main_frame->GetRenderHandle();

		// Now that we know if we have a Wii game we can run this
		main_frame->ModifyStatusBar();
	}
#endif
	// Init the core
	if (!Core::Init())
	{
		PanicAlert("Couldn't init the core.\nCheck your configuration.");
		return false;
	}

#if defined(HAVE_WX) && HAVE_WX
	// Boot to pause or not
	Core::SetState((g_pCodeWindow && StartUp.bBootToPause)
		? Core::CORE_PAUSE : Core::CORE_RUN);
#else
	Core::SetState(Core::CORE_RUN);
#endif
	// =====================	


	// =================================================================
	// Init MusicMod
	// ¯¯¯¯¯¯¯¯¯¯
	#ifdef MUSICMOD
		MusicMod::Main(StartUp.m_strFilename);
	#endif
	// ===================


	return true;
}

void Stop()
{
	Core::Stop();
}
/////////////////////////////////


} // namespace
