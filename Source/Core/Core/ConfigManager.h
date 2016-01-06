// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "Common/NonCopyable.h"
#include "Common/SysConf.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/SI_Device.h"
#include "DiscIO/Volume.h"

// DSP Backend Types
#define BACKEND_NULLSOUND   _trans("No audio output")
#define BACKEND_ALSA        "ALSA"
#define BACKEND_AOSOUND     "AOSound"
#define BACKEND_COREAUDIO   "CoreAudio"
#define BACKEND_OPENAL      "OpenAL"
#define BACKEND_PULSEAUDIO  "Pulse"
#define BACKEND_XAUDIO2     "XAudio2"
#define BACKEND_OPENSLES    "OpenSLES"

enum GPUDeterminismMode
{
	GPU_DETERMINISM_AUTO,
	GPU_DETERMINISM_NONE,
	// This is currently the only mode.  There will probably be at least
	// one more at some point.
	GPU_DETERMINISM_FAKE_COMPLETION,
};

struct SConfig : NonCopyable
{
	// Wii Devices
	bool m_WiiSDCard;
	bool m_WiiKeyboard;
	bool m_WiimoteContinuousScanning;
	bool m_WiimoteEnableSpeaker;

	// name of the last used filename
	std::string m_LastFilename;

	// ISO folder
	std::vector<std::string> m_ISOFolder;
	bool m_RecursiveISOFolder;

	// Settings
	bool bEnableDebugging;
	#ifdef USE_GDBSTUB
	int iGDBPort;
	#ifndef _WIN32
	std::string gdb_socket;
	#endif
	#endif
	bool bAutomaticStart;
	bool bBootToPause;

	int iCPUCore;

	// JIT (shared between JIT and JITIL)
	bool bJITNoBlockCache, bJITNoBlockLinking;
	bool bJITOff;
	bool bJITLoadStoreOff, bJITLoadStorelXzOff, bJITLoadStorelwzOff, bJITLoadStorelbzxOff;
	bool bJITLoadStoreFloatingOff;
	bool bJITLoadStorePairedOff;
	bool bJITFloatingPointOff;
	bool bJITIntegerOff;
	bool bJITPairedOff;
	bool bJITSystemRegistersOff;
	bool bJITBranchOff;
	bool bJITILTimeProfiling;
	bool bJITILOutputIR;

	bool bFastmem;
	bool bFPRF;
	bool bAccurateNaNs;

	int iTimingVariance; // in milli secounds
	bool bCPUThread;
	bool bDSPThread;
	bool bDSPHLE;
	bool bSkipIdle;
	bool bSyncGPUOnSkipIdleHack;
	bool bNTSC;
	bool bForceNTSCJ;
	bool bHLE_BS2;
	bool bEnableCheats;
	bool bEnableMemcardSdWriting;

	bool bDPL2Decoder;
	int iLatency;

	bool bRunCompareServer;
	bool bRunCompareClient;

	bool bMMU;
	bool bDCBZOFF;
	int iBBDumpPort;
	bool bFastDiscSpeed;

	bool bSyncGPU;
	int iSyncGpuMaxDistance;
	int iSyncGpuMinDistance;
	float fSyncGpuOverclock;

	int SelectedLanguage;
	bool bOverrideGCLanguage;

	bool bWii;

	// Interface settings
	bool bConfirmStop, bHideCursor, bAutoHideCursor, bUsePanicHandlers, bOnScreenDisplayMessages;
	std::string theme_name;

	// Display settings
	std::string strFullscreenResolution;
	int iRenderWindowXPos, iRenderWindowYPos;
	int iRenderWindowWidth, iRenderWindowHeight;
	bool bRenderWindowAutoSize, bKeepWindowOnTop;
	bool bFullscreen, bRenderToMain;
	bool bProgressive, bPAL60;
	bool bDisableScreenSaver;

	int iPosX, iPosY, iWidth, iHeight;

	// Fifo Player related settings
	bool bLoopFifoReplay;

	enum EBootBS2
	{
		BOOT_DEFAULT,
		BOOT_BS2_JAP,
		BOOT_BS2_USA,
		BOOT_BS2_EUR,
	};

	enum EBootType
	{
		BOOT_ISO,
		BOOT_ELF,
		BOOT_DOL,
		BOOT_WII_NAND,
		BOOT_BS2,
		BOOT_DFF
	};
	EBootType m_BootType;

	std::string m_strVideoBackend;
	std::string m_strGPUDeterminismMode;

	// set based on the string version
	GPUDeterminismMode m_GPUDeterminismMode;

	// files
	std::string m_strFilename;
	std::string m_strBootROM;
	std::string m_strSRAM;
	std::string m_strDefaultISO;
	std::string m_strDVDRoot;
	std::string m_strApploader;
	std::string m_strUniqueID;
	std::string m_strName;
	u16 m_revision;

