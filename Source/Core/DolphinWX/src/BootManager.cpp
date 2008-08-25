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
#include "Frame.h"
#include "CodeWindow.h"

static std::string s_DataBasePath_EUR = "Data_EUR";
static std::string s_DataBasePath_USA = "Data_USA";
static std::string s_DataBasePath_JAP = "Data_JAP";

extern CFrame* main_frame;
extern CCodeWindow* g_pCodeWindow;

namespace BootManager
{
#ifdef _WIN32
extern "C" HINSTANCE wxGetInstance();
#endif

bool BootCore(const std::string& _rFilename)
{
	SCoreStartupParameter StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	if (g_pCodeWindow)
	{
//		StartUp.bUseDualCore = code_frame->UseDualCore();
		StartUp.bUseJIT = !g_pCodeWindow->UseInterpreter();
	}
	else
	{
//		StartUp.bUseDualCore = false;
		StartUp.bUseJIT = true;
	}
	StartUp.m_BootType = SCoreStartupParameter::BOOT_ISO;
	StartUp.m_strFilename = _rFilename;
	StartUp.bRunCompareClient = false;
	StartUp.bRunCompareServer = false;
	StartUp.bEnableDebugging = g_pCodeWindow ? true : false; // RUNNING_DEBUG
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

	// Load overrides
	IniFile ini;
	std::string unique_id = StartUp.GetUniqueID();
	if (unique_id.size() == 6 && ini.Load(("Patches/" + unique_id + ".ini").c_str()))
	{
		ini.Get("Core", "UseDualCore", &StartUp.bUseDualCore, StartUp.bUseDualCore);
		ini.Get("Core", "OptimizeQuantizers", &StartUp.bOptimizeQuantizers, StartUp.bOptimizeQuantizers);
	}
	if(main_frame)
		StartUp.hMainWindow = main_frame->GetRenderHandle();

	// init the core
	if (!Core::Init(StartUp))
	{
		PanicAlert("Couldn't init the core.\nCheck your configuration.");
		return(false);
	}

	Core::SetState(g_pCodeWindow ? Core::CORE_PAUSE : Core::CORE_RUN);
	return(true);
}


void Stop()
{
	Core::Stop();
}
} // namespace

