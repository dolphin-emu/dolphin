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

#include <string> // System
#include <vector>
#include <wx/spinbutt.h>

#include "Common.h"
#include "CommonPaths.h"

#include "Core.h" // Core
#include "HW/EXI.h"
#include "HW/SI.h"
#include "HW/DSPHLE/DSPHLE.h"
#include "HW/DSPLLE/DSPLLE.h"
#include "IPC_HLE/WII_IPC_HLE.h"

#include "Globals.h" // Local
#include "ConfigMain.h"
#include "ConfigManager.h"
#include "SysConf.h"
#include "Frame.h"
#include "HotkeyDlg.h"
#include "Main.h"

#include "VideoBackendBase.h"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

#define TEXT_BOX(page, text) new wxStaticText(page, wxID_ANY, text, wxDefaultPosition, wxDefaultSize)

extern CFrame* main_frame;

// keep these in sync with CConfigMain::InitializeGUILists
static const wxLanguage langIds[] =
{
	wxLANGUAGE_DEFAULT,
	wxLANGUAGE_ARABIC,
	wxLANGUAGE_CHINESE_SIMPLIFIED,
	wxLANGUAGE_CHINESE_TRADITIONAL,
	wxLANGUAGE_CZECH,
	wxLANGUAGE_DANISH,
	wxLANGUAGE_DUTCH,
	wxLANGUAGE_ENGLISH,
	wxLANGUAGE_FRENCH,
	wxLANGUAGE_GERMAN,
	wxLANGUAGE_GREEK,
	wxLANGUAGE_HEBREW,
	wxLANGUAGE_HUNGARIAN,
	wxLANGUAGE_ITALIAN,
	wxLANGUAGE_JAPANESE,
	wxLANGUAGE_KOREAN,
	wxLANGUAGE_NORWEGIAN_BOKMAL,
	wxLANGUAGE_POLISH,
	wxLANGUAGE_PORTUGUESE,
	wxLANGUAGE_PORTUGUESE_BRAZILIAN,
	wxLANGUAGE_RUSSIAN,
	wxLANGUAGE_SERBIAN,
	wxLANGUAGE_SPANISH,
	wxLANGUAGE_TURKISH,
};

// Strings for Device Selections
#define DEV_NONE_STR		_trans("<Nothing>")
#define DEV_DUMMY_STR		_trans("Dummy")

#define SIDEV_STDCONT_STR	_trans("Standard Controller")
#define SIDEV_GBA_STR		"GBA"
#define SIDEV_AM_BB_STR		_trans("AM-Baseboard")

#define EXIDEV_MEMCARD_STR	_trans("Memory Card")
#define EXIDEV_MIC_STR		_trans("Mic")
#define EXIDEV_BBA_STR		"BBA"
#define EXIDEV_AM_BB_STR	_trans("AM-Baseboard")
#define EXIDEV_GECKO_STR	"USBGecko"

#define CSTR_TRANS(a)		wxString(wxGetTranslation(wxT(a))).mb_str()

#ifdef WIN32
//only used with xgettext to be picked up as translatable string.
//win32 does not have wx on its path, the provided wxALL_FILES 
//translation does not work there.
#define unusedALL_FILES wxTRANSLATE("All files (*.*)|*.*");
#endif

BEGIN_EVENT_TABLE(CConfigMain, wxDialog)

EVT_CLOSE(CConfigMain::OnClose)
EVT_BUTTON(wxID_OK, CConfigMain::OnOk)


EVT_CHECKBOX(ID_CPUTHREAD, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_IDLESKIP, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_ENABLECHEATS, CConfigMain::CoreSettingsChanged)
EVT_CHOICE(ID_FRAMELIMIT, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_FRAMELIMIT_USEFPSFORLIMITING, CConfigMain::CoreSettingsChanged)

EVT_CHECKBOX(ID_ALWAYS_HLE_BS2, CConfigMain::CoreSettingsChanged)
EVT_RADIOBOX(ID_CPUENGINE, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_LOCKTHREADS, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_DSPTHREAD, CConfigMain::CoreSettingsChanged)


EVT_CHOICE(ID_DISPLAY_FULLSCREENRES, CConfigMain::DisplaySettingsChanged)
EVT_TEXT(ID_DISPLAY_WINDOWWIDTH, CConfigMain::DisplaySettingsChanged)
EVT_TEXT(ID_DISPLAY_WINDOWHEIGHT, CConfigMain::DisplaySettingsChanged)
EVT_CHECKBOX(ID_DISPLAY_AUTOSIZE, CConfigMain::DisplaySettingsChanged)
EVT_CHECKBOX(ID_DISPLAY_FULLSCREEN, CConfigMain::DisplaySettingsChanged)
EVT_CHECKBOX(ID_DISPLAY_HIDECURSOR, CConfigMain::DisplaySettingsChanged)
EVT_CHECKBOX(ID_DISPLAY_RENDERTOMAIN, CConfigMain::DisplaySettingsChanged)
EVT_CHECKBOX(ID_DISPLAY_PROGSCAN, CConfigMain::DisplaySettingsChanged)
EVT_CHECKBOX(ID_DISPLAY_NTSCJ, CConfigMain::DisplaySettingsChanged)

EVT_RADIOBOX(ID_DSPENGINE, CConfigMain::AudioSettingsChanged)
EVT_CHECKBOX(ID_ENABLE_DTK_MUSIC, CConfigMain::AudioSettingsChanged)
EVT_CHECKBOX(ID_ENABLE_THROTTLE, CConfigMain::AudioSettingsChanged)
EVT_CHECKBOX(ID_DUMP_AUDIO, CConfigMain::AudioSettingsChanged)
EVT_CHOICE(ID_FREQUENCY, CConfigMain::AudioSettingsChanged)
EVT_CHOICE(ID_BACKEND, CConfigMain::AudioSettingsChanged)
EVT_SLIDER(ID_VOLUME, CConfigMain::AudioSettingsChanged)

EVT_CHECKBOX(ID_INTERFACE_CONFIRMSTOP, CConfigMain::DisplaySettingsChanged)
EVT_CHECKBOX(ID_INTERFACE_USEPANICHANDLERS, CConfigMain::DisplaySettingsChanged)
EVT_RADIOBOX(ID_INTERFACE_THEME, CConfigMain::DisplaySettingsChanged)
EVT_CHOICE(ID_INTERFACE_LANG, CConfigMain::DisplaySettingsChanged)
EVT_BUTTON(ID_HOTKEY_CONFIG, CConfigMain::DisplaySettingsChanged)


EVT_CHOICE(ID_GC_SRAM_LNG, CConfigMain::GCSettingsChanged)

EVT_CHOICE(ID_GC_EXIDEVICE_SLOTA, CConfigMain::GCSettingsChanged)
EVT_BUTTON(ID_GC_EXIDEVICE_SLOTA_PATH, CConfigMain::GCSettingsChanged)
EVT_CHOICE(ID_GC_EXIDEVICE_SLOTB, CConfigMain::GCSettingsChanged)
EVT_BUTTON(ID_GC_EXIDEVICE_SLOTB_PATH, CConfigMain::GCSettingsChanged)
EVT_CHOICE(ID_GC_EXIDEVICE_SP1, CConfigMain::GCSettingsChanged)
EVT_CHOICE(ID_GC_SIDEVICE0, CConfigMain::GCSettingsChanged)
EVT_CHOICE(ID_GC_SIDEVICE1, CConfigMain::GCSettingsChanged)
EVT_CHOICE(ID_GC_SIDEVICE2, CConfigMain::GCSettingsChanged)
EVT_CHOICE(ID_GC_SIDEVICE3, CConfigMain::GCSettingsChanged)


EVT_CHOICE(ID_WII_BT_BAR, CConfigMain::WiiSettingsChanged)
EVT_SLIDER(ID_WII_BT_SENS, CConfigMain::WiiSettingsChanged)
EVT_SLIDER(ID_WII_BT_SPKV, CConfigMain::WiiSettingsChanged)
EVT_CHECKBOX(ID_WII_BT_MOT, CConfigMain::WiiSettingsChanged)

EVT_CHECKBOX(ID_WII_IPL_SSV, CConfigMain::WiiSettingsChanged)
EVT_CHECKBOX(ID_WII_IPL_E60, CConfigMain::WiiSettingsChanged)
EVT_CHOICE(ID_WII_IPL_AR, CConfigMain::WiiSettingsChanged)
EVT_CHOICE(ID_WII_IPL_LNG, CConfigMain::WiiSettingsChanged)

EVT_CHECKBOX(ID_WII_SD_CARD, CConfigMain::WiiSettingsChanged)
EVT_CHECKBOX(ID_WII_KEYBOARD, CConfigMain::WiiSettingsChanged)
EVT_CHECKBOX(ID_WII_WIIMOTE_RECONNECT, CConfigMain::WiiSettingsChanged)


EVT_LISTBOX(ID_ISOPATHS, CConfigMain::ISOPathsSelectionChanged)
EVT_CHECKBOX(ID_RECURSIVEISOPATH, CConfigMain::RecursiveDirectoryChanged)
EVT_BUTTON(ID_ADDISOPATH, CConfigMain::AddRemoveISOPaths)
EVT_BUTTON(ID_REMOVEISOPATH, CConfigMain::AddRemoveISOPaths)

