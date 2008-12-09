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

#include "ConfigMain.h"
#include "PluginManager.h"
#include "Frame.h"

extern CFrame* main_frame;

BEGIN_EVENT_TABLE(CConfigMain, wxDialog)

EVT_CLOSE(CConfigMain::OnClose)
EVT_BUTTON(ID_CLOSE, CConfigMain::CloseClick)
EVT_CHECKBOX(ID_ALLWAYS_HLEBIOS, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_USEDYNAREC, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_USEDUALCORE, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_LOCKTHREADS, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_OPTIMIZEQUANTIZERS, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_IDLESKIP, CConfigMain::CoreSettingsChanged)
EVT_CHECKBOX(ID_ENABLECHEATS, CConfigMain::CoreSettingsChanged)
EVT_CHOICE(ID_GC_SRAM_LNG, CConfigMain::GCSettingsChanged)

EVT_CHOICE(ID_WII_BT_BAR, CConfigMain::WiiSettingsChanged)
EVT_CHECKBOX(ID_WII_BT_LEDS, CConfigMain::WiiSettingsChanged)
EVT_CHECKBOX(ID_WII_BT_SPEAKERS, CConfigMain::WiiSettingsChanged)
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
	for(u32 i = 0; i < SConfig::GetInstance().m_ISOFolder.size(); i++)
	{
		ISOPaths->Append(wxString(SConfig::GetInstance().m_ISOFolder[i].c_str(), wxConvUTF8));
	}
}

CConfigMain::~CConfigMain()
{
}