	std::string m_perfDir;

	void LoadDefaults();
	bool AutoSetup(EBootBS2 _BootBS2);
	const std::string &GetUniqueID() const { return m_strUniqueID; }
	void CheckMemcardPath(std::string& memcardPath, const std::string& gameRegion, bool isSlotA);
	DiscIO::IVolume::ELanguage GetCurrentLanguage(bool wii) const;

	IniFile LoadDefaultGameIni() const;
	IniFile LoadLocalGameIni() const;
	IniFile LoadGameIni() const;

	static IniFile LoadDefaultGameIni(const std::string& id, u16 revision);
	static IniFile LoadLocalGameIni(const std::string& id, u16 revision);
	static IniFile LoadGameIni(const std::string& id, u16 revision);

	static std::vector<std::string> GetGameIniFilenames(const std::string& id, u16 revision);

	std::string m_NANDPath;

	std::string m_strMemoryCardA;
	std::string m_strMemoryCardB;
	std::string m_strGbaCartA;
	std::string m_strGbaCartB;
	TEXIDevices m_EXIDevice[3];
	SIDevices m_SIDevice[4];
	std::string m_bba_mac;

	// interface language
	int m_InterfaceLanguage;
	float m_EmulationSpeed;
	bool m_OCEnable;
	float m_OCFactor;
	// other interface settings
	bool m_InterfaceToolbar;
	bool m_InterfaceStatusbar;
	bool m_InterfaceLogWindow;
	bool m_InterfaceLogConfigWindow;
	bool m_InterfaceExtendedFPSInfo;

	bool m_ListDrives;
	bool m_ListWad;
	bool m_ListElfDol;
	bool m_ListWii;
	bool m_ListGC;
	bool m_ListPal;
	bool m_ListUsa;
	bool m_ListJap;
	bool m_ListAustralia;
	bool m_ListFrance;
	bool m_ListGermany;
	bool m_ListItaly;
	bool m_ListKorea;
	bool m_ListNetherlands;
	bool m_ListRussia;
	bool m_ListSpain;
	bool m_ListTaiwan;
	bool m_ListWorld;
	bool m_ListUnknown;
	int m_ListSort;
	int m_ListSort2;

	// Game list column toggles
	bool m_showSystemColumn;
	bool m_showBannerColumn;
	bool m_showMakerColumn;
	bool m_showFileNameColumn;
	bool m_showIDColumn;
	bool m_showRegionColumn;
	bool m_showSizeColumn;
	bool m_showStateColumn;

	// Toggles whether compressed titles show up in blue in the game list
	bool m_ColorCompressed;

	std::string m_WirelessMac;
	bool m_PauseMovie;
	bool m_ShowLag;
	bool m_ShowFrameCount;
	std::string m_strMovieAuthor;
	unsigned int m_FrameSkip;
	bool m_DumpFrames;
	bool m_DumpFramesSilent;
	bool m_ShowInputDisplay;

	bool m_PauseOnFocusLost;

	// DSP settings
	bool m_DSPEnableJIT;
	bool m_DSPCaptureLog;
	bool m_DumpAudio;
	bool m_IsMuted;
	bool m_DumpUCode;
	int m_Volume;
	std::string sBackend;

	// Input settings
	bool m_BackgroundInput;
	bool m_AdapterRumble[4];
	bool m_AdapterKonga[4];

	SysConf* m_SYSCONF;

	// Save settings
	void SaveSettings();

	// Load settings
	void LoadSettings();

	// Return the permanent and somewhat globally used instance of this struct
	static SConfig& GetInstance() { return(*m_Instance); }

	static void Init();
	static void Shutdown();

private:
	SConfig();
	~SConfig();

	void SaveGeneralSettings(IniFile& ini);
	void SaveInterfaceSettings(IniFile& ini);
	void SaveDisplaySettings(IniFile& ini);
	void SaveGameListSettings(IniFile& ini);
	void SaveCoreSettings(IniFile& ini);
	void SaveDSPSettings(IniFile& ini);
	void SaveInputSettings(IniFile& ini);
	void SaveMovieSettings(IniFile& ini);
	void SaveFifoPlayerSettings(IniFile& ini);

	void LoadGeneralSettings(IniFile& ini);
	void LoadInterfaceSettings(IniFile& ini);
	void LoadDisplaySettings(IniFile& ini);
	void LoadGameListSettings(IniFile& ini);
	void LoadCoreSettings(IniFile& ini);
	void LoadDSPSettings(IniFile& ini);
	void LoadInputSettings(IniFile& ini);
	void LoadMovieSettings(IniFile& ini);
	void LoadFifoPlayerSettings(IniFile& ini);

	static SConfig* m_Instance;
};
