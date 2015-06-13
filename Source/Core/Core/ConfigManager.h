// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "Common/SysConf.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/SI_Device.h"
#include "Core/PowerPC/PowerPC.h"
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
	OptionGroup  m_general_group{"General"};
	OptionGroup  m_interface_group{"Interface"};
	OptionGroup  m_display_group{"Display"};
	OptionGroup  m_gamelist_group{"GameList"};
	OptionGroup  m_core_group{"Core"};
	OptionGroup  m_movie_group{"Movie"};
	OptionGroup  m_dsp_group{"DSP"};
	OptionGroup  m_input_group{"Input"};
	OptionGroup  m_fifoplayer_group{"FifoPlayer"};

	Option<std::string> m_LastFilename {m_general_group, "LastFilename", ""};
	Option<bool> m_ShowLag {m_general_group, "ShowLag", false};
	Option<bool> m_ShowFrameCount {m_general_group, "ShowFrameCount", false};
	Option<int>  m_numISOPaths {m_general_group, "ISOPaths", 0};
	Option<bool> m_RecursiveISOFolder {m_general_group, "RecursiveISOPaths", false};
	Option<std::string> m_NANDPath {m_general_group, "NANDRootPath", ""};
	Option<std::string> m_WirelessMac {m_general_group, "WirelessMac", ""};
#ifdef USE_GDBSTUB
#ifndef _WIN32
	Option<std::string> gdb_socket {m_general_group, "GDBSocket", ""};
#endif
	Option<int> iGDBPort {m_general_group, "GDBPort", -1};
#endif

	Option<bool> bConfirmStop {m_interface_group, "ConfirmStop", true};
	Option<bool> bUsePanicHandlers {m_interface_group, "UsePanicHandlers", true};
	Option<bool> bOnScreenDisplayMessages {m_interface_group, "OnScreenDisplayMessages", true};
	Option<bool> bHideCursor {m_interface_group, "HideCursor", false};
	Option<bool> bAutoHideCursor {m_interface_group, "AutoHideCursor", false};
	Option<int>  iPosX {m_interface_group, "MainWindowPosX", 100};
	Option<int>  iPosY {m_interface_group, "MainWindowPosY", 100};
	Option<int>  iWidth {m_interface_group, "MainWindowWidth", 800};
	Option<int>  iHeight {m_interface_group, "MainWindowHeight", 600};
	Option<int>  m_InterfaceLanguage {m_interface_group, "Language", 0};
	Option<bool> m_InterfaceToolbar {m_interface_group, "ShowToolbar", true};
	Option<bool> m_InterfaceStatusbar {m_interface_group, "ShowStatusbar", true};
	Option<bool> m_InterfaceLogWindow {m_interface_group, "ShowLogWindow", false};
	Option<bool> m_InterfaceLogConfigWindow {m_interface_group, "ShowLogConfigWindow", false};
	Option<bool> m_InterfaceExtendedFPSInfo {m_interface_group, "ExtendedFPSInfo", false};
	Option<bool> m_PauseOnFocusLost {m_interface_group, "PauseOnFocusLost", false};
	Option<std::string> theme_name {m_interface_group, "ThemeName40", "Clean"};

	Option<bool> bFullscreen {m_display_group, "Fullscreen", false};
	Option<std::string> strFullscreenResolution {m_display_group, "FullscreenResolution", "Auto"};
	Option<bool> bRenderToMain {m_display_group, "RenderToMain", false};
	Option<int>  iRenderWindowXPos {m_display_group, "RenderWindowXPos", -1};
	Option<int>  iRenderWindowYPos {m_display_group, "RenderWindowYPos", -1};
	Option<int>  iRenderWindowWidth {m_display_group, "RenderWindowWidth", 640};
	Option<int>  iRenderWindowHeight {m_display_group, "RenderWindowHeight", 480};
	Option<bool> bRenderWindowAutoSize {m_display_group, "RenderWindowAutoSize", false};
	Option<bool> bKeepWindowOnTop {m_display_group, "KeepWindowOnTop", false};
	Option<bool> bProgressive {m_display_group, "ProgressiveScan", false};
	Option<bool> bDisableScreenSaver {m_display_group, "DisableScreenSaver", true};
	Option<bool> bForceNTSCJ {m_display_group, "ForceNTSCJ", false};

	Option<bool> m_ListDrives {m_gamelist_group, "ListDrives", false};
	Option<bool> m_ListWad {m_gamelist_group, "ListWad", true};
	Option<bool> m_ListWii {m_gamelist_group, "ListWii", true};
	Option<bool> m_ListGC {m_gamelist_group, "ListGC", true};
	Option<bool> m_ListJap {m_gamelist_group, "ListJap", true};
	Option<bool> m_ListPal {m_gamelist_group, "ListPal", true};
	Option<bool> m_ListUsa {m_gamelist_group, "ListUsa", true};
	Option<bool> m_ListAustralia {m_gamelist_group, "ListAustralia", true};
	Option<bool> m_ListFrance {m_gamelist_group, "ListFrance", true};
	Option<bool> m_ListGermany {m_gamelist_group, "ListGermany", true};
	Option<bool> m_ListItaly {m_gamelist_group, "ListItaly", true};
	Option<bool> m_ListKorea {m_gamelist_group, "ListKorea", true};
	Option<bool> m_ListNetherlands {m_gamelist_group, "ListNetherlands", true};
	Option<bool> m_ListRussia {m_gamelist_group, "ListRussia", true};
	Option<bool> m_ListSpain {m_gamelist_group, "ListSpain", true};
	Option<bool> m_ListTaiwan {m_gamelist_group, "ListTaiwan", true};
	Option<bool> m_ListWorld {m_gamelist_group, "ListWorld", true};
	Option<bool> m_ListUnknown {m_gamelist_group, "ListUnknown", true};
	Option<int>  m_ListSort {m_gamelist_group, "ListSort", 3};
	Option<int>  m_ListSort2 {m_gamelist_group, "ListSortSecondary", 0};
	Option<bool> m_ColorCompressed {m_gamelist_group, "ColorCompressed", true};
	Option<bool> m_showSystemColumn {m_gamelist_group, "ColumnPlatform", true};
	Option<bool> m_showBannerColumn {m_gamelist_group, "ColumnBanner", true};
	Option<bool> m_showMakerColumn {m_gamelist_group, "ColumnNotes", true};
	Option<bool> m_showIDColumn {m_gamelist_group, "ColumnID", false};
	Option<bool> m_showRegionColumn {m_gamelist_group, "ColumnRegion", true};
	Option<bool> m_showSizeColumn {m_gamelist_group, "ColumnSize", true};
	Option<bool> m_showStateColumn {m_gamelist_group, "ColumnState", true};

	Option<bool> bHLE_BS2 {m_core_group, "HLE_BS2", false};
