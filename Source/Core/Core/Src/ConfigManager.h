// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _CONFIGMANAGER_H
#define _CONFIGMANAGER_H

#include <string>
#include <vector>

#include "Boot/Boot.h"
#include "HW/EXI_Device.h"
#include "HW/SI_Device.h"
#include "SysConf.h"

// DSP Backend Types
#define BACKEND_NULLSOUND	_trans("No audio output")
#define BACKEND_ALSA		"ALSA"
#define BACKEND_AOSOUND		"AOSound"
#define BACKEND_COREAUDIO	"CoreAudio"
#define BACKEND_DIRECTSOUND	"DSound"
#define BACKEND_OPENAL		"OpenAL"
#define BACKEND_PULSEAUDIO	"Pulse"
#define BACKEND_XAUDIO2		"XAudio2"
#define BACKEND_OPENSLES	"OpenSLES"
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
	bool b_UseFPS;
	// other interface settings
	bool m_InterfaceToolbar;
	bool m_InterfaceStatusbar;
	bool m_InterfaceLogWindow;
	bool m_InterfaceLogConfigWindow;
	bool m_InterfaceConsole;

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

	std::string m_WirelessMac;
	bool m_PauseMovie;
	bool m_ShowLag;
	std::string m_strMovieAuthor;

	// DSP settings
	bool m_EnableJIT;
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

#endif // endif config manager

