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

#include <string> // System
#include <vector>

#include "Core.h" // Core
#include "ConsoleWindow.h"

#include "Globals.h" // Local
#include "ConfigMain.h"
#include "PluginManager.h"
#include "ConfigManager.h"
#include "Frame.h"


extern CFrame* main_frame;


BEGIN_EVENT_TABLE(CConfigMain, wxDialog)

EVT_CLOSE(CConfigMain::OnClose)
EVT_BUTTON(wxID_CLOSE, CConfigMain::CloseClick)

EVT_CHECKBOX(ID_INTERFACE_CONFIRMSTOP, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_INTERFACE_HIDECURSOR, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_INTERFACE_AUTOHIDECURSOR, CConfigMain::CoreSettingsChanged)
EVT_RADIOBOX(ID_INTERFACE_THEME, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_INTERFACE_WIIMOTE_LEDS, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_INTERFACE_WIIMOTE_SPEAKERS, CConfigMain::CoreSettingsChanged)
EVT_CHOICE(ID_INTERFACE_LANG, CConfigMain::CoreSettingsChanged)

EVT_CHECKBOX(ID_ALLWAYS_HLEBIOS, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_USEDYNAREC, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_USEDUALCORE, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_LOCKTHREADS, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_OPTIMIZEQUANTIZERS, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_IDLESKIP, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_ENABLECHEATS, CConfigMain::CoreSettingsChanged)

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
EVT_CHECKBOX(ID_WII_IPL_SSV, CConfigMain::WiiSettingsChanged)
EVT_CHECKBOX(ID_WII_IPL_PGS, CConfigMain::WiiSettingsChanged)
EVT_CHECKBOX(ID_WII_IPL_E60, CConfigMain::WiiSettingsChanged)
EVT_CHOICE(ID_WII_IPL_AR, CConfigMain::WiiSettingsChanged)
EVT_CHOICE(ID_WII_IPL_LNG, CConfigMain::WiiSettingsChanged)

EVT_LISTBOX(ID_ISOPATHS, CConfigMain::ISOPathsSelectionChanged)
EVT_BUTTON(ID_ADDISOPATH, CConfigMain::AddRemoveISOPaths)
EVT_BUTTON(ID_REMOVEISOPATH, CConfigMain::AddRemoveISOPaths)
EVT_FILEPICKER_CHANGED(ID_DEFAULTISO, CConfigMain::DefaultISOChanged)
EVT_DIRPICKER_CHANGED(ID_DVDROOT, CConfigMain::DVDRootChanged)

EVT_CHOICE(ID_GRAPHIC_CB, CConfigMain::OnSelectionChanged)
EVT_BUTTON(ID_GRAPHIC_CONFIG, CConfigMain::OnConfig)
EVT_CHOICE(ID_DSP_CB, CConfigMain::OnSelectionChanged)
EVT_BUTTON(ID_DSP_CONFIG, CConfigMain::OnConfig)
EVT_CHOICE(ID_PAD_CB, CConfigMain::OnSelectionChanged)
EVT_BUTTON(ID_PAD_CONFIG, CConfigMain::OnConfig)
EVT_CHOICE(ID_WIIMOTE_CB, CConfigMain::OnSelectionChanged)
EVT_BUTTON(ID_WIIMOTE_CONFIG, CConfigMain::OnConfig)

END_EVENT_TABLE()

CConfigMain::CConfigMain(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	// Control refreshing of the ISOs list
	bRefreshList = false;

	// Load Wii SYSCONF
	FullSYSCONFPath = FULL_WII_USER_DIR "shared2/sys/SYSCONF";
	pStream = NULL;
	pStream = fopen(FullSYSCONFPath.c_str(), "rb");
    if (pStream != NULL)
    {
        fread(m_SYSCONF, 1, 0x4000, pStream);
        fclose(pStream);
		m_bSysconfOK = true;
    }
    else
	{
		PanicAlert("Could not read %s. Please recover the SYSCONF file to that location.", FullSYSCONFPath.c_str());
		m_bSysconfOK = false;
	}

	CreateGUIControls();

	// Update selected ISO paths
	for(u32 i = 0; i < SConfig::GetInstance().m_ISOFolder.size(); i++)
	{
		ISOPaths->Append(wxString(SConfig::GetInstance().m_ISOFolder[i].c_str(), wxConvUTF8));
	}
}

CConfigMain::~CConfigMain()
{
}

//////////////////////////////////////////////////////////////////////////
// Used to restrict changing of some options while emulator is running
void CConfigMain::UpdateGUI()
{
	if(Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		// Disable the Core stuff on GeneralPage
		AllwaysHLEBIOS->Disable();
		UseDynaRec->Disable();
		UseDualCore->Disable();
		LockThreads->Disable();
		OptimizeQuantizers->Disable();
		SkipIdle->Disable();
		EnableCheats->Disable();
		// Disable GC Stuff, but devices should be dynamic soon
		GCSystemLang->Disable();
		GCEXIDevice[0]->Disable(); GCEXIDevice[1]->Disable(); GCEXIDevice[2]->Disable();
		GCMemcardPath[0]->Disable(); GCMemcardPath[1]->Disable();
		GCSIDevice[0]->Disable(); GCSIDevice[1]->Disable(); GCSIDevice[2]->Disable(); GCSIDevice[3]->Disable();
		WiiPage->Disable();
		PathsPage->Disable();
		PluginPage->Disable();
	}
}

