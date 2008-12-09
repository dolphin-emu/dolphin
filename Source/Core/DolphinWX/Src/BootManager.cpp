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

   Call sequenc: This file has one of the first function called when a game is booted,
   the boot sequence in the code is:
   
	DolphinWX:	GameListCtrl.cpp	OnActivated
				BootManager.cpp		BootCore
	Core		Core.cpp			Init		Thread creation
									EmuThread	Calls CBoot::BootUp
				Boot.cpp			
 */
// =============





#include <string>
#include <vector>

#include "Globals.h"
#include "Common.h"
#include "IniFile.h"
#include "BootManager.h"
#include "ISOFile.h"
#include "Volume.h"
#include "VolumeCreator.h"
#include "Config.h"
#include "Core.h"
#if defined(HAVE_WX) && HAVE_WX
#include "ConfigMain.h"
#include "Frame.h"
#include "CodeWindow.h"
#endif

static std::string s_DataBasePath_EUR = "Data_EUR";
static std::string s_DataBasePath_USA = "Data_USA";
static std::string s_DataBasePath_JAP = "Data_JAP";

#if defined(HAVE_WX) && HAVE_WX
extern CFrame* main_frame;
extern CCodeWindow* g_pCodeWindow;
#endif

namespace BootManager
{
#ifdef _WIN32
extern "C" HINSTANCE wxGetInstance();
#endif

bool BootCore(const std::string& _rFilename)
{
	SCoreStartupParameter StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	// Use custom settings for debugging mode
	#if defined(HAVE_WX) && HAVE_WX		
		if (g_pCodeWindow)
		{
	//		StartUp.bUseDualCore = code_frame->UseDualCore();
			StartUp.bUseJIT = !g_pCodeWindow->UseInterpreter();
			StartUp.bAutomaticStart = g_pCodeWindow->AutomaticStart();
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
	SConfig::GetInstance().m_LastFilename = StartUp.m_strFilename;
	StartUp.bRunCompareClient = false;
	StartUp.bRunCompareServer = false;
	std::string BaseDataPath;

	#ifdef _WIN32
		StartUp.hInstance = wxGetInstance();
		#ifdef _M_X64
			StartUp.bUseFastMem = true;
		#endif
	#endif

	if ( !StartUp.AutoSetup(SCoreStartupParameter::BOOT_DEFAULT) )
	{
		return false;
	}	

	// ------------------------------------------------
	// Load game specific settings
	// ----------------
	IniFile ini;
	std::string unique_id = StartUp.GetUniqueID();
	if (unique_id.size() == 6 && ini.Load((FULL_GAMECONFIG_DIR + unique_id + ".ini").c_str()))
	{
		// ------------------------------------------------
		// General settings
		// ----------------
		ini.Get("Core", "UseDualCore", &StartUp.bUseDualCore, StartUp.bUseDualCore);
		ini.Get("Core", "SkipIdle", &StartUp.bSkipIdle, StartUp.bSkipIdle);
		ini.Get("Core", "OptimizeQuantizers", &StartUp.bOptimizeQuantizers, StartUp.bOptimizeQuantizers);


		// ------------------------------------------------
		// Update SYSCONF with game specific settings
		// ----------------
		bool bEnableProgressiveScan, bEnableWideScreen;
		//bRefreshList = false;
		FILE* pStream; // file handle
		u8 m_SYSCONF[0x4000]; // SYSCONF file
		u16 IPL_PGS = 0x17CC; // pregressive scan
		u16 IPL_AR = 0x04D9; // widescreen
		std::string FullSYSCONFPath = FULL_WII_USER_DIR "shared2/sys/SYSCONF";

		// Load Wii SYSCONF
		pStream = NULL;
		pStream = fopen(FullSYSCONFPath.c_str(), "rb");
		if (pStream != NULL)
		{
			fread(m_SYSCONF, 1, 0x4000, pStream);
			fclose(pStream);

			//wxMessageBox(wxString::Format(": %02x", m_SYSCONF[IPL_AR]));

			ini.Get("Core", "EnableProgressiveScan", &bEnableProgressiveScan, (int)m_SYSCONF[IPL_PGS]);
			ini.Get("Core", "EnableWideScreen", &bEnableWideScreen, (int)m_SYSCONF[IPL_AR]);

			m_SYSCONF[IPL_PGS] = bEnableProgressiveScan;
			m_SYSCONF[IPL_AR] = bEnableWideScreen;

			//wxMessageBox(wxString::Format(": %02x", m_SYSCONF[IPL_AR]));

			// Enable custom Wii SYSCONF settings by saving the file to shared2
			pStream = NULL;
			pStream = fopen(FullSYSCONFPath.c_str(), "wb");
			if (pStream != NULL)
			{
				fwrite(m_SYSCONF, 1, 0x4000, pStream);
				fclose(pStream);
			}	
			else
			{
				PanicAlert("Could not write to %s", FullSYSCONFPath.c_str());
			}	
			
		}
		else
		{
			PanicAlert("Could not read %s", FullSYSCONFPath.c_str());
		}
		// ---------
	}
	// ---------

	// Save some values to our local version of SCoreStartupParameter
	SConfig::GetInstance().m_LocalCoreStartupParameter.bWii = StartUp.bWii;
	SConfig::GetInstance().m_LocalCoreStartupParameter.bNTSC = StartUp.bNTSC;


#if defined(HAVE_WX) && HAVE_WX
	if(main_frame)
	{
		StartUp.hMainWindow = main_frame->GetRenderHandle();

		// Now that we know if we have a Wii game we can run this
		main_frame->ModifyStatusBar();
	}
#endif
	// init the core
	if (!Core::Init(StartUp))
	{
		PanicAlert("Couldn't init the core.\nCheck your configuration.");
		return(false);
	}

#if defined(HAVE_WX) && HAVE_WX
	// Boot to pause or not
	Core::SetState((g_pCodeWindow && !StartUp.bAutomaticStart)
		? Core::CORE_PAUSE : Core::CORE_RUN);
#else
	Core::SetState(Core::CORE_RUN);
#endif
	return(true);
}


void Stop()
{
	Core::Stop();
}
} // namespace