void CConfigMain::CreateGUIControls()
{
	Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	GeneralPage = new wxPanel(Notebook, ID_GENERALPAGE, wxDefaultPosition, wxDefaultSize);
	Notebook->AddPage(GeneralPage, wxT("Core"));
	GamecubePage = new wxPanel(Notebook, ID_GAMECUBEPAGE, wxDefaultPosition, wxDefaultSize);
	Notebook->AddPage(GamecubePage, wxT("Gamecube"));
	WiiPage = new wxPanel(Notebook, ID_WIIPAGE, wxDefaultPosition, wxDefaultSize);
	Notebook->AddPage(WiiPage, wxT("Wii"));
	PathsPage = new wxPanel(Notebook, ID_PATHSPAGE, wxDefaultPosition, wxDefaultSize);
	Notebook->AddPage(PathsPage, wxT("Paths"));
	PluginPage = new wxPanel(Notebook, ID_PLUGINPAGE, wxDefaultPosition, wxDefaultSize);
	Notebook->AddPage(PluginPage, wxT("Plugins"));

	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(0, 0, 1, wxEXPAND, 5);
	sButtons->Add(m_Close, 0, wxALL, 5);
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);
	
	this->SetSizer(sMain);
	this->Layout();

	// Core page
	sbBasic = new wxStaticBoxSizer(wxVERTICAL, GeneralPage, wxT("Basic Settings"));
	UseDualCore = new wxCheckBox(GeneralPage, ID_USEDUALCORE, wxT("Enable Dual Core"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	UseDualCore->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore);
	SkipIdle = new wxCheckBox(GeneralPage, ID_IDLESKIP, wxT("Enable Idle Skipping"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	SkipIdle->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle);
	EnableCheats = new wxCheckBox(GeneralPage, ID_ENABLECHEATS, wxT("Enable Cheats"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	EnableCheats->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats);

	sbAdvanced = new wxStaticBoxSizer(wxVERTICAL, GeneralPage, wxT("Advanced Settings"));
	AllwaysHLEBIOS = new wxCheckBox(GeneralPage, ID_ALLWAYS_HLEBIOS, wxT("HLE the BIOS all the time"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	AllwaysHLEBIOS->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bHLEBios);
	UseDynaRec = new wxCheckBox(GeneralPage, ID_USEDYNAREC, wxT("Enable Dynamic Recompilation"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	UseDynaRec->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bUseJIT);
	LockThreads = new wxCheckBox(GeneralPage, ID_LOCKTHREADS, wxT("Lock threads to cores"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	LockThreads->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bLockThreads);
	OptimizeQuantizers = new wxCheckBox(GeneralPage, ID_OPTIMIZEQUANTIZERS, wxT("Optimize Quantizers"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	OptimizeQuantizers->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bOptimizeQuantizers);

	sGeneral = new wxBoxSizer(wxVERTICAL);
	sbBasic->Add(UseDualCore, 0, wxALL, 5);
	sbBasic->Add(SkipIdle, 0, wxALL, 5);
	sbBasic->Add(EnableCheats, 0, wxALL, 5);
	sGeneral->Add(sbBasic, 0, wxEXPAND|wxALL, 5);
	sGeneral->AddStretchSpacer();

	sbAdvanced->Add(AllwaysHLEBIOS, 0, wxALL, 5);
	sbAdvanced->Add(UseDynaRec, 0, wxALL, 5);
	sbAdvanced->Add(LockThreads, 0, wxALL, 5);
	sbAdvanced->Add(OptimizeQuantizers, 0, wxALL, 5);
	sGeneral->Add(sbAdvanced, 0, wxEXPAND|wxALL, 5);
	GeneralPage->SetSizer(sGeneral);
	sGeneral->Layout();

	// Gamecube page
	sbGamecubeIPLSettings = new wxStaticBoxSizer(wxVERTICAL, GamecubePage, wxT("IPL Settings"));
	arrayStringFor_GCSystemLang.Add(wxT("English"));
	arrayStringFor_GCSystemLang.Add(wxT("German"));
	arrayStringFor_GCSystemLang.Add(wxT("French"));
	arrayStringFor_GCSystemLang.Add(wxT("Spanish"));
	arrayStringFor_GCSystemLang.Add(wxT("Italian"));
	arrayStringFor_GCSystemLang.Add(wxT("Dutch"));
	GCSystemLangText = new wxStaticText(GamecubePage, ID_GC_SRAM_LNG_TEXT, wxT("System Language:"), wxDefaultPosition, wxDefaultSize);
	GCSystemLang = new wxChoice(GamecubePage, ID_GC_SRAM_LNG, wxDefaultPosition, wxDefaultSize, arrayStringFor_GCSystemLang, 0, wxDefaultValidator);
	GCSystemLang->SetSelection(SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage);

	sGamecube = new wxBoxSizer(wxVERTICAL);
	sGamecubeIPLSettings = new wxGridBagSizer(0, 0);
	sGamecubeIPLSettings->Add(GCSystemLangText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sGamecubeIPLSettings->Add(GCSystemLang, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALL, 5);
	sbGamecubeIPLSettings->Add(sGamecubeIPLSettings);
	sGamecube->Add(sbGamecubeIPLSettings, 0, wxEXPAND|wxALL, 5);
	GamecubePage->SetSizer(sGamecube);
	sGamecube->Layout();


	//////////////////////////////////
	// Wii page
	// --------
	sbWiimoteSettings = new wxStaticBoxSizer(wxVERTICAL, WiiPage, wxT("Wiimote Settings"));
	arrayStringFor_WiiSensBarPos.Add(wxT("Bottom")); arrayStringFor_WiiSensBarPos.Add(wxT("Top"));
	WiiSensBarPosText = new wxStaticText(WiiPage, ID_WII_BT_BAR_TEXT, wxT("Sensor Bar Position:"), wxDefaultPosition, wxDefaultSize);
	WiiSensBarPos = new wxChoice(WiiPage, ID_WII_BT_BAR, wxDefaultPosition, wxDefaultSize, arrayStringFor_WiiSensBarPos, 0, wxDefaultValidator);
	WiiSensBarPos->SetSelection(m_SYSCONF[BT_BAR]);
	WiiLeds = new wxCheckBox(WiiPage, ID_WII_BT_LEDS, wxT("Show Wiimote Leds in status bar"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiiLeds->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiLeds);
	WiiSpeakers = new wxCheckBox(WiiPage, ID_WII_BT_SPEAKERS, wxT("Show Wiimote Speaker status in status bar"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiiSpeakers->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiSpeakers);

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
	arrayStringFor_WiiSystemLang = arrayStringFor_GCSystemLang;
	arrayStringFor_WiiSystemLang.Insert(wxT("Japanese"), 0);
	WiiSystemLangText = new wxStaticText(WiiPage, ID_WII_IPL_LNG_TEXT, wxT("System Language:"), wxDefaultPosition, wxDefaultSize);
	WiiSystemLang = new wxChoice(WiiPage, ID_WII_IPL_LNG, wxDefaultPosition, wxDefaultSize, arrayStringFor_WiiSystemLang, 0, wxDefaultValidator);
	WiiSystemLang->SetSelection(m_SYSCONF[IPL_LNG]);

	// Populate sbWiimoteSettings
	sWii = new wxBoxSizer(wxVERTICAL);
	sWiimoteSettings = new wxGridBagSizer(0, 0);
	sWiimoteSettings->Add(WiiSensBarPosText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sWiimoteSettings->Add(WiiSensBarPos, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALL, 5);
	sWiimoteSettings->Add(WiiLeds, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sWiimoteSettings->Add(WiiSpeakers, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
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

	// Plugin page
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
	FillChoiceBox(PADSelection, PLUGIN_TYPE_PAD, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin);
	FillChoiceBox(WiimoteSelection, PLUGIN_TYPE_WIIMOTE, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin);

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
	Fit();
	Center();
}

void CConfigMain::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();

	/* First check that we did successfully populate m_SYSCONF earlier, otherwise don't 
       save anything, it will be a corrupted file */
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

	// save the config... dolphin crashes by far to often to save the settings on closing only
	SConfig::GetInstance().SaveSettings();
	
	// Update the status bar
	main_frame->ModifyStatusBar();
}

void CConfigMain::CloseClick(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

void CConfigMain::CoreSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_ALLWAYS_HLEBIOS:
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

void CConfigMain::GCSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_GC_SRAM_LNG:
		SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage = GCSystemLang->GetSelection();
		break;
	}
}

void CConfigMain::WiiSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_WII_BT_BAR: // Wiimote settings
		m_SYSCONF[BT_BAR] = WiiSensBarPos->GetSelection();
		break;
	case ID_WII_BT_LEDS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiLeds = WiiLeds->IsChecked();
		break;
	case ID_WII_BT_SPEAKERS:
		SConfig::GetInstance().m_LocalCoreStartupParameter.bWiiSpeakers = WiiSpeakers->IsChecked();
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

void CConfigMain::OnSelectionChanged(wxCommandEvent& WXUNUSED (event))
{
	GetFilename(GraphicSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin);
	GetFilename(DSPSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin);
	GetFilename(PADSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin);
	GetFilename(WiimoteSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin);
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

void CConfigMain::FillChoiceBox(wxChoice* _pChoice, int _PluginType, const std::string& _SelectFilename)
{
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

			if (rInfos[i].GetFileName() == _SelectFilename)
			{
				Index = NewIndex;
			}
		}
	}

	_pChoice->Select(Index);
}

void CConfigMain::CallConfig(wxChoice* _pChoice)
{
	int Index = _pChoice->GetSelection();

	if (Index >= 0)
	{
		const CPluginInfo* pInfo = static_cast<CPluginInfo*>(_pChoice->GetClientData(Index));

		if (pInfo != NULL)
			CPluginManager::GetInstance().OpenConfig((HWND) this->GetHandle(), pInfo->GetFileName().c_str());
	}
}

bool CConfigMain::GetFilename(wxChoice* _pChoice, std::string& _rFilename)
{
	_rFilename.clear();

	int Index = _pChoice->GetSelection();
	printf("%i\n", Index);

	if (Index >= 0)
	{
		const CPluginInfo* pInfo = static_cast<CPluginInfo*>(_pChoice->GetClientData(Index));
		_rFilename = pInfo->GetFileName();
		printf("%s\n", _rFilename.c_str());
		return(true);
	}

	return(false);
}