#ifdef _M_X86
	Option<int>  iCPUCore {m_core_group, "CPUCore", PowerPC::CORE_JIT64};
#elif _M_ARM_64
	Option<int>  iCPUCore {m_core_group, "CPUCore", PowerPC::CORE_JITARM64};
#else
	Option<int>  iCPUCore {m_core_group, "CPUCore", PowerPC::CORE_INTERPRETER};
#endif
	Option<bool> bFastmem {m_core_group, "Fastmem", true};
	Option<bool> bDSPHLE {m_core_group, "DSPHLE", true};
	Option<bool> bCPUThread {m_core_group, "CPUThread", true};
	Option<bool> bSkipIdle {m_core_group, "SkipIdle", true};
	Option<bool> bSyncGPUOnSkipIdleHack {m_core_group, "SyncOnSkipIdle", true};
	Option<std::string> m_strDefaultISO {m_core_group, "DefaultISO", ""};
	Option<std::string> m_strDVDRoot {m_core_group, "DVDRoot", ""};
	Option<std::string> m_strApploader {m_core_group, "Apploader", ""};
	Option<bool> bEnableCheats {m_core_group, "EnableCheats", false};
	Option<int>  SelectedLanguage {m_core_group, "SelectedLanguage", 0};
	Option<bool> bOverrideGCLanguage {m_core_group, "OverrideGCLang", false};
	Option<bool> bDPL2Decoder {m_core_group, "DPL2Decoder", false};
	Option<int>  iLatency {m_core_group, "Latency", 14};
	Option<std::string> m_strMemoryCardA {m_core_group, "MemcardAPath", ""};
	Option<std::string> m_strMemoryCardB {m_core_group, "MemcardBPath", ""};
	Option<std::string> m_strGbaCartA {m_core_group, "AgpCartAPath", ""};
	Option<std::string> m_strGbaCartB {m_core_group, "AgpCartBPath", ""};
	Option<std::string> m_bba_mac {m_core_group, "BBA_MAC", ""};
	Option<bool> bJITILTimeProfiling {m_core_group, "TimeProfiling", false};
	Option<bool> bJITILOutputIR {m_core_group, "OutputIR", false};
	Option<bool> m_WiiSDCard {m_core_group, "WiiSDCard", false};
	Option<bool> m_WiiKeyboard {m_core_group, "WiiKeyboard", false};
	Option<bool> m_WiimoteContinuousScanning {m_core_group, "WiimoteContinuousScanning", false};
	Option<bool> m_WiimoteEnableSpeaker {m_core_group, "WiimoteEnableSpeaker", false};
	Option<bool> bRunCompareServer {m_core_group, "RunCompareServer", false};
	Option<bool> bRunCompareClient {m_core_group, "RunCompareClient", false};
	Option<bool> bMMU {m_core_group, "MMU", false};
	Option<int>  iBBDumpPort {m_core_group, "BBDumpPort", 0};
	Option<bool> bSyncGPU {m_core_group, "SyncGPU", false};
	Option<int>  iSyncGpuMaxDistance {m_core_group, "SyncGpuMaxDistance", 200000};
	Option<int>  iSyncGpuMinDistance {m_core_group, "SyncGpuMinDistance", -200000};
	Option<float> fSyncGpuOverclock {m_core_group, "SyncGpuOverclock", 1.0};
	Option<bool> bFastDiscSpeed {m_core_group, "FastDiscSpeed", false};
	Option<bool> bDCBZOFF {m_core_group, "DCBZ", false};
	Option<int>  m_Framelimit {m_core_group, "FrameLimit", 1};
	Option<float> m_OCFactor {m_core_group, "Overclock", 1.0};
	Option<bool> m_OCEnable {m_core_group, "OverclockEnable", false};
	Option<int>  m_FrameSkip {m_core_group, "FrameSkip", 0};
	Option<std::string> m_strVideoBackend {m_core_group, "GFXBackend", ""};
	Option<std::string> m_strGPUDeterminismMode {m_core_group, "GPUDeterminismMode", "auto"};
	Option<bool> m_GameCubeAdapter {m_core_group, "GameCubeAdapter", true};
	Option<bool> m_AdapterRumble {m_core_group, "AdapterRumble", true};
	Option<TEXIDevices, int> m_EXIDevice[3] {
		{m_core_group, "SlotA", EXIDEVICE_MEMORYCARD},
		{m_core_group, "SlotB", EXIDEVICE_NONE},
		{m_core_group, "SerialPort1", EXIDEVICE_NONE},
	};
	Option<SIDevices, int> m_SIDevice[4] {
		{m_core_group, "SIDevice0", SIDEVICE_GC_CONTROLLER},
		{m_core_group, "SIDevice1", SIDEVICE_NONE},
		{m_core_group, "SIDevice2", SIDEVICE_NONE},
		{m_core_group, "SIDevice3", SIDEVICE_NONE},
	};

	Option<bool> m_PauseMovie {m_movie_group, "PauseMovie", false};
	Option<std::string> m_strMovieAuthor {m_movie_group, "Author", ""};
	Option<bool> m_DumpFrames {m_movie_group, "DumpFrames", false};
	Option<bool> m_DumpFramesSilent {m_movie_group, "DumpFramesSilent", false};
	Option<bool> m_ShowInputDisplay {m_movie_group, "ShowInputDisplay", false};

	Option<bool> m_DSPEnableJIT {m_dsp_group, "EnableJIT", true};
	Option<bool> m_DumpAudio {m_dsp_group, "DumpAudio", false};
