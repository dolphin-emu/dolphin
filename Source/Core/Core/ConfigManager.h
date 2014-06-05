// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/SysConf.h"
#include "Core/Boot/Boot.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/SI_Device.h"

// DSP Backend Types
#define BACKEND_NULLSOUND   _trans("No audio output")
#define BACKEND_ALSA        "ALSA"
#define BACKEND_AOSOUND     "AOSound"
#define BACKEND_COREAUDIO   "CoreAudio"
#define BACKEND_DIRECTSOUND "DSound"
#define BACKEND_OPENAL      "OpenAL"
#define BACKEND_PULSEAUDIO  "Pulse"
#define BACKEND_XAUDIO2     "XAudio2"
#define BACKEND_OPENSLES    "OpenSLES"
struct SConfig : NonCopyable
{
	// Wii Devices
	bool m_WiiSDCard;
	bool m_WiiKeyboard;
	bool m_WiimoteContinuousScanning;
	bool m_WiimoteEnableSpeaker;

	// name of the last used filename
	std::string m_LastFilename;

	// gcm folder
	std::vector<std::string> m_ISOFolder;
	bool m_RecursiveISOFolder;

	SCoreStartupParameter m_LocalCoreStartupParameter;
	std::string m_NANDPath;

	std::string m_strMemoryCardA;
	std::string m_strMemoryCardB;
	TEXIDevices m_EXIDevice[3];
	SIDevices m_SIDevice[4];
	std::string m_bba_mac;

	// interface language
	int m_InterfaceLanguage;
	// framelimit choose
	unsigned int m_Framelimit;
	// other interface settings
	bool m_InterfaceToolbar;
	bool m_InterfaceStatusbar;
	bool m_InterfaceLogWindow;
	bool m_InterfaceLogConfigWindow;
	bool m_InterfaceExtendedFPSInfo;

	bool m_ListDrives;
	bool m_ListWad;
	bool m_ListWii;
	bool m_ListGC;
	bool m_ListPal;
	bool m_ListUsa;
	bool m_ListJap;
	bool m_ListFrance;
	bool m_ListItaly;
	bool m_ListKorea;
	bool m_ListTaiwan;
	bool m_ListUnknown;
	int m_ListSort;
	int m_ListSort2;

	// Game list column toggles
	bool m_showSystemColumn;
	bool m_showBannerColumn;
	bool m_showNotesColumn;
	bool m_showIDColumn;
	bool m_showRegionColumn;
	bool m_showSizeColumn;
	bool m_showStateColumn;

	// Toggles whether compressed titles show up in blue in the game list
	bool m_ColorCompressed;

	std::string m_WirelessMac;
	bool m_PauseMovie;
	bool m_ShowLag;
	std::string m_strMovieAuthor;
	unsigned int m_FrameSkip;

	// DSP settings
	bool m_DSPEnableJIT;
	bool m_DumpAudio;
	int m_Volume;
	std::string sBackend;

	SysConf* m_SYSCONF;

	// save settings
	void SaveSettings();

	// load settings
	void LoadSettings();

	// Return the permanent and somewhat globally used instance of this struct
	static SConfig& GetInstance() {return(*m_Instance);}

	static void Init();
	static void Shutdown();

private:
	SConfig();
	~SConfig();

	static SConfig* m_Instance;
};