EVT_FILEPICKER_CHANGED(ID_DEFAULTISO, CConfigMain::DefaultISOChanged)
EVT_DIRPICKER_CHANGED(ID_DVDROOT, CConfigMain::DVDRootChanged)
EVT_FILEPICKER_CHANGED(ID_APPLOADERPATH, CConfigMain::ApploaderPathChanged)


EVT_CHOICE(ID_GRAPHIC_CB, CConfigMain::OnSelectionChanged)
EVT_BUTTON(ID_GRAPHIC_CONFIG, CConfigMain::OnConfig)

END_EVENT_TABLE()

CConfigMain::CConfigMain(wxWindow* parent, wxWindowID id, const wxString& title,
		const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	// Control refreshing of the ISOs list
	bRefreshList = false;

	AddResolutions();
	CreateGUIControls();

	// Update selected ISO paths
	for(u32 i = 0; i < SConfig::GetInstance().m_ISOFolder.size(); i++)
	{
		ISOPaths->Append(wxString(SConfig::GetInstance().m_ISOFolder[i].c_str(), *wxConvCurrent));
	}
}

CConfigMain::~CConfigMain()
{
}

void CConfigMain::SetSelectedTab(int tab)
{
	// TODO : this is just a quick and dirty way to do it, possible cleanup

	switch (tab)
	{
	case ID_AUDIOPAGE:
		Notebook->SetSelection(2);
		break;
	}
}

// Used to restrict changing of some options while emulator is running
void CConfigMain::UpdateGUI()
{
	if(Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		// Disable the Core stuff on GeneralPage
		CPUThread->Disable();
		SkipIdle->Disable();
		EnableCheats->Disable();
		
		AlwaysHLE_BS2->Disable();
		CPUEngine->Disable();
		LockThreads->Disable();
		DSPThread->Disable();
		
		// Disable stuff on DisplayPage
		FullscreenResolution->Disable();
		RenderToMain->Disable();
		ProgressiveScan->Disable();
		NTSCJ->Disable();

		// Disable graphics backend selection
		GraphicSelection->Disable();

		// Disable stuff on AudioPage
		DSPEngine->Disable();

		// Disable stuff on GamecubePage
		GCSystemLang->Disable();

		// Disable stuff on WiiPage
		WiiSensBarPos->Disable();
		WiiSensBarSens->Disable();
		WiimoteSpkVolume->Disable();
		WiimoteMotor->Disable();
		WiiScreenSaver->Disable();
		WiiEuRGB60->Disable();
		WiiAspectRatio->Disable();
		WiiSystemLang->Disable();

		// Disable stuff on PathsPage
		PathsPage->Disable();
	}
}

void CConfigMain::InitializeGUILists()
{
	// General page
	// Framelimit
	arrayStringFor_Framelimit.Add(_("Off"));
	arrayStringFor_Framelimit.Add(_("Auto"));
	for (int i = 10; i <= 120; i += 5)	// from 10 to 120
		arrayStringFor_Framelimit.Add(wxString::Format(wxT("%i"), i));

	// Emulator Engine
	arrayStringFor_CPUEngine.Add(_("Interpreter (VERY slow)"));
	arrayStringFor_CPUEngine.Add(_("JIT Recompiler (recommended)"));
	arrayStringFor_CPUEngine.Add(_("JITIL experimental recompiler"));
	
	// DSP Engine 
	arrayStringFor_DSPEngine.Add(_("DSP HLE emulation (fast)"));
	arrayStringFor_DSPEngine.Add(_("DSP LLE recompiler"));
	arrayStringFor_DSPEngine.Add(_("DSP LLE interpreter (slow)"));
	
	
	// Display page
	// Resolutions
	if (arrayStringFor_FullscreenResolution.empty())
		arrayStringFor_FullscreenResolution.Add(_("<No resolutions found>"));

	// Themes
	arrayStringFor_Themes.Add(wxT("Boomy"));
	arrayStringFor_Themes.Add(wxT("Vista"));
	arrayStringFor_Themes.Add(wxT("X-Plastik"));
	arrayStringFor_Themes.Add(wxT("KDE"));
	
	// Gamecube page
	// GC Language arrayStrings
	arrayStringFor_GCSystemLang.Add(_("English"));
	arrayStringFor_GCSystemLang.Add(_("German"));
	arrayStringFor_GCSystemLang.Add(_("French"));
	arrayStringFor_GCSystemLang.Add(_("Spanish"));
	arrayStringFor_GCSystemLang.Add(_("Italian"));
	arrayStringFor_GCSystemLang.Add(_("Dutch"));

	
	// Wii page
	// Sensorbar Position
	arrayStringFor_WiiSensBarPos.Add(_("Bottom"));
	arrayStringFor_WiiSensBarPos.Add(_("Top"));
	
	// Aspect ratio
	arrayStringFor_WiiAspectRatio.Add(wxT("4:3"));
	arrayStringFor_WiiAspectRatio.Add(wxT("16:9"));
	
	// Wii Language arrayStrings
	arrayStringFor_WiiSystemLang = arrayStringFor_GCSystemLang;
	arrayStringFor_WiiSystemLang.Insert(_("Japanese"), 0);
	arrayStringFor_WiiSystemLang.Add(_("Simplified Chinese"));
	arrayStringFor_WiiSystemLang.Add(_("Traditional Chinese"));
	arrayStringFor_WiiSystemLang.Add(_("Korean"));

	// GUI language arrayStrings
	// keep these in sync with the langIds array at the beginning of this file
	arrayStringFor_InterfaceLang.Add(_("<System>"));
	arrayStringFor_InterfaceLang.Add(_("Arabic"));
	arrayStringFor_InterfaceLang.Add(_("Chinese (Simplified)"));
	arrayStringFor_InterfaceLang.Add(_("Chinese (Traditional)"));
	arrayStringFor_InterfaceLang.Add(_("Czech"));
	arrayStringFor_InterfaceLang.Add(_("Danish"));
	arrayStringFor_InterfaceLang.Add(_("Dutch"));
	arrayStringFor_InterfaceLang.Add(_("English"));
	arrayStringFor_InterfaceLang.Add(_("French"));
	arrayStringFor_InterfaceLang.Add(_("German"));
	arrayStringFor_InterfaceLang.Add(_("Greek"));
	arrayStringFor_InterfaceLang.Add(_("Hebrew"));
	arrayStringFor_InterfaceLang.Add(_("Hungarian"));
	arrayStringFor_InterfaceLang.Add(_("Italian"));
	arrayStringFor_InterfaceLang.Add(_("Japanese"));
	arrayStringFor_InterfaceLang.Add(_("Korean"));
	arrayStringFor_InterfaceLang.Add(_("Norwegian Bokmaal"));
	arrayStringFor_InterfaceLang.Add(_("Polish"));
	arrayStringFor_InterfaceLang.Add(_("Portuguese"));
	arrayStringFor_InterfaceLang.Add(_("Portuguese (Brazilian)"));
	arrayStringFor_InterfaceLang.Add(_("Russian"));
	arrayStringFor_InterfaceLang.Add(_("Serbian"));
	arrayStringFor_InterfaceLang.Add(_("Spanish"));
	arrayStringFor_InterfaceLang.Add(_("Turkish"));
}