#if defined __linux__ && HAVE_ALSA
	Option<std::string> sBackend {m_dsp_group, "Backend", BACKEND_ALSA};
#elif defined __APPLE__
	Option<std::string> sBackend {m_dsp_group, "Backend", BACKEND_COREAUDIO};
#elif defined _WIN32
	Option<std::string> sBackend {m_dsp_group, "Backend", BACKEND_XAUDIO2};
#elif defined ANDROID
	Option<std::string> sBackend {m_dsp_group, "Backend", BACKEND_OPENSLES};
#else
	Option<std::string> sBackend {m_dsp_group, "Backend", BACKEND_NULLSOUND};
#endif
	Option<int>  m_Volume {m_dsp_group, "Volume", 100};
	Option<bool> m_DSPCaptureLog {m_dsp_group, "CaptureLog", false};

	Option<bool> m_BackgroundInput {m_input_group, "BackgroundInput", false};
	Option<bool> bLoopFifoReplay {m_fifoplayer_group, "LoopReplay", true};

	// ISO folder
	std::vector<std::string> m_ISOFolder;

	// Settings
	bool bEnableDebugging;
	bool bAutomaticStart;
	bool bBootToPause;

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

	bool bFPRF;
	bool bAccurateNaNs;

	bool bDSPThread;
	bool bNTSC;
	bool bEnableMemcardSaving;

	bool bWii;

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

	// set based on the string version
	GPUDeterminismMode m_GPUDeterminismMode;

	// files
	std::string m_strFilename;
	std::string m_strBootROM;
	std::string m_strSRAM;
	std::string m_strUniqueID;
	std::string m_strName;
	u16 m_revision;

	std::string m_perfDir;

	// DSP settings
	bool m_IsMuted;

	SysConf* m_SYSCONF;

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

	void LoadGeneralSettings(IniFile& ini);
	void LoadDSPSettings(IniFile& ini);

	static SConfig* m_Instance;
};