void CConfigMain::CreateGUIControls()
{
	// Deal with all the language arrayStrings here
	// GC
	arrayStringFor_GCSystemLang.Add(wxT("English"));
	arrayStringFor_GCSystemLang.Add(wxT("German"));
	arrayStringFor_GCSystemLang.Add(wxT("French"));
	arrayStringFor_GCSystemLang.Add(wxT("Spanish"));
	arrayStringFor_GCSystemLang.Add(wxT("Italian"));
	arrayStringFor_GCSystemLang.Add(wxT("Dutch"));
	// Wii
	arrayStringFor_WiiSystemLang = arrayStringFor_GCSystemLang;
	arrayStringFor_WiiSystemLang.Insert(wxT("Japanese"), 0);
	// GUI
	arrayStringFor_InterfaceLang = arrayStringFor_GCSystemLang;
		
	// Create the notebook and pages
	Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	GeneralPage = new wxPanel(Notebook, ID_GENERALPAGE, wxDefaultPosition, wxDefaultSize);
	GamecubePage = new wxPanel(Notebook, ID_GAMECUBEPAGE, wxDefaultPosition, wxDefaultSize);
	WiiPage = new wxPanel(Notebook, ID_WIIPAGE, wxDefaultPosition, wxDefaultSize);
	PathsPage = new wxPanel(Notebook, ID_PATHSPAGE, wxDefaultPosition, wxDefaultSize);
	PluginPage = new wxPanel(Notebook, ID_PLUGINPAGE, wxDefaultPosition, wxDefaultSize);

	Notebook->AddPage(GeneralPage, wxT("General"));
	Notebook->AddPage(GamecubePage, wxT("Gamecube"));
	Notebook->AddPage(WiiPage, wxT("Wii"));
	Notebook->AddPage(PathsPage, wxT("Paths"));
	Notebook->AddPage(PluginPage, wxT("Plugins"));


	//////////////////////////////////////////////////////////////////////////
	// General page

	// Core Settings - Basic
	UseDualCore = new wxCheckBox(GeneralPage, ID_USEDUALCORE, wxT("Enable Dual Core"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	UseDualCore->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore);
	SkipIdle = new wxCheckBox(GeneralPage, ID_IDLESKIP, wxT("Enable Idle Skipping"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	SkipIdle->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle);
	EnableCheats = new wxCheckBox(GeneralPage, ID_ENABLECHEATS, wxT("Enable Cheats"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	EnableCheats->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats);
	// Core Settings - Advanced
	AllwaysHLEBIOS = new wxCheckBox(GeneralPage, ID_ALLWAYS_HLEBIOS, wxT("HLE the BIOS all the time"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	AllwaysHLEBIOS->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bHLEBios);
	UseDynaRec = new wxCheckBox(GeneralPage, ID_USEDYNAREC, wxT("Enable the JIT dynarec"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	UseDynaRec->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bUseJIT);
	LockThreads = new wxCheckBox(GeneralPage, ID_LOCKTHREADS, wxT("Lock threads to cores"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	LockThreads->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bLockThreads);
	OptimizeQuantizers = new wxCheckBox(GeneralPage, ID_OPTIMIZEQUANTIZERS, wxT("Optimize Quantizers"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	OptimizeQuantizers->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bOptimizeQuantizers);

	// Interface settings

	// Confirm on stop
	ConfirmStop = new wxCheckBox(GeneralPage, ID_INTERFACE_CONFIRMSTOP, wxT("Confirm On Stop"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	ConfirmStop->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop);
	// Hide Cursor
	wxStaticText *HideCursorText = new wxStaticText(GeneralPage, ID_INTERFACE_HIDECURSOR_TEXT, wxT("Hide Cursor:"), wxDefaultPosition, wxDefaultSize);
	AutoHideCursor = new wxCheckBox(GeneralPage, ID_INTERFACE_AUTOHIDECURSOR, wxT("Auto"));
	AutoHideCursor->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bAutoHideCursor);
	HideCursor = new wxCheckBox(GeneralPage, ID_INTERFACE_HIDECURSOR, wxT("Always"));
	HideCursor->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor);
	// Wiimote status in statusbar
	wxStaticText *WiimoteStatusText = new wxStaticText(GeneralPage, ID_INTERFACE_WIIMOTE_TEXT, wxT("Show wiimote status:"), wxDefaultPosition, wxDefaultSize);
	WiimoteStatusLEDs = new wxCheckBox(GeneralPage, ID_INTERFACE_WIIMOTE_LEDS, wxT("LEDs"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiimoteStatusLEDs->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiLeds);
	WiimoteStatusSpeakers = new wxCheckBox(GeneralPage, ID_INTERFACE_WIIMOTE_SPEAKERS, wxT("Speakers"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiimoteStatusSpeakers->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiSpeakers);

	// Interface Language
	// At the moment this only changes the language displayed in m_gamelistctrl
	// If someone wants to control the whole GUI's language, it should be set here too
	wxStaticText *InterfaceLangText = new wxStaticText(GeneralPage, ID_INTERFACE_LANG_TEXT, wxT("Game List Language:"), wxDefaultPosition, wxDefaultSize);
	InterfaceLang = new wxChoice(GeneralPage, ID_INTERFACE_LANG, wxDefaultPosition, wxDefaultSize, arrayStringFor_InterfaceLang, 0, wxDefaultValidator);
	// need redesign
	InterfaceLang->SetSelection(SConfig::GetInstance().m_InterfaceLanguage);

	// Themes
	wxArrayString ThemeChoices;
	ThemeChoices.Add(wxT("Boomy"));
	ThemeChoices.Add(wxT("Vista"));
	ThemeChoices.Add(wxT("X-Plastik"));
	ThemeChoices.Add(wxT("KDE"));
	Theme = new wxRadioBox(GeneralPage, ID_INTERFACE_THEME, wxT("Theme"),wxDefaultPosition, wxDefaultSize, ThemeChoices, 1, wxRA_SPECIFY_ROWS);
	// Set selected
	Theme->SetSelection(SConfig::GetInstance().m_LocalCoreStartupParameter.iTheme);

	// ToolTips
	UseDynaRec->SetToolTip(wxT("Disabling this will cause Dolphin to run in interpreter mode,"
		"\nwhich can be more accurate, but is MUCH slower"));
	ConfirmStop->SetToolTip(wxT("Show a confirmation box before stopping a game."));
	AutoHideCursor->SetToolTip(wxT("This will auto hide the cursor in fullscreen mode."));
	HideCursor->SetToolTip(wxT("This will always hide the cursor when it's over the rendering window."
		"\nIt can be convenient in a Wii game that already has a cursor."));
	WiimoteStatusLEDs->SetToolTip(wxT("Show which wiimotes are connected in the statusbar."));
	WiimoteStatusSpeakers->SetToolTip(wxT("Show wiimote speaker status in the statusbar."));

	InterfaceLang->SetToolTip(wxT("For the time being this will only change the text shown in"
		"\nthe game list of PAL GC games."));
	// Copyright notice
	Theme->SetItemToolTip(0, wxT("Created by Milosz Wlazlo [miloszwl@miloszwl.com, miloszwl.deviantart.com]"));
	Theme->SetItemToolTip(1, wxT("Created by VistaIcons.com"));
	Theme->SetItemToolTip(2, wxT("Created by black_rider and published on ForumW.org > Web Developments"));
	Theme->SetItemToolTip(3, wxT("Created by KDE-Look.org"));

	// Populate the settings
	sCore = new wxBoxSizer(wxHORIZONTAL);
	sbBasic = new wxStaticBoxSizer(wxVERTICAL, GeneralPage, wxT("Basic Settings"));
	sbBasic->Add(UseDualCore, 0, wxALL, 5);
	sbBasic->Add(SkipIdle, 0, wxALL, 5);
	sbBasic->Add(EnableCheats, 0, wxALL, 5);
	sbAdvanced = new wxStaticBoxSizer(wxVERTICAL, GeneralPage, wxT("Advanced Settings"));
	sbAdvanced->Add(AllwaysHLEBIOS, 0, wxALL, 5);
	sbAdvanced->Add(UseDynaRec, 0, wxALL, 5);
	sbAdvanced->Add(LockThreads, 0, wxALL, 5);
	sbAdvanced->Add(OptimizeQuantizers, 0, wxALL, 5);
	sCore->Add(sbBasic, 0, wxEXPAND);
	sCore->AddStretchSpacer();
	sCore->Add(sbAdvanced, 0, wxEXPAND);

	sbInterface = new wxStaticBoxSizer(wxVERTICAL, GeneralPage, wxT("Interface Settings"));
	sbInterface->Add(ConfirmStop, 0, wxALL, 5);
	wxBoxSizer *sHideCursor = new wxBoxSizer(wxHORIZONTAL);
	sHideCursor->Add(HideCursorText);
	sHideCursor->Add(AutoHideCursor, 0, wxLEFT, 5);
	sHideCursor->Add(HideCursor, 0, wxLEFT, 5);
	sbInterface->Add(sHideCursor, 0, wxALL, 5);
	wxBoxSizer *sWiimoteStatus = new wxBoxSizer(wxHORIZONTAL);
	sWiimoteStatus->Add(WiimoteStatusText);
	sWiimoteStatus->Add(WiimoteStatusLEDs, 0, wxLEFT, 5);
	sWiimoteStatus->Add(WiimoteStatusSpeakers, 0, wxLEFT, 5);
	sbInterface->Add(sWiimoteStatus, 0, wxALL, 5);
	sbInterface->Add(Theme, 0, wxEXPAND | wxALL, 5);
	wxBoxSizer *sInterfaceLanguage = new wxBoxSizer(wxHORIZONTAL);
	sInterfaceLanguage->Add(InterfaceLangText, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sInterfaceLanguage->Add(InterfaceLang, 0, wxEXPAND | wxALL, 5);
	sbInterface->Add(sInterfaceLanguage, 0, wxEXPAND | wxALL, 5);

	// Populate the entire page
	sGeneralPage = new wxBoxSizer(wxVERTICAL);
	sGeneralPage->Add(sCore, 0, wxEXPAND | wxALL, 5);
	sGeneralPage->Add(sbInterface, 0, wxEXPAND | wxALL, 5);

	GeneralPage->SetSizer(sGeneralPage);
	sGeneralPage->Layout();


	//////////////////////////////////////////////////////////////////////////
	// Gamecube page
	// IPL settings
	sbGamecubeIPLSettings = new wxStaticBoxSizer(wxVERTICAL, GamecubePage, wxT("IPL Settings"));
	GCSystemLangText = new wxStaticText(GamecubePage, ID_GC_SRAM_LNG_TEXT, wxT("System Language:"), wxDefaultPosition, wxDefaultSize);
	GCSystemLang = new wxChoice(GamecubePage, ID_GC_SRAM_LNG, wxDefaultPosition, wxDefaultSize, arrayStringFor_GCSystemLang, 0, wxDefaultValidator);
	GCSystemLang->SetSelection(SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage);
	// Devices
	wxStaticBoxSizer *sbGamecubeDeviceSettings = new wxStaticBoxSizer(wxVERTICAL, GamecubePage, wxT("Device Settings"));
	// EXI Devices
	wxStaticText *GCEXIDeviceText[3];
	GCEXIDeviceText[0] = new wxStaticText(GamecubePage, ID_GC_EXIDEVICE_SLOTA_TEXT, wxT("Slot A"), wxDefaultPosition, wxDefaultSize);
	GCEXIDeviceText[1] = new wxStaticText(GamecubePage, ID_GC_EXIDEVICE_SLOTB_TEXT, wxT("Slot B"), wxDefaultPosition, wxDefaultSize);
	GCEXIDeviceText[2] = new wxStaticText(GamecubePage, ID_GC_EXIDEVICE_SP1_TEXT,	wxT("SP1   "), wxDefaultPosition, wxDefaultSize);
	GCEXIDeviceText[2]->SetToolTip(wxT("Serial Port 1 - This is the port the network adapter uses"));
	const wxString SlotDevices[] = {wxT("<Nothing>"),wxT("Memory Card"), wxT("Mic")};
	static const int numSlotDevices = sizeof(SlotDevices)/sizeof(wxString);
	const wxString SP1Devices[] = {wxT("<Nothing>"),wxT("BBA")};
	static const int numSP1Devices = sizeof(SP1Devices)/sizeof(wxString);
	GCEXIDevice[0] = new wxChoice(GamecubePage, ID_GC_EXIDEVICE_SLOTA, wxDefaultPosition, wxDefaultSize, numSlotDevices, SlotDevices, 0, wxDefaultValidator);
	GCEXIDevice[1] = new wxChoice(GamecubePage, ID_GC_EXIDEVICE_SLOTB, wxDefaultPosition, wxDefaultSize, numSlotDevices, SlotDevices, 0, wxDefaultValidator);
	GCEXIDevice[2] = new wxChoice(GamecubePage, ID_GC_EXIDEVICE_SP1, wxDefaultPosition, wxDefaultSize, numSP1Devices, SP1Devices, 0, wxDefaultValidator);
	GCMemcardPath[0] = new wxButton(GamecubePage, ID_GC_EXIDEVICE_SLOTA_PATH, wxT("..."), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT, wxDefaultValidator);
	GCMemcardPath[1] = new wxButton(GamecubePage, ID_GC_EXIDEVICE_SLOTB_PATH, wxT("..."), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT, wxDefaultValidator);
	for (int i = 0; i < 3; ++i)
	{
		bool isMemcard = false;
		if (SConfig::GetInstance().m_EXIDevice[i] == EXIDEVICE_MEMORYCARD_A)
			isMemcard = GCEXIDevice[i]->SetStringSelection(SlotDevices[1]);
		else if (SConfig::GetInstance().m_EXIDevice[i] == EXIDEVICE_MEMORYCARD_B)
			isMemcard = GCEXIDevice[i]->SetStringSelection(SlotDevices[1]);
		else if (SConfig::GetInstance().m_EXIDevice[i] == EXIDEVICE_MIC)
			GCEXIDevice[i]->SetStringSelection(SlotDevices[2]);
		else if (SConfig::GetInstance().m_EXIDevice[i] == EXIDEVICE_ETH)
			GCEXIDevice[i]->SetStringSelection(SP1Devices[1]);
		else
			GCEXIDevice[i]->SetStringSelection(wxT("<Nothing>"));
		if (!isMemcard && i < 2)
			GCMemcardPath[i]->Disable();
	}
	//SI Devices
	wxStaticText *GCSIDeviceText[4];
	GCSIDeviceText[0] = new wxStaticText(GamecubePage, ID_GC_SIDEVICE_TEXT, wxT("Port 1"), wxDefaultPosition, wxDefaultSize);
	GCSIDeviceText[1] = new wxStaticText(GamecubePage, ID_GC_SIDEVICE_TEXT, wxT("Port 2"), wxDefaultPosition, wxDefaultSize);
	GCSIDeviceText[2] = new wxStaticText(GamecubePage, ID_GC_SIDEVICE_TEXT, wxT("Port 3"), wxDefaultPosition, wxDefaultSize);
	GCSIDeviceText[3] = new wxStaticText(GamecubePage, ID_GC_SIDEVICE_TEXT, wxT("Port 4"), wxDefaultPosition, wxDefaultSize);
	const wxString SIDevices[] = {wxT("<Nothing>"), wxT("Standard Controller"), wxT("GBA")};
	static const int numSIDevices = sizeof(SIDevices)/sizeof(wxString);
	GCSIDevice[0] = new wxChoice(GamecubePage, ID_GC_SIDEVICE0, wxDefaultPosition, wxDefaultSize, numSIDevices, SIDevices, 0, wxDefaultValidator);
	GCSIDevice[1] = new wxChoice(GamecubePage, ID_GC_SIDEVICE1, wxDefaultPosition, wxDefaultSize, numSIDevices, SIDevices, 0, wxDefaultValidator);
	GCSIDevice[2] = new wxChoice(GamecubePage, ID_GC_SIDEVICE2, wxDefaultPosition, wxDefaultSize, numSIDevices, SIDevices, 0, wxDefaultValidator);
	GCSIDevice[3] = new wxChoice(GamecubePage, ID_GC_SIDEVICE3, wxDefaultPosition, wxDefaultSize, numSIDevices, SIDevices, 0, wxDefaultValidator);
	for (int i = 0; i < 4; ++i)
	{
		if (SConfig::GetInstance().m_SIDevice[i] == SI_GC_CONTROLLER)
			GCSIDevice[i]->SetStringSelection(SIDevices[1]);
		else if (SConfig::GetInstance().m_SIDevice[i] == SI_GBA)
			GCSIDevice[i]->SetStringSelection(SIDevices[2]);
		else
			GCSIDevice[i]->SetStringSelection(SIDevices[0]);
	}

	sGamecube = new wxBoxSizer(wxVERTICAL);
	sGamecubeIPLSettings = new wxGridBagSizer(0, 0);
	sGamecubeIPLSettings->Add(GCSystemLangText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sGamecubeIPLSettings->Add(GCSystemLang, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALL, 5);
	sbGamecubeIPLSettings->Add(sGamecubeIPLSettings);
	sGamecube->Add(sbGamecubeIPLSettings, 0, wxEXPAND|wxALL, 5);
	wxBoxSizer *sEXIDevices[4];
	wxBoxSizer *sSIDevices[4];
	for (int i = 0; i < 3; ++i)
	{
		sEXIDevices[i] = new wxBoxSizer(wxHORIZONTAL);
		sEXIDevices[i]->Add(GCEXIDeviceText[i], 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
		sEXIDevices[i]->Add(GCEXIDevice[i], 0, wxALL, 5);
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
	sGamecube->Add(sbGamecubeDeviceSettings, 0, wxEXPAND|wxALL, 5);
	GamecubePage->SetSizer(sGamecube);
	sGamecube->Layout();


	//////////////////////////////////////////////////////////////////////////
	// Wii page
	sbWiimoteSettings = new wxStaticBoxSizer(wxVERTICAL, WiiPage, wxT("Wiimote Settings"));
	arrayStringFor_WiiSensBarPos.Add(wxT("Bottom")); arrayStringFor_WiiSensBarPos.Add(wxT("Top"));
	WiiSensBarPosText = new wxStaticText(WiiPage, ID_WII_BT_BAR_TEXT, wxT("Sensor Bar Position:"), wxDefaultPosition, wxDefaultSize);
	WiiSensBarPos = new wxChoice(WiiPage, ID_WII_BT_BAR, wxDefaultPosition, wxDefaultSize, arrayStringFor_WiiSensBarPos, 0, wxDefaultValidator);
	WiiSensBarPos->SetSelection(m_SYSCONF[BT_BAR]);

	sbWiiIPLSettings = new wxStaticBoxSizer(wxVERTICAL, WiiPage, wxT("IPL Settings"));
	WiiScreenSaver = new wxCheckBox(WiiPage, ID_WII_IPL_SSV, wxT("Enable Screen Saver (burn-in reduction)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiiScreenSaver->SetValue(m_SYSCONF[IPL_SSV]!=0);
	WiiProgressiveScan = new wxCheckBox(WiiPage, ID_WII_IPL_PGS, wxT("Enable Progressive Scan"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiiProgressiveScan->SetValue(m_SYSCONF[IPL_PGS]!=0);
	WiiEuRGB60 = new wxCheckBox(WiiPage, ID_WII_IPL_E60, wxT("Use EuRGB60 Mode (PAL6)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiiEuRGB60->SetValue(m_SYSCONF[IPL_E60]!=0);
	arrayStringFor_WiiAspectRatio.Add(wxT("4:3")); arrayStringFor_WiiAspectRatio.Add(wxT("16:9"));
	WiiAspectRatioText = new wxStaticText(WiiPage, ID_WII_IPL_AR_TEXT, wxT("Aspect Ratio:"), wxDefaultPosition, wxDefaultSize);
	WiiAspectRatio = new wxChoice(WiiPage, ID_WII_IPL_AR, wxDefaultPosition, wxDefaultSize, arrayStringFor_WiiAspectRatio, 0, wxDefaultValidator);
	WiiAspectRatio->SetSelection(m_SYSCONF[IPL_AR]);
	WiiSystemLangText = new wxStaticText(WiiPage, ID_WII_IPL_LNG_TEXT, wxT("System Language:"), wxDefaultPosition, wxDefaultSize);
	WiiSystemLang = new wxChoice(WiiPage, ID_WII_IPL_LNG, wxDefaultPosition, wxDefaultSize, arrayStringFor_WiiSystemLang, 0, wxDefaultValidator);
	WiiSystemLang->SetSelection(m_SYSCONF[IPL_LNG]);

	// Populate sbWiimoteSettings
	sWii = new wxBoxSizer(wxVERTICAL);
	sWiimoteSettings = new wxGridBagSizer(0, 0);
	sWiimoteSettings->Add(WiiSensBarPosText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sWiimoteSettings->Add(WiiSensBarPos, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALL, 5);
	sbWiimoteSettings->Add(sWiimoteSettings);
	sWii->Add(sbWiimoteSettings, 0, wxEXPAND|wxALL, 5);

	sWiiIPLSettings = new wxGridBagSizer(0, 0);
	sWiiIPLSettings->Add(WiiScreenSaver, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sWiiIPLSettings->Add(WiiProgressiveScan, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sWiiIPLSettings->Add(WiiEuRGB60, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sWiiIPLSettings->Add(WiiAspectRatioText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sWiiIPLSettings->Add(WiiAspectRatio, wxGBPosition(3, 1), wxGBSpan(1, 1), wxALL, 5);
	sWiiIPLSettings->Add(WiiSystemLangText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sWiiIPLSettings->Add(WiiSystemLang, wxGBPosition(4, 1), wxGBSpan(1, 1), wxALL, 5);
	sbWiiIPLSettings->Add(sWiiIPLSettings);
	sWii->Add(sbWiiIPLSettings, 0, wxEXPAND|wxALL, 5);
	WiiPage->SetSizer(sWii);
	sWii->Layout();


	//////////////////////////////////////////////////////////////////////////
	// Paths page
	sbISOPaths = new wxStaticBoxSizer(wxVERTICAL, PathsPage, wxT("ISO Directories"));
	ISOPaths = new wxListBox(PathsPage, ID_ISOPATHS, wxDefaultPosition, wxDefaultSize, arrayStringFor_ISOPaths, wxLB_SINGLE, wxDefaultValidator);
	AddISOPath = new wxButton(PathsPage, ID_ADDISOPATH, wxT("Add..."), wxDefaultPosition, wxDefaultSize, 0);
	RemoveISOPath = new wxButton(PathsPage, ID_REMOVEISOPATH, wxT("Remove"), wxDefaultPosition, wxDefaultSize, 0);
	RemoveISOPath->Enable(false);

	DefaultISOText = new wxStaticText(PathsPage, ID_DEFAULTISO_TEXT, wxT("Default ISO:"), wxDefaultPosition, wxDefaultSize);
	DefaultISO = new wxFilePickerCtrl(PathsPage, ID_DEFAULTISO, wxEmptyString, wxT("Choose a default ISO:"),
		wxString::Format(wxT("All GC/Wii images (gcm, iso, gcz)|*.gcm;*.iso;*.gcz|All files (%s)|%s"), wxFileSelectorDefaultWildcardStr, wxFileSelectorDefaultWildcardStr),
		wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL|wxFLP_FILE_MUST_EXIST|wxFLP_OPEN);
	DefaultISO->SetPath(wxString::FromAscii(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM.c_str()));

	DVDRootText = new wxStaticText(PathsPage, ID_DVDROOT_TEXT, wxT("DVD Root:"), wxDefaultPosition, wxDefaultSize);
	DVDRoot = new wxDirPickerCtrl(PathsPage, ID_DVDROOT, wxEmptyString, wxT("Choose a DVD root directory:"), wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
	DVDRoot->SetPath(wxString::FromAscii(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDVDRoot.c_str()));

	sPaths = new wxBoxSizer(wxVERTICAL);

	sbISOPaths->Add(ISOPaths, 1, wxEXPAND|wxALL, 0);

	sISOButtons = new wxBoxSizer(wxHORIZONTAL);
	sISOButtons->AddStretchSpacer(1);
	sISOButtons->Add(AddISOPath, 0, wxALL, 0);
	sISOButtons->Add(RemoveISOPath, 0, wxALL, 0);
	sbISOPaths->Add(sISOButtons, 0, wxEXPAND|wxALL, 5);
	sPaths->Add(sbISOPaths, 1, wxEXPAND|wxALL, 5);

	sOtherPaths = new wxGridBagSizer(0, 0);
	sOtherPaths->AddGrowableCol(1);
	sOtherPaths->Add(DefaultISOText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sOtherPaths->Add(DefaultISO, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sOtherPaths->Add(DVDRootText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sOtherPaths->Add(DVDRoot, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sPaths->Add(sOtherPaths, 0, wxEXPAND|wxALL, 5);
	PathsPage->SetSizer(sPaths);
	sPaths->Layout();

	//////////////////////////////////////////////////////////////////////////
	// Plugins page
	sbGraphicsPlugin = new wxStaticBoxSizer(wxHORIZONTAL, PluginPage, wxT("Graphics"));
	GraphicSelection = new wxChoice(PluginPage, ID_GRAPHIC_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	GraphicConfig = new wxButton(PluginPage, ID_GRAPHIC_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	sbDSPPlugin = new wxStaticBoxSizer(wxHORIZONTAL, PluginPage, wxT("DSP"));
	DSPSelection = new wxChoice(PluginPage, ID_DSP_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	DSPConfig = new wxButton(PluginPage, ID_DSP_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	sbPadPlugin = new wxStaticBoxSizer(wxHORIZONTAL, PluginPage, wxT("Pad"));
	PADSelection = new wxChoice(PluginPage, ID_PAD_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	PADConfig = new wxButton(PluginPage, ID_PAD_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	sbWiimotePlugin = new wxStaticBoxSizer(wxHORIZONTAL, PluginPage, wxT("Wiimote"));
	WiimoteSelection = new wxChoice(PluginPage, ID_WIIMOTE_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	WiimoteConfig = new wxButton(PluginPage, ID_WIIMOTE_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	FillChoiceBox(GraphicSelection, PLUGIN_TYPE_VIDEO, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin);
	FillChoiceBox(DSPSelection, PLUGIN_TYPE_DSP, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin);
	for (int i = 0; i < MAXPADS; i++)
	    FillChoiceBox(PADSelection, PLUGIN_TYPE_PAD, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin[i]);

	for (int i=0; i < MAXWIIMOTES; i++)
	    FillChoiceBox(WiimoteSelection, PLUGIN_TYPE_WIIMOTE, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin[i]);

	sPlugins = new wxBoxSizer(wxVERTICAL);
	sbGraphicsPlugin->Add(GraphicSelection, 1, wxEXPAND|wxALL, 5);
	sbGraphicsPlugin->Add(GraphicConfig, 0, wxALL, 5);
	sPlugins->Add(sbGraphicsPlugin, 0, wxEXPAND|wxALL, 5);

	sbDSPPlugin->Add(DSPSelection, 1, wxEXPAND|wxALL, 5);
	sbDSPPlugin->Add(DSPConfig, 0, wxALL, 5);
	sPlugins->Add(sbDSPPlugin, 0, wxEXPAND|wxALL, 5);

	sbPadPlugin->Add(PADSelection, 1, wxEXPAND|wxALL, 5);
	sbPadPlugin->Add(PADConfig, 0, wxALL, 5);
	sPlugins->Add(sbPadPlugin, 0, wxEXPAND|wxALL, 5);

	sbWiimotePlugin->Add(WiimoteSelection, 1, wxEXPAND|wxALL, 5);
	sbWiimotePlugin->Add(WiimoteConfig, 0, wxALL, 5);
	sPlugins->Add(sbWiimotePlugin, 0, wxEXPAND|wxALL, 5);
	PluginPage->SetSizer(sPlugins);
	sPlugins->Layout();



	m_Close = new wxButton(this, wxID_CLOSE);

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(0, 0, 1, wxEXPAND, 5);
	sButtons->Add(m_Close, 0, wxALL, 5);

	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);

	UpdateGUI();

	this->SetSizer(sMain);
	this->Layout();

	Fit();
	Center();
}

void CConfigMain::OnClose(wxCloseEvent& WXUNUSED (event))
{
	EndModal((bRefreshList) ? wxID_OK : wxID_CLOSE);

	// First check that we did successfully populate m_SYSCONF earlier, otherwise don't
	// save anything, it will be a corrupted file
	if(m_bSysconfOK)
	{
		// Save SYSCONF with the new settings
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

	// Save the config. Dolphin crashes to often to save the settings on closing only
	SConfig::GetInstance().SaveSettings();
}

void CConfigMain::CloseClick(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

//////////////////////////////////////////////////////////////////////////
// Core AND Interface settings
void CConfigMain::CoreSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_INTERFACE_CONFIRMSTOP: // Interface
		SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop = ConfirmStop->IsChecked();
		break;
	case ID_INTERFACE_AUTOHIDECURSOR:
		if (AutoHideCursor->IsChecked()) HideCursor->SetValue(!AutoHideCursor->IsChecked()); // Update the other one
		SConfig::GetInstance().m_LocalCoreStartupParameter.bAutoHideCursor = AutoHideCursor->IsChecked();		
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor = HideCursor->IsChecked();		
		break;
	case ID_INTERFACE_HIDECURSOR:
		if (HideCursor->IsChecked()) AutoHideCursor->SetValue(!HideCursor->IsChecked()); // Update the other one
		SConfig::GetInstance().m_LocalCoreStartupParameter.bAutoHideCursor = AutoHideCursor->IsChecked();		
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor = HideCursor->IsChecked();
		break;
	case ID_INTERFACE_THEME:
		SConfig::GetInstance().m_LocalCoreStartupParameter.iTheme = Theme->GetSelection();
		main_frame->InitBitmaps();
		break;
	case ID_INTERFACE_WIIMOTE_LEDS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiLeds = WiimoteStatusLEDs->IsChecked();
		break;
	case ID_INTERFACE_WIIMOTE_SPEAKERS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiSpeakers = WiimoteStatusSpeakers->IsChecked();
		break;
	case ID_INTERFACE_LANG:
		SConfig::GetInstance().m_InterfaceLanguage = (INTERFACE_LANGUAGE)InterfaceLang->GetSelection();
		bRefreshList = true;
		break;

	case ID_ALLWAYS_HLEBIOS: // Core
		SConfig::GetInstance().m_LocalCoreStartupParameter.bHLEBios = AllwaysHLEBIOS->IsChecked();
		break;
	case ID_USEDYNAREC:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bUseJIT = UseDynaRec->IsChecked();
		break;
	case ID_USEDUALCORE:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore = UseDualCore->IsChecked();
		break;
	case ID_LOCKTHREADS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bLockThreads = LockThreads->IsChecked();
		break;
	case ID_OPTIMIZEQUANTIZERS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bOptimizeQuantizers = OptimizeQuantizers->IsChecked();
		break;
	case ID_IDLESKIP:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle = SkipIdle->IsChecked();
		break;
	case ID_ENABLECHEATS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats = EnableCheats->IsChecked();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// GC settings
void CConfigMain::GCSettingsChanged(wxCommandEvent& event)
{
	int sidevice = 0;
	switch (event.GetId())
	{
	case ID_GC_SRAM_LNG:
		SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage = GCSystemLang->GetSelection();
		break;

 	case ID_GC_EXIDEVICE_SLOTA:
		switch (event.GetSelection())
		{
		case 1: // memcard
			SConfig::GetInstance().m_EXIDevice[0] = EXIDEVICE_MEMORYCARD_A;
			break;
		case 2: // mic
			SConfig::GetInstance().m_EXIDevice[0] = EXIDEVICE_MIC;
			break;
		default:
			SConfig::GetInstance().m_EXIDevice[0] = EXIDEVICE_DUMMY;
			break;
		}
		GCMemcardPath[0]->Enable(event.GetSelection() == 1);
 		break;
	case ID_GC_EXIDEVICE_SLOTA_PATH:
		ChooseMemcardPath(SConfig::GetInstance().m_strMemoryCardA, true);
		break;
	case ID_GC_EXIDEVICE_SLOTB:
		switch (event.GetSelection())
		{
		case 1: // memcard
			SConfig::GetInstance().m_EXIDevice[1] = EXIDEVICE_MEMORYCARD_B;
			break;
		case 2: // mic
			SConfig::GetInstance().m_EXIDevice[1] = EXIDEVICE_MIC;
			break;
		default:
			SConfig::GetInstance().m_EXIDevice[1] = EXIDEVICE_DUMMY;
			break;
		}
		GCMemcardPath[1]->Enable(event.GetSelection() == 1);
		break;
	case ID_GC_EXIDEVICE_SLOTB_PATH:
		ChooseMemcardPath(SConfig::GetInstance().m_strMemoryCardB, false);
		break;
	case ID_GC_EXIDEVICE_SP1: // The only thing we emulate on SP1 is the BBA
		SConfig::GetInstance().m_EXIDevice[2] = event.GetSelection() ? EXIDEVICE_ETH : EXIDEVICE_DUMMY;
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
	std::string filename = std::string(wxFileSelector
									   (wxT("Choose a file to open"),
										wxT(FULL_GC_USER_DIR), isSlotA ? wxT(GC_MEMCARDA):wxT(GC_MEMCARDB), 
										wxEmptyString,
		wxT("Gamecube Memory Cards (*.raw,*.gcp)|*.raw;*.gcp")).mb_str());
	if (!filename.empty())
		strMemcard = filename;
}

void CConfigMain::ChooseSIDevice(std::string deviceName, int deviceNum)
{
	TSIDevices tempType;
	if (deviceName.compare("Standard Controller") == 0)
		tempType = SI_GC_CONTROLLER;
	else if (deviceName.compare("GBA") == 0)
		tempType = SI_GBA;
	else
		tempType = SI_DUMMY;

	SConfig::GetInstance().m_SIDevice[deviceNum] = tempType;
}

//////////////////////////////////////////////////////////////////////////
// Wii settings
void CConfigMain::WiiSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_WII_BT_BAR: // Wiimote settings
		m_SYSCONF[BT_BAR] = WiiSensBarPos->GetSelection();
		break;

	case ID_WII_IPL_AR: // IPL settings
		m_SYSCONF[IPL_AR] = WiiAspectRatio->GetSelection();
		break;
	case ID_WII_IPL_SSV:
		m_SYSCONF[IPL_SSV] = WiiScreenSaver->IsChecked();
		break;
	case ID_WII_IPL_LNG:
		m_SYSCONF[IPL_LNG] = WiiSystemLang->GetSelection();
		break;
	case ID_WII_IPL_PGS:
		m_SYSCONF[IPL_PGS] = WiiProgressiveScan->IsChecked();
		break;
	case ID_WII_IPL_E60:
		m_SYSCONF[IPL_E60] = WiiEuRGB60->IsChecked();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// Paths settings
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
		wxString dirHome;
		wxGetHomeDir(&dirHome);

		wxDirDialog dialog(this, _T("Choose a directory to add"), dirHome, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

		if (dialog.ShowModal() == wxID_OK)
		{
			if (ISOPaths->FindString(dialog.GetPath()) != -1)
			{
				wxMessageBox(_("The chosen directory is already in the list"), _("Error"), wxOK);
			}
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
		SConfig::GetInstance().m_ISOFolder.push_back(std::string(ISOPaths->GetStrings()[i].ToAscii()));
}

void CConfigMain::DefaultISOChanged(wxFileDirPickerEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM = DefaultISO->GetPath().ToAscii();
}

void CConfigMain::DVDRootChanged(wxFileDirPickerEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDVDRoot = DVDRoot->GetPath().ToAscii();
}

//////////////////////////////////////////////////////////////////////////
// Plugin settings
void CConfigMain::OnSelectionChanged(wxCommandEvent& WXUNUSED (event))
{
	// Update plugin filenames
	GetFilename(GraphicSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin);
	GetFilename(DSPSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin);
	for (int i = 0; i < MAXPADS; i++)
		GetFilename(PADSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin[i]);	
	for (int i = 0; i < MAXWIIMOTES; i++)
		GetFilename(WiimoteSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin[i]);
}

void CConfigMain::OnConfig(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case ID_GRAPHIC_CONFIG:
		    CallConfig(GraphicSelection);
		    break;

		case ID_DSP_CONFIG:
		    CallConfig(DSPSelection);
		    break;

		case ID_PAD_CONFIG:
		    CallConfig(PADSelection);
		    break;

		case ID_WIIMOTE_CONFIG:
		    CallConfig(WiimoteSelection);
		    break;
	}
}

void CConfigMain::CallConfig(wxChoice* _pChoice)
{
	int Index = _pChoice->GetSelection();
	Console::Print("CallConfig: %i\n", Index);

	if (Index >= 0)
	{
		const CPluginInfo* pInfo = static_cast<CPluginInfo*>(_pChoice->GetClientData(Index));

		if (pInfo != NULL)
			CPluginManager::GetInstance().OpenConfig((HWND) this->GetHandle(), pInfo->GetFilename().c_str(), pInfo->GetPluginInfo().Type);
	}
}

void CConfigMain::FillChoiceBox(wxChoice* _pChoice, int _PluginType, const std::string& _SelectFilename)
{
	Console::Print("FillChoiceBox\n");

	_pChoice->Clear();

	int Index = -1;
	const CPluginInfos& rInfos = CPluginManager::GetInstance().GetPluginInfos();

	for (size_t i = 0; i < rInfos.size(); i++)
	{
		const PLUGIN_INFO& rPluginInfo = rInfos[i].GetPluginInfo();

		if (rPluginInfo.Type == _PluginType)
		{
			wxString temp;
			temp = wxString::FromAscii(rInfos[i].GetPluginInfo().Name);
			int NewIndex = _pChoice->Append(temp, (void*)&rInfos[i]);

			if (rInfos[i].GetFilename() == _SelectFilename)
			{
				Index = NewIndex;
			}
		}
	}

	_pChoice->Select(Index);
}

bool CConfigMain::GetFilename(wxChoice* _pChoice, std::string& _rFilename)
{	
	_rFilename.clear();
	int Index = _pChoice->GetSelection();

	if (Index >= 0)
	{
		const CPluginInfo* pInfo = static_cast<CPluginInfo*>(_pChoice->GetClientData(Index));
		_rFilename = pInfo->GetFilename();
		Console::Print("GetFilename: %i %s\n", Index, _rFilename.c_str());
		return(true);
	}

	return(false);
}
