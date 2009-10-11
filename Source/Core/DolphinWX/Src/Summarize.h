// Copyright (C) 2003 Dolphin Project.

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

#ifndef __SUMMARIZE_H__
#define __SUMMARIZE_H__

std::string Summarize_Plug()
{
	return StringFromFormat(
		"Plugin Information\n\n"
		"Default GFX Plugin: %s\n"
		"Default DSP Plugin: %s\n"
		"Default PAD Plugin: %s\n"
		"Default WiiMote Plugin: %s\n\n"
		"Current GFX Plugin: %s\n"
		"Current DSP Plugin: %s\n"
		"Current PAD Plugin[0]: %s\n"
		"Current PAD Plugin[1]: %s\n"
		"Current PAD Plugin[2]: %s\n"
		"Current PAD Plugin[3]: %s\n"
		"Current WiiMote Plugin[0]: %s\n",
		SConfig::GetInstance().m_DefaultGFXPlugin.c_str(),
		SConfig::GetInstance().m_DefaultDSPPlugin.c_str(),
		SConfig::GetInstance().m_DefaultPADPlugin.c_str(),
		SConfig::GetInstance().m_DefaultWiiMotePlugin.c_str(),
		Core::GetStartupParameter().m_strVideoPlugin.c_str(),
		Core::GetStartupParameter().m_strDSPPlugin.c_str(),
		Core::GetStartupParameter().m_strPadPlugin[0].c_str(),
		Core::GetStartupParameter().m_strPadPlugin[1].c_str(),
		Core::GetStartupParameter().m_strPadPlugin[2].c_str(),
		Core::GetStartupParameter().m_strPadPlugin[3].c_str(),
		Core::GetStartupParameter().m_strWiimotePlugin[0].c_str()
		);
}

std::string Summarize_Settings()
{
	return StringFromFormat(
		"Dolphin Settings\n\n"
		"HLE the IPL: %s\n"
		"Use Dynarec: %s\n"
		"Use Dual Core: %s\n"
		"DSP Thread: %s\n"
		"Skip Idle: %s\n"
		"Lock Threads: %s\n"
		"Use Dual Core: %s\n"
		"Default GCM: %s\n"
		"DVD Root: %s\n"
		"Optimize Quantizers: %s\n"
		"Enable Cheats: %s\n"
		"Selected Language: %d\n"
		"Memcard A: %s\n"
		"Memcard B: %s\n"
		"Slot A: %d\n"
		"Slot B: %d\n"
		"Serial Port 1: %d\n"
		"Run Compare Server: %s\n"
		"Run Compare Client: %s\n"
		"TLB Hack: %s\n"
		"Frame Limit: %d\n"
		"[Wii]Widescreen: %s\n"
		"[Wii]Progressive Scan: %s\n",
		Core::GetStartupParameter().bHLE_BS2?"True":"False",
		Core::GetStartupParameter().bUseJIT?"True":"False",
		Core::GetStartupParameter().bUseDualCore?"True":"False",
		Core::GetStartupParameter().bDSPThread?"True":"False",
		Core::GetStartupParameter().bSkipIdle?"True":"False",
		Core::GetStartupParameter().bLockThreads?"True":"False",
		Core::GetStartupParameter().bUseDualCore?"True":"False",
		Core::GetStartupParameter().m_strDefaultGCM.c_str(),
		Core::GetStartupParameter().m_strDVDRoot.c_str(),
		Core::GetStartupParameter().bOptimizeQuantizers?"True":"False",
		Core::GetStartupParameter().bEnableCheats?"True":"False",
		Core::GetStartupParameter().SelectedLanguage, //FIXME show language based on index
		SConfig::GetInstance().m_strMemoryCardA.c_str(),
		SConfig::GetInstance().m_strMemoryCardB.c_str(),
		SConfig::GetInstance().m_EXIDevice[0], //FIXME
		SConfig::GetInstance().m_EXIDevice[1], //FIXME
		SConfig::GetInstance().m_EXIDevice[2], //FIXME
		Core::GetStartupParameter().bRunCompareServer?"True":"False",
		Core::GetStartupParameter().bRunCompareClient?"True":"False",
		Core::GetStartupParameter().iTLBHack?"True":"False",
		SConfig::GetInstance().m_Framelimit*5,
		SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.AR")?"True":"False",
		SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.PGS")?"True":"False"
		);
}

std::string Summarize_CPU()
{
	return StringFromFormat(
		"Processor Information: \n%s\n",
		cpu_info.Summarize().c_str()
		);
}

std::string Summarize_Drives()
{
	char ** drives = cdio_get_devices();
	std::string drive;
	for (int i = 0; drives[i] != NULL && i < 24; i++)
	{

		drive += StringFromFormat(
			 "CD/DVD Drive%d: %s\n",
			 i+1,
			 drives[i]
			 );
	}
	return drive;
}

#endif //__SUMMARIZE_H__