void CConfigMain::InitializeGUIValues()
{
	const SCoreStartupParameter& startup_params = SConfig::GetInstance().m_LocalCoreStartupParameter;
	
	// Load DSP Settings.
	ac_Config.Load();

	// General - Basic
	CPUThread->SetValue(startup_params.bCPUThread);
	SkipIdle->SetValue(startup_params.bSkipIdle);
	EnableCheats->SetValue(startup_params.bEnableCheats);
	Framelimit->SetSelection(SConfig::GetInstance().m_Framelimit);
	UseFPSForLimiting->SetValue(SConfig::GetInstance().b_UseFPS);

	// General - Advanced
	AlwaysHLE_BS2->SetValue(startup_params.bHLE_BS2);
	CPUEngine->SetSelection(startup_params.iCPUCore);
	LockThreads->SetValue(startup_params.bLockThreads);
	DSPThread->SetValue(startup_params.bDSPThread);


	// Display - Display
	FullscreenResolution->SetStringSelection(wxString::FromAscii(startup_params.strFullscreenResolution.c_str()));
	WindowWidth->SetValue(startup_params.iRenderWindowWidth);
	WindowHeight->SetValue(startup_params.iRenderWindowHeight);
	WindowAutoSize->SetValue(startup_params.bRenderWindowAutoSize);
	Fullscreen->SetValue(startup_params.bFullscreen);
	HideCursor->SetValue(startup_params.bHideCursor);
	RenderToMain->SetValue(startup_params.bRenderToMain);
	ProgressiveScan->SetValue(startup_params.bProgressive);
	// A bit strange behavior, but this needs to stay in sync with the main progressive boolean
	SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", startup_params.bProgressive);
	NTSCJ->SetValue(startup_params.bNTSCJ);


	// Display - Interface
	ConfirmStop->SetValue(startup_params.bConfirmStop);
	UsePanicHandlers->SetValue(startup_params.bUsePanicHandlers);
	Theme->SetSelection(startup_params.iTheme);
	// need redesign
	for (unsigned int i = 0; i < sizeof(langIds) / sizeof(wxLanguage); i++)
	{
		if (langIds[i] == SConfig::GetInstance().m_InterfaceLanguage)
		{
			InterfaceLang->SetSelection(i);
			break;
		}
	}

	// Audio DSP Engine
	if (startup_params.bDSPHLE)
		DSPEngine->SetSelection(0);
	else
		DSPEngine->SetSelection(ac_Config.m_EnableJIT ? 1 : 2);

	// Audio
	VolumeSlider->Enable(SupportsVolumeChanges(ac_Config.sBackend));
	VolumeSlider->SetValue(ac_Config.m_Volume);
	VolumeText->SetLabel(wxString::Format(wxT("%d %%"), ac_Config.m_Volume));
	EnableDTKMusic->SetValue(ac_Config.m_EnableDTKMusic ? true : false);
	EnableThrottle->SetValue(ac_Config.m_EnableThrottle ? true : false);
	DumpAudio->SetValue(ac_Config.m_DumpAudio ? true : false);
	FrequencySelection->SetSelection(
		FrequencySelection->FindString(wxString::Format(_("%d Hz"), ac_Config.iFrequency)));
	// add backends to the list
	AddAudioBackends();


	// Gamecube - IPL
	GCSystemLang->SetSelection(startup_params.SelectedLanguage);

	// Gamecube - Devices
	wxArrayString SlotDevices;
		SlotDevices.Add(_(DEV_NONE_STR));
		SlotDevices.Add(_(DEV_DUMMY_STR));
		SlotDevices.Add(_(EXIDEV_MEMCARD_STR));
		SlotDevices.Add(_(EXIDEV_GECKO_STR));
#if HAVE_PORTAUDIO
		SlotDevices.Add(_(EXIDEV_MIC_STR));
#endif

	wxArrayString SP1Devices;
		SP1Devices.Add(_(DEV_NONE_STR));
		SP1Devices.Add(_(DEV_DUMMY_STR));
		SP1Devices.Add(_(EXIDEV_BBA_STR));
		SP1Devices.Add(_(EXIDEV_AM_BB_STR));

	wxArrayString SIDevices;
		SIDevices.Add(_(DEV_NONE_STR));
		SIDevices.Add(_(SIDEV_STDCONT_STR));
		SIDevices.Add(_(SIDEV_GBA_STR));
		SIDevices.Add(_(SIDEV_AM_BB_STR));

	for (int i = 0; i < 3; ++i)
	{
		bool isMemcard = false;

		// Add strings to the wxChoice list, the third wxChoice is the SP1 slot
		if (i == 2)
			GCEXIDevice[i]->Append(SP1Devices);
		else
			GCEXIDevice[i]->Append(SlotDevices);

		switch (SConfig::GetInstance().m_EXIDevice[i])
		{
		case EXIDEVICE_NONE:
			GCEXIDevice[i]->SetStringSelection(SlotDevices[0]);
			break;
		case EXIDEVICE_MEMORYCARD_A:
		case EXIDEVICE_MEMORYCARD_B:
			isMemcard = GCEXIDevice[i]->SetStringSelection(SlotDevices[2]);
			break;
		case EXIDEVICE_MIC:
			GCEXIDevice[i]->SetStringSelection(SlotDevices[4]);
			break;
		case EXIDEVICE_ETH:
			GCEXIDevice[i]->SetStringSelection(SP1Devices[2]);
			break;
		case EXIDEVICE_AM_BASEBOARD:
			GCEXIDevice[i]->SetStringSelection(SP1Devices[3]);
			break;
		case EXIDEVICE_GECKO:
			GCEXIDevice[i]->SetStringSelection(SlotDevices[3]);
			break;
		case EXIDEVICE_DUMMY:
		default:
			GCEXIDevice[i]->SetStringSelection(SlotDevices[1]);
			break;
		}
		if (!isMemcard && i < 2)
			GCMemcardPath[i]->Disable();
	}
	for (int i = 0; i < 4; ++i)
	{
		// Add string to the wxChoice list
		GCSIDevice[i]->Append(SIDevices);

		switch (SConfig::GetInstance().m_SIDevice[i])
		{
		case SI_GC_CONTROLLER:
			GCSIDevice[i]->SetStringSelection(SIDevices[1]);
			break;
		case SI_GBA:
			GCSIDevice[i]->SetStringSelection(SIDevices[2]);
			break;
		case SI_AM_BASEBOARD:
			GCSIDevice[i]->SetStringSelection(SIDevices[3]);
			break;
		default:
			GCSIDevice[i]->SetStringSelection(SIDevices[0]);
			break;
		}
		// Remove the AM baseboard from the list, only the first list can select it
		if (i == 0)
			SIDevices.RemoveAt(SIDevices.GetCount() - 1);
	}

	// Wii - Wiimote
	WiiSensBarPos->SetSelection(SConfig::GetInstance().m_SYSCONF->GetData<u8>("BT.BAR"));
	WiiSensBarSens->SetValue(SConfig::GetInstance().m_SYSCONF->GetData<u32>("BT.SENS"));
	WiimoteSpkVolume->SetValue(SConfig::GetInstance().m_SYSCONF->GetData<u8>("BT.SPKV"));
	WiimoteMotor->SetValue(SConfig::GetInstance().m_SYSCONF->GetData<bool>("BT.MOT"));
	WiimoteReconnectOnLoad->SetValue(SConfig::GetInstance().m_WiimoteReconnectOnLoad);
	
	// Wii - Misc
	WiiScreenSaver->SetValue(!!SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.SSV"));
	WiiEuRGB60->SetValue(!!SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.E60"));
	WiiAspectRatio->SetSelection(SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.AR"));
	WiiSystemLang->SetSelection(SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.LNG"));
	
	// Wii - Devices
	WiiSDCard->SetValue(SConfig::GetInstance().m_WiiSDCard);
	WiiKeyboard->SetValue(SConfig::GetInstance().m_WiiKeyboard);


	// Paths
	RecursiveISOPath->SetValue(SConfig::GetInstance().m_RecursiveISOFolder);
	DefaultISO->SetPath(wxString(startup_params.m_strDefaultGCM.c_str(), *wxConvCurrent));
	DVDRoot->SetPath(wxString(startup_params.m_strDVDRoot.c_str(), *wxConvCurrent));
	ApploaderPath->SetPath(wxString(startup_params.m_strApploader.c_str(), *wxConvCurrent));

	// video backend list
	for (std::vector<VideoBackend*>::const_iterator it = g_available_video_backends.begin(); it != g_available_video_backends.end(); ++it)
	{
		GraphicSelection->AppendString(wxString::FromUTF8((*it)->GetName().c_str()));
		if (*it == g_video_backend)
			GraphicSelection->Select(it - g_available_video_backends.begin());
	}
}

void CConfigMain::InitializeGUITooltips()
{
	// General - Basic
	CPUThread->SetToolTip(_("This splits the Video and CPU threads, so they can be run on separate cores.\nCauses major speed improvements on PCs with more than one core, but can also cause occasional crashes/glitches."));
	Framelimit->SetToolTip(_("If you set Framelimit higher than game full speed (NTSC:60, PAL:50), you also have to disable Audio Throttle in DSP to make it effective."));

	// General - Advanced
	DSPThread->SetToolTip(_("Run DSP LLE on a dedicated thread (not recommended)."));

	// Display - Display
	FullscreenResolution->SetToolTip(_("Select resolution for fullscreen mode"));
	WindowWidth->SetToolTip(_("Window width for windowed mode"));
	WindowHeight->SetToolTip(_("Window height for windowed mode"));
	WindowAutoSize->SetToolTip(_("Auto size the window to match the game's output resolution adjusted by the EFB scale.\nIt is best to set the aspect ratio to stretch when using this."));
	Fullscreen->SetToolTip(_("Start the rendering window in fullscreen mode."));
	HideCursor->SetToolTip(_("Hide the cursor when it is over the rendering window and the rendering window has focus."));
	RenderToMain->SetToolTip(_("Render to main window."));
	ProgressiveScan->SetToolTip(_("Will enable progressive scan option if supported by software."));
	NTSCJ->SetToolTip(_("Required for using the Japanese ROM font."));

	// Display - Interface
	ConfirmStop->SetToolTip(_("Show a confirmation box before stopping a game."));
	UsePanicHandlers->SetToolTip(_("Show a message box when a potentially serious error has occured.\nDisabling this may avoid annoying and non-fatal messages, but it may also mean that Dolphin suddenly crashes without any explanation at all."));

	// Display - Themes: Copyright notice
	Theme->SetItemToolTip(0, _("Created by Milosz Wlazlo [miloszwl@miloszwl.com, miloszwl.deviantart.com]"));
	Theme->SetItemToolTip(1, _("Created by VistaIcons.com"));
	Theme->SetItemToolTip(2, _("Created by black_rider and published on ForumW.org > Web Developments"));
	Theme->SetItemToolTip(3, _("Created by KDE-Look.org"));

	InterfaceLang->SetToolTip(_("Change the language of the user interface.\nRequires restart."));

	// Audio tooltips
	EnableDTKMusic->SetToolTip(_("This is used to play music tracks, like BGM."));
	EnableThrottle->SetToolTip(_("This is used to control game speed by sound throttle.\nDisabling this could cause abnormal game speed, such as too fast.\nBut sometimes enabling this could cause constant noise.\n\nKeyboard Shortcut <TAB>:  Hold down to instantly disable Throttle."));
	FrequencySelection->SetToolTip(_("Changing this will have no effect while the emulator is running!"));
	BackendSelection->SetToolTip(_("Changing this will have no effect while the emulator is running!"));

	// Gamecube - Devices
	GCEXIDevice[2]->SetToolTip(_("Serial Port 1 - This is the port which devices such as the net adapter use"));

	// Wii - Devices
	WiiKeyboard->SetToolTip(_("This could cause slow down in Wii Menu and some games."));
}

void CConfigMain::CreateGUIControls()
{
	InitializeGUILists();
	
	// Create the notebook and pages
	Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	wxPanel* const GeneralPage = new wxPanel(Notebook, ID_GENERALPAGE, wxDefaultPosition, wxDefaultSize);
	wxPanel* const DisplayPage = new wxPanel(Notebook, ID_DISPLAYPAGE, wxDefaultPosition, wxDefaultSize);
	wxPanel* const AudioPage = new wxPanel(Notebook, ID_AUDIOPAGE, wxDefaultPosition, wxDefaultSize);
	wxPanel* const GamecubePage = new wxPanel(Notebook, ID_GAMECUBEPAGE, wxDefaultPosition, wxDefaultSize);
	wxPanel* const WiiPage = new wxPanel(Notebook, ID_WIIPAGE, wxDefaultPosition, wxDefaultSize);
	PathsPage = new wxPanel(Notebook, ID_PATHSPAGE, wxDefaultPosition, wxDefaultSize);

	Notebook->AddPage(GeneralPage, _("General"));
	Notebook->AddPage(DisplayPage, _("Display"));
	Notebook->AddPage(AudioPage, _("Audio"));
	Notebook->AddPage(GamecubePage, _("Gamecube"));
	Notebook->AddPage(WiiPage, _("Wii"));
	Notebook->AddPage(PathsPage, _("Paths"));

	// General page
	// Core Settings - Basic
	CPUThread = new wxCheckBox(GeneralPage, ID_CPUTHREAD, _("Enable Dual Core (speedup)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	SkipIdle = new wxCheckBox(GeneralPage, ID_IDLESKIP, _("Enable Idle Skipping (speedup)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	EnableCheats = new wxCheckBox(GeneralPage, ID_ENABLECHEATS, _("Enable Cheats"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	// Framelimit
	Framelimit = new wxChoice(GeneralPage, ID_FRAMELIMIT, wxDefaultPosition, wxDefaultSize, arrayStringFor_Framelimit, 0, wxDefaultValidator);
	UseFPSForLimiting = new wxCheckBox(GeneralPage, ID_FRAMELIMIT_USEFPSFORLIMITING, _("Use FPS For Limiting"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	// Core Settings - Advanced
	AlwaysHLE_BS2 = new wxCheckBox(GeneralPage, ID_ALWAYS_HLE_BS2, _("Skip GC BIOS"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	CPUEngine = new wxRadioBox(GeneralPage, ID_CPUENGINE, _("CPU Emulator Engine"), wxDefaultPosition, wxDefaultSize, arrayStringFor_CPUEngine, 0, wxRA_SPECIFY_ROWS);
	LockThreads = new wxCheckBox(GeneralPage, ID_LOCKTHREADS, _("Lock threads to cores"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	DSPThread = new wxCheckBox(GeneralPage, ID_DSPTHREAD, _("DSP LLE on thread"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Populate the General settings
	wxBoxSizer* sFramelimit = new wxBoxSizer(wxHORIZONTAL);
	sFramelimit->Add(TEXT_BOX(GeneralPage, _("Framelimit:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sFramelimit->Add(Framelimit, 0, wxALL | wxEXPAND, 5);
	sFramelimit->Add(UseFPSForLimiting, 0, wxALL | wxEXPAND, 5);
	wxStaticBoxSizer* const sbBasic = new wxStaticBoxSizer(wxVERTICAL, GeneralPage, _("Basic Settings"));
	sbBasic->Add(CPUThread, 0, wxALL, 5);
	sbBasic->Add(SkipIdle, 0, wxALL, 5);
	sbBasic->Add(EnableCheats, 0, wxALL, 5);
	sbBasic->Add(sFramelimit, 0, wxALL | wxEXPAND, 5);

	wxStaticBoxSizer* const sbAdvanced = new wxStaticBoxSizer(wxVERTICAL, GeneralPage, _("Advanced Settings"));
	sbAdvanced->Add(AlwaysHLE_BS2, 0, wxALL, 5);
	sbAdvanced->Add(CPUEngine, 0, wxALL, 5);
	sbAdvanced->Add(LockThreads, 0, wxALL, 5);
	sbAdvanced->Add(DSPThread, 0, wxALL, 5);

	wxBoxSizer* const sGeneralPage = new wxBoxSizer(wxVERTICAL);
	sGeneralPage->Add(sbBasic, 0, wxEXPAND | wxALL, 5);
	sGeneralPage->Add(sbAdvanced, 0, wxEXPAND | wxALL, 5);
	GeneralPage->SetSizer(sGeneralPage);

	// General display settings
	FullscreenResolution = new wxChoice(DisplayPage, ID_DISPLAY_FULLSCREENRES, wxDefaultPosition, wxDefaultSize, arrayStringFor_FullscreenResolution, 0, wxDefaultValidator, arrayStringFor_FullscreenResolution[0]);
	WindowWidth = new wxSpinCtrl(DisplayPage, ID_DISPLAY_WINDOWWIDTH, wxEmptyString, wxDefaultPosition, wxSize(70, -1));
	WindowWidth->SetRange(0,3280);
	WindowHeight = new wxSpinCtrl(DisplayPage, ID_DISPLAY_WINDOWHEIGHT, wxEmptyString, wxDefaultPosition, wxSize(70, -1));
	WindowHeight->SetRange(0,2048);
	WindowAutoSize = new wxCheckBox(DisplayPage, ID_DISPLAY_AUTOSIZE, _("Auto"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	Fullscreen = new wxCheckBox(DisplayPage, ID_DISPLAY_FULLSCREEN, _("Start Renderer in Fullscreen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	HideCursor = new wxCheckBox(DisplayPage, ID_DISPLAY_HIDECURSOR, _("Hide Mouse Cursor"));
	RenderToMain = new wxCheckBox(DisplayPage, ID_DISPLAY_RENDERTOMAIN, _("Render to Main Window"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	ProgressiveScan = new wxCheckBox(DisplayPage, ID_DISPLAY_PROGSCAN, _("Enable Progressive Scan"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	NTSCJ = new wxCheckBox(DisplayPage, ID_DISPLAY_NTSCJ, _("Set Console as NTSC-J"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Interface Language
	// TODO : Do these really belong to the display page?

	InterfaceLang = new wxChoice(DisplayPage, ID_INTERFACE_LANG, wxDefaultPosition,
			wxDefaultSize, arrayStringFor_InterfaceLang, 0, wxDefaultValidator);
	// Hotkey configuration
	HotkeyConfig = new wxButton(DisplayPage, ID_HOTKEY_CONFIG, _("Hotkeys"),
			wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT, wxDefaultValidator);
	// Themes - this should really be a wxChoice...
	Theme = new wxRadioBox(DisplayPage, ID_INTERFACE_THEME, _("Theme"),
			wxDefaultPosition, wxDefaultSize, arrayStringFor_Themes, 1, wxRA_SPECIFY_ROWS);
	// Interface settings
	ConfirmStop = new wxCheckBox(DisplayPage, ID_INTERFACE_CONFIRMSTOP, _("Confirm On Stop"),
			wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	UsePanicHandlers = new wxCheckBox(DisplayPage, ID_INTERFACE_USEPANICHANDLERS,
			_("Use Panic Handlers"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Populate the Display page
	wxBoxSizer* sDisplayRes = new wxBoxSizer(wxHORIZONTAL);
	sDisplayRes->Add(TEXT_BOX(DisplayPage, _("Fullscreen Display Resolution:")),
		   	0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	sDisplayRes->Add(FullscreenResolution, 0, wxEXPAND, 5);

	// backend
	wxBoxSizer* svidbackend = new wxBoxSizer(wxHORIZONTAL);
	svidbackend->Add(TEXT_BOX(DisplayPage, _("Video Backend:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	GraphicSelection = new wxChoice(DisplayPage, ID_GRAPHIC_CB,
			wxDefaultPosition, wxDefaultSize, 0, NULL, 0, wxDefaultValidator);
	svidbackend->Add(GraphicSelection, 0, wxLEFT|wxBOTTOM|wxTOP, 5);
	GraphicConfig = new wxButton(DisplayPage, ID_GRAPHIC_CONFIG, _("GFX Config"),
			wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT, wxDefaultValidator);
	svidbackend->Add(GraphicConfig, 0, wxALIGN_CENTER_VERTICAL, 5);

	wxBoxSizer* sDisplaySize = new wxBoxSizer(wxHORIZONTAL);
	sDisplaySize->Add(TEXT_BOX(DisplayPage, _("Window Size:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	sDisplaySize->Add(WindowWidth, 0, wxEXPAND | wxRIGHT, 5);
	sDisplaySize->Add(TEXT_BOX(DisplayPage, wxT("x")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	sDisplaySize->Add(WindowHeight, 0, wxEXPAND | wxRIGHT, 5);
	sDisplaySize->Add(WindowAutoSize, 0, wxALIGN_CENTER_VERTICAL, 5);

	sbDisplay = new wxStaticBoxSizer(wxVERTICAL, DisplayPage, _("Emulator Display Settings"));
	sbDisplay->Add(svidbackend, 0, wxEXPAND | wxLEFT, 5);
	sbDisplay->Add(sDisplayRes, 0, wxALL, 5);
	sbDisplay->Add(sDisplaySize, 0, wxALL, 5);
	sbDisplay->Add(Fullscreen, 0, wxEXPAND | wxALL, 5);
	sbDisplay->Add(HideCursor, 0, wxALL, 5);
	sbDisplay->Add(RenderToMain, 0, wxEXPAND | wxALL, 5);
	sbDisplay->Add(ProgressiveScan, 0, wxEXPAND | wxALL, 5);
	sbDisplay->Add(NTSCJ, 0, wxEXPAND | wxALL, 5);

	wxBoxSizer* sInterface = new wxBoxSizer(wxHORIZONTAL);
	sInterface->Add(TEXT_BOX(DisplayPage, _("Language:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sInterface->Add(InterfaceLang, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sInterface->AddStretchSpacer();
	sInterface->Add(HotkeyConfig, 0, wxALIGN_RIGHT | wxALL, 5);
	sbInterface = new wxStaticBoxSizer(wxVERTICAL, DisplayPage, _("Interface Settings"));
	sbInterface->Add(ConfirmStop, 0, wxALL, 5);
	sbInterface->Add(UsePanicHandlers, 0, wxALL, 5);
	sbInterface->Add(Theme, 0, wxEXPAND | wxALL, 5);
	sbInterface->Add(sInterface, 0, wxEXPAND | wxALL, 5);

	sDisplayPage = new wxBoxSizer(wxVERTICAL);
	sDisplayPage->Add(sbDisplay, 0, wxEXPAND | wxALL, 5);
	sDisplayPage->Add(sbInterface, 0, wxEXPAND | wxALL, 5);
	DisplayPage->SetSizer(sDisplayPage);

	
	// Audio page
	DSPEngine = new wxRadioBox(AudioPage, ID_DSPENGINE, _("DSP Emulator Engine"),
				wxDefaultPosition, wxDefaultSize, arrayStringFor_DSPEngine, 0, wxRA_SPECIFY_ROWS);
	EnableDTKMusic = new wxCheckBox(AudioPage, ID_ENABLE_DTK_MUSIC, _("Enable DTK Music"),
				wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	EnableThrottle = new wxCheckBox(AudioPage, ID_ENABLE_THROTTLE, _("Enable Audio Throttle"),
				wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	DumpAudio = new wxCheckBox(AudioPage, ID_DUMP_AUDIO, _("Dump Audio"),
				wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	VolumeSlider = new wxSlider(AudioPage, ID_VOLUME, 0, 1, 100,
				wxDefaultPosition, wxDefaultSize, wxSL_VERTICAL|wxSL_INVERSE);
	VolumeText = new wxStaticText(AudioPage, wxID_ANY, wxT(""),
				wxDefaultPosition, wxDefaultSize, 0);
	BackendSelection = new wxChoice(AudioPage, ID_BACKEND, wxDefaultPosition,
				wxDefaultSize, wxArrayBackends, 0, wxDefaultValidator, wxEmptyString);
	FrequencySelection = new wxChoice(AudioPage, ID_FREQUENCY);
	FrequencySelection->Append(wxString::Format(_("%d Hz"), 48000));
	FrequencySelection->Append(wxString::Format(_("%d Hz"), 32000));

	// Create sizer and add items to dialog
	wxStaticBoxSizer *sbAudioSettings = new wxStaticBoxSizer(wxVERTICAL, AudioPage, _("Sound Settings"));
	sbAudioSettings->Add(DSPEngine, 0, wxALL | wxEXPAND, 5);
	sbAudioSettings->Add(EnableDTKMusic, 0, wxALL, 5);
	sbAudioSettings->Add(EnableThrottle, 0, wxALL, 5);
	sbAudioSettings->Add(DumpAudio, 0, wxALL, 5);

	wxStaticBoxSizer *sbVolume = new wxStaticBoxSizer(wxVERTICAL, AudioPage, _("Volume"));
	sbVolume->Add(VolumeSlider, 1, wxLEFT|wxRIGHT, 13);
	sbVolume->Add(VolumeText, 0, wxALIGN_CENTER|wxALL, 5);

	wxGridBagSizer *sBackend = new wxGridBagSizer();
	sBackend->Add(TEXT_BOX(AudioPage, _("Audio Backend:")), wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBackend->Add(BackendSelection, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
	sBackend->Add(TEXT_BOX(AudioPage, _("Sample Rate:")), wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBackend->Add(FrequencySelection, wxGBPosition(1, 1), wxDefaultSpan, wxALL, 5);
	wxStaticBoxSizer *sbBackend = new wxStaticBoxSizer(wxHORIZONTAL, AudioPage, _("Backend Settings"));
	sbBackend->Add(sBackend, 0, wxEXPAND);

	wxBoxSizer *sAudio = new wxBoxSizer(wxHORIZONTAL);
	sAudio->Add(sbAudioSettings, 1, wxEXPAND|wxALL, 5);
	sAudio->Add(sbVolume, 0, wxEXPAND|wxALL, 5);

	sAudioPage = new wxBoxSizer(wxVERTICAL);
	sAudioPage->Add(sAudio, 0, wxALL|wxEXPAND);
	sAudioPage->Add(sbBackend, 0, wxALL|wxEXPAND, 5);
	AudioPage->SetSizerAndFit(sAudioPage);


	// Gamecube page
	// IPL settings
	GCSystemLang = new wxChoice(GamecubePage, ID_GC_SRAM_LNG, wxDefaultPosition,
			wxDefaultSize, arrayStringFor_GCSystemLang, 0, wxDefaultValidator);
	// Device settings
	// EXI Devices
	wxStaticText* GCEXIDeviceText[3];
	GCEXIDeviceText[0] = TEXT_BOX(GamecubePage, _("Slot A"));
	GCEXIDeviceText[1] = TEXT_BOX(GamecubePage, _("Slot B"));
	GCEXIDeviceText[2] = TEXT_BOX(GamecubePage, wxT("SP1   "));
	GCEXIDevice[0] = new wxChoice(GamecubePage, ID_GC_EXIDEVICE_SLOTA);
	GCEXIDevice[1] = new wxChoice(GamecubePage, ID_GC_EXIDEVICE_SLOTB);
	GCEXIDevice[2] = new wxChoice(GamecubePage, ID_GC_EXIDEVICE_SP1);
	GCMemcardPath[0] = new wxButton(GamecubePage, ID_GC_EXIDEVICE_SLOTA_PATH, wxT("..."),
			wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT, wxDefaultValidator);
	GCMemcardPath[1] = new wxButton(GamecubePage, ID_GC_EXIDEVICE_SLOTB_PATH, wxT("..."),
			wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT, wxDefaultValidator);

	//SI Devices
	wxStaticText* GCSIDeviceText[4];
	GCSIDeviceText[0] = TEXT_BOX(GamecubePage, _("Port 1"));
	GCSIDeviceText[1] = TEXT_BOX(GamecubePage, _("Port 2"));
	GCSIDeviceText[2] = TEXT_BOX(GamecubePage, _("Port 3"));
	GCSIDeviceText[3] = TEXT_BOX(GamecubePage, _("Port 4"));
	GCSIDevice[0] = new wxChoice(GamecubePage, ID_GC_SIDEVICE0);
	GCSIDevice[1] = new wxChoice(GamecubePage, ID_GC_SIDEVICE1);
	GCSIDevice[2] = new wxChoice(GamecubePage, ID_GC_SIDEVICE2);
	GCSIDevice[3] = new wxChoice(GamecubePage, ID_GC_SIDEVICE3);

	// Populate the Gamecube page
	sGamecubeIPLSettings = new wxGridBagSizer();
	sGamecubeIPLSettings->Add(TEXT_BOX(GamecubePage, _("System Language:")),
			wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sGamecubeIPLSettings->Add(GCSystemLang, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
	sbGamecubeIPLSettings = new wxStaticBoxSizer(wxVERTICAL, GamecubePage, _("IPL Settings"));
	sbGamecubeIPLSettings->Add(sGamecubeIPLSettings);
	wxBoxSizer *sEXIDevices[4], *sSIDevices[4];
	wxStaticBoxSizer *sbGamecubeDeviceSettings = new wxStaticBoxSizer(wxVERTICAL, GamecubePage, _("Device Settings"));
	for (int i = 0; i < 3; ++i)
	{
		sEXIDevices[i] = new wxBoxSizer(wxHORIZONTAL);
		sEXIDevices[i]->Add(GCEXIDeviceText[i], 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sEXIDevices[i]->Add(GCEXIDevice[i], 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
		if (i < 2)
			sEXIDevices[i]->Add(GCMemcardPath[i], 0, wxALL, 5);
		sbGamecubeDeviceSettings->Add(sEXIDevices[i]);
	}
	for (int i = 0; i < 4; ++i)
	{
		sSIDevices[i] = new wxBoxSizer(wxHORIZONTAL);
		sSIDevices[i]->Add(GCSIDeviceText[i], 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sSIDevices[i]->Add(GCSIDevice[i], 0, wxALL, 5);
		sbGamecubeDeviceSettings->Add(sSIDevices[i]);
	}
	sGamecubePage = new wxBoxSizer(wxVERTICAL);
	sGamecubePage->Add(sbGamecubeIPLSettings, 0, wxEXPAND|wxALL, 5);
	sGamecubePage->Add(sbGamecubeDeviceSettings, 0, wxEXPAND|wxALL, 5);
	GamecubePage->SetSizer(sGamecubePage);


	// Wii page
	// Wiimote Settings
	WiiSensBarPos = new wxChoice(WiiPage, ID_WII_BT_BAR, wxDefaultPosition, wxDefaultSize, arrayStringFor_WiiSensBarPos, 0, wxDefaultValidator);
	WiiSensBarSens = new wxSlider(WiiPage, ID_WII_BT_SENS, 0, 0, 4);
	WiimoteSpkVolume = new wxSlider(WiiPage, ID_WII_BT_SPKV, 0, 0, 127);
	WiimoteMotor = new wxCheckBox(WiiPage, ID_WII_BT_MOT, _("Wiimote Motor"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiimoteReconnectOnLoad = new wxCheckBox(WiiPage, ID_WII_WIIMOTE_RECONNECT, _("Reconnect Wiimote On Load State"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Misc Settings
	WiiScreenSaver = new wxCheckBox(WiiPage, ID_WII_IPL_SSV, _("Enable Screen Saver (burn-in reduction)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiiEuRGB60 = new wxCheckBox(WiiPage, ID_WII_IPL_E60, _("Use EuRGB60 Mode (PAL60)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiiAspectRatio = new wxChoice(WiiPage, ID_WII_IPL_AR, wxDefaultPosition, wxDefaultSize, arrayStringFor_WiiAspectRatio, 0, wxDefaultValidator);
	WiiSystemLang = new wxChoice(WiiPage, ID_WII_IPL_LNG, wxDefaultPosition, wxDefaultSize, arrayStringFor_WiiSystemLang, 0, wxDefaultValidator);

	// Device Settings
	WiiSDCard = new wxCheckBox(WiiPage, ID_WII_SD_CARD, _("Insert SD Card"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiiKeyboard = new wxCheckBox(WiiPage, ID_WII_KEYBOARD, _("Connect USB Keyboard"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Populate the Wii Page
	sWiimoteSettings = new wxGridBagSizer();
	sWiimoteSettings->Add(TEXT_BOX(WiiPage, _("Sensor Bar Position:")),
			wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sWiimoteSettings->Add(WiiSensBarPos, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
	sWiimoteSettings->Add(TEXT_BOX(WiiPage, _("IR Sensitivity:")),
			wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sWiimoteSettings->Add(WiiSensBarSens, wxGBPosition(1, 1), wxDefaultSpan, wxEXPAND|wxALL, 5);
	sWiimoteSettings->Add(TEXT_BOX(WiiPage, _("Speaker Volume:")),
		wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sWiimoteSettings->Add(WiimoteSpkVolume, wxGBPosition(2, 1), wxDefaultSpan, wxEXPAND|wxALL, 5);
	sWiimoteSettings->Add(WiimoteMotor, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sWiimoteSettings->Add(WiimoteReconnectOnLoad, wxGBPosition(4, 0), wxGBSpan(1, 2), wxALL, 5);
	sbWiimoteSettings = new wxStaticBoxSizer(wxHORIZONTAL, WiiPage, _("Wiimote Settings"));
	sbWiimoteSettings->Add(sWiimoteSettings);

	sWiiIPLSettings = new wxGridBagSizer();
	sWiiIPLSettings->Add(WiiScreenSaver, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sWiiIPLSettings->Add(WiiEuRGB60, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sWiiIPLSettings->Add(TEXT_BOX(WiiPage, _("Aspect Ratio:")),
			wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sWiiIPLSettings->Add(WiiAspectRatio, wxGBPosition(2, 1), wxDefaultSpan, wxALL, 5);
	sWiiIPLSettings->Add(TEXT_BOX(WiiPage, _("System Language:")),
			wxGBPosition(3, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sWiiIPLSettings->Add(WiiSystemLang, wxGBPosition(3, 1), wxDefaultSpan, wxALL, 5);
	sbWiiIPLSettings = new wxStaticBoxSizer(wxVERTICAL, WiiPage, _("Misc Settings"));
	sbWiiIPLSettings->Add(sWiiIPLSettings);

	sbWiiDeviceSettings = new wxStaticBoxSizer(wxVERTICAL, WiiPage, _("Device Settings"));
	sbWiiDeviceSettings->Add(WiiSDCard, 0, wxALL, 5);
	sbWiiDeviceSettings->Add(WiiKeyboard, 0, wxALL, 5);

	sWiiPage = new wxBoxSizer(wxVERTICAL);
	sWiiPage->Add(sbWiimoteSettings, 0, wxEXPAND|wxALL, 5);
	sWiiPage->Add(sbWiiIPLSettings, 0, wxEXPAND|wxALL, 5);
	sWiiPage->Add(sbWiiDeviceSettings, 0, wxEXPAND|wxALL, 5);
	WiiPage->SetSizer(sWiiPage);

	
	// Paths page
	ISOPaths = new wxListBox(PathsPage, ID_ISOPATHS, wxDefaultPosition, wxDefaultSize, arrayStringFor_ISOPaths, wxLB_SINGLE, wxDefaultValidator);
	RecursiveISOPath = new wxCheckBox(PathsPage, ID_RECURSIVEISOPATH, _("Search Subfolders"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	AddISOPath = new wxButton(PathsPage, ID_ADDISOPATH, _("Add..."), wxDefaultPosition, wxDefaultSize, 0);
	RemoveISOPath = new wxButton(PathsPage, ID_REMOVEISOPATH, _("Remove"), wxDefaultPosition, wxDefaultSize, 0);
	RemoveISOPath->Enable(false);

	DefaultISO = new wxFilePickerCtrl(PathsPage, ID_DEFAULTISO, wxEmptyString, _("Choose a default ISO:"),
		_("All GC/Wii images (gcm, iso, ciso, gcz)") + wxString::Format(wxT("|*.gcm;*.iso;*.ciso;*.gcz|%s"), wxGetTranslation(wxALL_FILES)),
		wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL|wxFLP_OPEN);
	DVDRoot = new wxDirPickerCtrl(PathsPage, ID_DVDROOT, wxEmptyString, _("Choose a DVD root directory:"), wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
	ApploaderPath = new wxFilePickerCtrl(PathsPage, ID_APPLOADERPATH, wxEmptyString, _("Choose file to use as apploader: (applies to discs constructed from directories only)"),
		_("apploader (.img)") + wxString::Format(wxT("|*.img|%s"), wxGetTranslation(wxALL_FILES)),
		wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL|wxFLP_OPEN);

	// Populate the settings
	wxBoxSizer* sISOButtons = new wxBoxSizer(wxHORIZONTAL);
	sISOButtons->Add(RecursiveISOPath, 0, wxALL|wxALIGN_CENTER, 0);
	sISOButtons->AddStretchSpacer();
	sISOButtons->Add(AddISOPath, 0, wxALL, 0);
	sISOButtons->Add(RemoveISOPath, 0, wxALL, 0);
	sbISOPaths = new wxStaticBoxSizer(wxVERTICAL, PathsPage, _("ISO Directories"));
	sbISOPaths->Add(ISOPaths, 1, wxEXPAND|wxALL, 0);
	sbISOPaths->Add(sISOButtons, 0, wxEXPAND|wxALL, 5);

	sOtherPaths = new wxGridBagSizer();
	sOtherPaths->Add(TEXT_BOX(PathsPage, _("Default ISO:")),
			wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sOtherPaths->Add(DefaultISO, wxGBPosition(0, 1), wxDefaultSpan, wxEXPAND|wxALL, 5);
	sOtherPaths->Add(TEXT_BOX(PathsPage, _("DVD Root:")),
			wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sOtherPaths->Add(DVDRoot, wxGBPosition(1, 1), wxDefaultSpan, wxEXPAND|wxALL, 5);
	sOtherPaths->Add(TEXT_BOX(PathsPage, _("Apploader:")),
			wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sOtherPaths->Add(ApploaderPath, wxGBPosition(2, 1), wxDefaultSpan, wxEXPAND|wxALL, 5);
	sOtherPaths->AddGrowableCol(1);

	// Populate the Paths page
	sPathsPage = new wxBoxSizer(wxVERTICAL);
	sPathsPage->Add(sbISOPaths, 1, wxEXPAND|wxALL, 5);
	sPathsPage->Add(sOtherPaths, 0, wxEXPAND|wxALL, 5);
	PathsPage->SetSizer(sPathsPage);

	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	InitializeGUIValues();
	InitializeGUITooltips();

	UpdateGUI();

	SetSizerAndFit(sMain);
	Center();
	SetFocus();
}

void CConfigMain::OnClose(wxCloseEvent& WXUNUSED (event))
{
	EndModal((bRefreshList) ? wxID_OK : wxID_CANCEL);
}

void CConfigMain::OnOk(wxCommandEvent& WXUNUSED (event))
{
	Close();

	// Save the config. Dolphin crashes to often to save the settings on closing only
	SConfig::GetInstance().SaveSettings();

	// Save Audio settings
	ac_Config.SaveSettings();
}

// Core settings
void CConfigMain::CoreSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	// Core - Basic
	case ID_CPUTHREAD:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread = CPUThread->IsChecked();
		break;
	case ID_IDLESKIP:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle = SkipIdle->IsChecked();
		break;
	case ID_ENABLECHEATS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats = EnableCheats->IsChecked();
		break;
	case ID_FRAMELIMIT:
		SConfig::GetInstance().m_Framelimit = Framelimit->GetSelection();
		break;
	case ID_FRAMELIMIT_USEFPSFORLIMITING:
		SConfig::GetInstance().b_UseFPS = UseFPSForLimiting->IsChecked();
		break;
	// Core - Advanced
	case ID_ALWAYS_HLE_BS2:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHLE_BS2 = AlwaysHLE_BS2->IsChecked();
		break;
	case ID_CPUENGINE:
		SConfig::GetInstance().m_LocalCoreStartupParameter.iCPUCore = CPUEngine->GetSelection();
		if (main_frame->g_pCodeWindow)
			main_frame->g_pCodeWindow->GetMenuBar()->Check(IDM_INTERPRETER,
				SConfig::GetInstance().m_LocalCoreStartupParameter.iCPUCore?false:true);
		break;
	case ID_DSPTHREAD:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPThread = DSPThread->IsChecked();
		break;
	case ID_LOCKTHREADS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bLockThreads = LockThreads->IsChecked();
		break;
	}
}

// Display and Interface settings
void CConfigMain::DisplaySettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	// Display - Display
	case ID_DISPLAY_FULLSCREENRES:
		SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution =
			FullscreenResolution->GetStringSelection().mb_str();
#if defined(HAVE_XRANDR) && HAVE_XRANDR
		main_frame->m_XRRConfig->Update();
#endif
		break;
	case ID_DISPLAY_WINDOWWIDTH:
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth = WindowWidth->GetValue();
		break;
	case ID_DISPLAY_WINDOWHEIGHT:
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight = WindowHeight->GetValue();
		break;
	case ID_DISPLAY_AUTOSIZE:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderWindowAutoSize = WindowAutoSize->IsChecked();
		break;
	case ID_DISPLAY_FULLSCREEN:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen = Fullscreen->IsChecked();
		break;
	case ID_DISPLAY_HIDECURSOR:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor = HideCursor->IsChecked();
		break;
	case ID_DISPLAY_RENDERTOMAIN:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain = RenderToMain->IsChecked();
		break;
	case ID_DISPLAY_PROGSCAN:
		SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", ProgressiveScan->IsChecked());
		SConfig::GetInstance().m_LocalCoreStartupParameter.bProgressive = ProgressiveScan->IsChecked();
		break;
	case ID_DISPLAY_NTSCJ:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bNTSCJ = NTSCJ->IsChecked();
		break;
	// Display - Interface
	case ID_INTERFACE_CONFIRMSTOP:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop = ConfirmStop->IsChecked();
		break;
	case ID_INTERFACE_USEPANICHANDLERS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers = UsePanicHandlers->IsChecked();
		SetEnableAlert(UsePanicHandlers->IsChecked());
		break;
	case ID_INTERFACE_THEME:
		SConfig::GetInstance().m_LocalCoreStartupParameter.iTheme = Theme->GetSelection();
		main_frame->InitBitmaps();
		break;
	case ID_INTERFACE_LANG:
		if (SConfig::GetInstance().m_InterfaceLanguage != langIds[InterfaceLang->GetSelection()])
			SuccessAlertT("You must restart Dolphin in order for the change to take effect.");
		SConfig::GetInstance().m_InterfaceLanguage = langIds[InterfaceLang->GetSelection()];
		break;
	case ID_HOTKEY_CONFIG:
		{
			HotkeyConfigDialog m_HotkeyDialog(this);
			m_HotkeyDialog.ShowModal();
		}
		// Update the GUI in case menu accelerators were changed
		main_frame->UpdateGUI();
		break;
	}
}

void CConfigMain::AudioSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_DSPENGINE:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPHLE = DSPEngine->GetSelection() == 0;
		ac_Config.m_EnableJIT = DSPEngine->GetSelection() == 1;
		ac_Config.Update();
		break;
	case ID_VOLUME:
		ac_Config.m_Volume = VolumeSlider->GetValue();
		ac_Config.Update();
		VolumeText->SetLabel(wxString::Format(wxT("%d %%"), VolumeSlider->GetValue()));
		break;
	case ID_BACKEND:
		VolumeSlider->Enable(SupportsVolumeChanges(std::string(BackendSelection->GetStringSelection().mb_str())));
	default:
		ac_Config.m_EnableDTKMusic = EnableDTKMusic->GetValue();
		ac_Config.m_EnableThrottle = EnableThrottle->GetValue();
		ac_Config.m_DumpAudio = DumpAudio->GetValue();
		ac_Config.sBackend = BackendSelection->GetStringSelection().mb_str();
		long int frequency;
		FrequencySelection->GetStringSelection().ToLong(&frequency);
		ac_Config.iFrequency = frequency;
		ac_Config.Update();
		break;
	}
}

void CConfigMain::AddAudioBackends()
{
	std::vector<std::string> backends = AudioCommon::GetSoundBackends();
	// I'm sure Billiard will change this into an auto sometimes soon :P
	for (std::vector<std::string>::const_iterator iter = backends.begin(); 
		 iter != backends.end(); ++iter)
	{
		BackendSelection->Append(wxString::FromAscii((*iter).c_str()));
		int num = BackendSelection->\
			FindString(wxString::FromAscii(ac_Config.sBackend.c_str()));
		BackendSelection->SetSelection(num);
	}
}

bool CConfigMain::SupportsVolumeChanges(std::string backend)
{
	//FIXME: this one should ask the backend whether it supports it.
	//       but getting the backend from string etc. is probably
	//       too much just to enable/disable a stupid slider...
	return (backend == BACKEND_DIRECTSOUND ||
			backend == BACKEND_COREAUDIO ||
			backend == BACKEND_OPENAL ||
			backend == BACKEND_XAUDIO2 ||
			backend == BACKEND_PULSEAUDIO);
}


// GC settings
// -----------------------
void CConfigMain::GCSettingsChanged(wxCommandEvent& event)
{
	int sidevice = 0;
	int exidevice = 0;
	switch (event.GetId())
	{
	// Gamecube - IPL
	case ID_GC_SRAM_LNG:
		SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage = GCSystemLang->GetSelection();
		bRefreshList = true;
		break;
	// Gamecube - Devices
	case ID_GC_EXIDEVICE_SP1:
		exidevice++;
	case ID_GC_EXIDEVICE_SLOTB:
		exidevice++;
	case ID_GC_EXIDEVICE_SLOTA:
		ChooseEXIDevice(std::string(event.GetString().mb_str()), exidevice);
		break;
	case ID_GC_EXIDEVICE_SLOTA_PATH:
		ChooseMemcardPath(SConfig::GetInstance().m_strMemoryCardA, true);
		break;
	case ID_GC_EXIDEVICE_SLOTB_PATH:
		ChooseMemcardPath(SConfig::GetInstance().m_strMemoryCardB, false);
		break;
	case ID_GC_SIDEVICE3:
		sidevice++;
	case ID_GC_SIDEVICE2:
		sidevice++;
	case ID_GC_SIDEVICE1:
		sidevice++;
	case ID_GC_SIDEVICE0:
		ChooseSIDevice(std::string(event.GetString().mb_str()), sidevice);
		break;
	}
}

void CConfigMain::ChooseMemcardPath(std::string& strMemcard, bool isSlotA)
{
	std::string filename = std::string(wxFileSelector(
		_("Choose a file to open"),
		wxString::FromUTF8(File::GetUserPath(D_GCUSER_IDX).c_str()),
		isSlotA ? wxT(GC_MEMCARDA) : wxT(GC_MEMCARDB),
		wxEmptyString,
		_("Gamecube Memory Cards (*.raw,*.gcp)") + wxString(wxT("|*.raw;*.gcp"))).mb_str());

	if (!filename.empty())
	{
		// also check that the path isn't used for the other memcard...
		if (filename.compare(isSlotA ? SConfig::GetInstance().m_strMemoryCardB
			: SConfig::GetInstance().m_strMemoryCardA) != 0)
		{
			strMemcard = filename;

			if (Core::GetState() != Core::CORE_UNINITIALIZED)
			{
				// Change memcard to the new file
				ExpansionInterface::ChangeDevice(
					isSlotA ? 0 : 1, // SlotA: channel 0, SlotB channel 1
					isSlotA ? EXIDEVICE_MEMORYCARD_A : EXIDEVICE_MEMORYCARD_B,
					0);	// SP1 is device 2, slots are device 0
			}
		}
		else
		{
			PanicAlertT("Cannot use that file as a memory card.\n"
					"Are you trying to use the same file in both slots?");
		}
	}
}

void CConfigMain::ChooseSIDevice(std::string deviceName, int deviceNum)
{
	TSIDevices tempType;
	if (!deviceName.compare(CSTR_TRANS(SIDEV_STDCONT_STR)))
		tempType = SI_GC_CONTROLLER;
	else if (!deviceName.compare(SIDEV_GBA_STR))
		tempType = SI_GBA;
	else if (!deviceName.compare(CSTR_TRANS(SIDEV_AM_BB_STR)))
		tempType = SI_AM_BASEBOARD;
	else
		tempType = SI_NONE;

	SConfig::GetInstance().m_SIDevice[deviceNum] = tempType;

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		// Change plugged device! :D
		SerialInterface::ChangeDevice(tempType, deviceNum);
	}
}

void CConfigMain::ChooseEXIDevice(std::string deviceName, int deviceNum)
{
	TEXIDevices tempType;

	if (!deviceName.compare(CSTR_TRANS(EXIDEV_MEMCARD_STR)))
		tempType = deviceNum ? EXIDEVICE_MEMORYCARD_B : EXIDEVICE_MEMORYCARD_A;
	else if (!deviceName.compare(CSTR_TRANS(EXIDEV_MIC_STR)))
		tempType = EXIDEVICE_MIC;
	else if (!deviceName.compare(EXIDEV_BBA_STR))
		tempType = EXIDEVICE_ETH;
	else if (!deviceName.compare(CSTR_TRANS(EXIDEV_AM_BB_STR)))
		tempType = EXIDEVICE_AM_BASEBOARD;
	else if (!deviceName.compare(EXIDEV_GECKO_STR))
		tempType = EXIDEVICE_GECKO;
	else if (!deviceName.compare(CSTR_TRANS(DEV_NONE_STR)))
		tempType = EXIDEVICE_NONE;
	else
		tempType = EXIDEVICE_DUMMY;

	// Gray out the memcard path button if we're not on a memcard
	if (tempType == EXIDEVICE_MEMORYCARD_A || tempType == EXIDEVICE_MEMORYCARD_B)
		GCMemcardPath[deviceNum]->Enable();
	else if (deviceNum == 0 || deviceNum == 1)
		GCMemcardPath[deviceNum]->Disable();

	SConfig::GetInstance().m_EXIDevice[deviceNum] = tempType;

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		// Change plugged device! :D
		ExpansionInterface::ChangeDevice(
			(deviceNum == 1) ? 1 : 0,	// SlotB is on channel 1, slotA and SP1 are on 0
			tempType,					// The device enum to change to
			(deviceNum == 2) ? 2 : 0);	// SP1 is device 2, slots are device 0
	}
}




// Wii settings
// -------------------
void CConfigMain::WiiSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	// Wii - Wiimote settings
	case ID_WII_BT_BAR:
		SConfig::GetInstance().m_SYSCONF->SetData("BT.BAR", WiiSensBarPos->GetSelection());
		break;
	case ID_WII_BT_SENS:
		SConfig::GetInstance().m_SYSCONF->SetData("BT.SENS", WiiSensBarSens->GetValue());
		break;
	case ID_WII_BT_SPKV:
		SConfig::GetInstance().m_SYSCONF->SetData("BT.SPKV", WiimoteSpkVolume->GetValue());
		break;
	case ID_WII_BT_MOT:
		SConfig::GetInstance().m_SYSCONF->SetData("BT.MOT", WiimoteMotor->IsChecked());
		break;
	case ID_WII_WIIMOTE_RECONNECT:
		SConfig::GetInstance().m_WiimoteReconnectOnLoad = WiimoteReconnectOnLoad->IsChecked();
		break;
	// SYSCONF settings
	case ID_WII_IPL_SSV:
		SConfig::GetInstance().m_SYSCONF->SetData("IPL.SSV", WiiScreenSaver->IsChecked());
		break;
	case ID_WII_IPL_E60:
		SConfig::GetInstance().m_SYSCONF->SetData("IPL.E60", WiiEuRGB60->IsChecked());
		break;
	case ID_WII_IPL_AR:
		SConfig::GetInstance().m_SYSCONF->SetData("IPL.AR", WiiAspectRatio->GetSelection());
		break;
	case ID_WII_IPL_LNG:
		SConfig::GetInstance().m_SYSCONF->SetData("IPL.LNG", WiiSystemLang->GetSelection());
		break;
	// Wii - Devices
	case ID_WII_SD_CARD:
		SConfig::GetInstance().m_WiiSDCard = WiiSDCard->IsChecked();
		WII_IPC_HLE_Interface::SDIO_EventNotify();
		break;
	case ID_WII_KEYBOARD:
		SConfig::GetInstance().m_WiiKeyboard = WiiKeyboard->IsChecked();
		break;
	}
}


// Paths settings
// -------------------
void CConfigMain::ISOPathsSelectionChanged(wxCommandEvent& WXUNUSED (event))
{
	if (!ISOPaths->GetStringSelection().empty())
	{
		RemoveISOPath->Enable(true);
	}
	else
	{
		RemoveISOPath->Enable(false);
	}
}

void CConfigMain::AddRemoveISOPaths(wxCommandEvent& event)
{
	if (event.GetId() == ID_ADDISOPATH)
	{
		wxDirDialog dialog(this, _("Choose a directory to add"), wxGetHomeDir(),
				wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

		if (dialog.ShowModal() == wxID_OK)
		{
			if (ISOPaths->FindString(dialog.GetPath()) != -1)
				wxMessageBox(_("The chosen directory is already in the list"), _("Error"), wxOK);
			else
			{
				bRefreshList = true;
				ISOPaths->Append(dialog.GetPath());
			}
		}
	}
	else
	{
		bRefreshList = true;
		ISOPaths->Delete(ISOPaths->GetSelection());
	}

	// Save changes right away
	SConfig::GetInstance().m_ISOFolder.clear();

	for (unsigned int i = 0; i < ISOPaths->GetCount(); i++)
		SConfig::GetInstance().m_ISOFolder.push_back(std::string(ISOPaths->GetStrings()[i].mb_str()));
}

void CConfigMain::RecursiveDirectoryChanged(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_RecursiveISOFolder = RecursiveISOPath->IsChecked();
	bRefreshList = true;
}

void CConfigMain::DefaultISOChanged(wxFileDirPickerEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM = DefaultISO->GetPath().mb_str();
}

void CConfigMain::DVDRootChanged(wxFileDirPickerEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDVDRoot = DVDRoot->GetPath().mb_str();
}

void CConfigMain::ApploaderPathChanged(wxFileDirPickerEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.m_strApploader = ApploaderPath->GetPath().mb_str();
}


// GFX backend selection
void CConfigMain::OnSelectionChanged(wxCommandEvent& ev)
{
	g_video_backend = g_available_video_backends[ev.GetInt()];
	SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend = g_video_backend->GetName();
}

void CConfigMain::OnConfig(wxCommandEvent&)
{
	if (g_video_backend)
		g_video_backend->ShowConfig(this);
}

// Search for avaliable resolutions
void CConfigMain::AddResolutions()
{
#ifdef _WIN32
	DWORD iModeNum = 0;
	DEVMODE dmi;
	ZeroMemory(&dmi, sizeof(dmi));
	dmi.dmSize = sizeof(dmi);
	std::vector<std::string> resos;

	while (EnumDisplaySettings(NULL, iModeNum++, &dmi) != 0)
	{
		char res[100];
		sprintf(res, "%dx%d", dmi.dmPelsWidth, dmi.dmPelsHeight);
		std::string strRes(res);
		// Only add unique resolutions
		if (std::find(resos.begin(), resos.end(), strRes) == resos.end())
		{
			resos.push_back(strRes);
			arrayStringFor_FullscreenResolution.Add(wxString::FromAscii(res));
		}
		ZeroMemory(&dmi, sizeof(dmi));
	}
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
	main_frame->m_XRRConfig->AddResolutions(arrayStringFor_FullscreenResolution);
#elif defined(__APPLE__)
	CFArrayRef modes = CGDisplayAvailableModes(CGMainDisplayID());
	for (CFIndex i = 0; i < CFArrayGetCount(modes); i++)
	{
		std::stringstream res;
		CFDictionaryRef mode;
		CFNumberRef ref;
		int w, h, d;

		mode = (CFDictionaryRef)CFArrayGetValueAtIndex(modes, i);
		ref = (CFNumberRef)CFDictionaryGetValue(mode, kCGDisplayWidth);
		CFNumberGetValue(ref, kCFNumberIntType, &w);
		ref = (CFNumberRef)CFDictionaryGetValue(mode, kCGDisplayHeight);
		CFNumberGetValue(ref, kCFNumberIntType, &h);
		ref = (CFNumberRef)CFDictionaryGetValue(mode,
			kCGDisplayBitsPerPixel);
		CFNumberGetValue(ref, kCFNumberIntType, &d);

		if (CFDictionaryContainsKey(mode, kCGDisplayModeIsStretched))
			continue;
		if (d != 32)
			continue;

		res << w << "x" << h;

		arrayStringFor_FullscreenResolution.Add(res.str());
	}
#endif
}
