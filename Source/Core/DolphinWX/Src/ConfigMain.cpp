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

#include "Config.h"

BEGIN_EVENT_TABLE(CConfigMain, wxDialog)

EVT_CLOSE(CConfigMain::OnClose)
EVT_BUTTON(ID_OK, CConfigMain::OKClick)
EVT_BUTTON(ID_APPLY, CConfigMain::OKClick)
EVT_BUTTON(ID_CANCEL, CConfigMain::OKClick)
EVT_CHECKBOX(ID_ALLWAYS_HLEBIOS, CConfigMain::AllwaysHLEBIOSCheck)
EVT_CHECKBOX(ID_USEDYNAREC, CConfigMain::UseDynaRecCheck)
EVT_CHECKBOX(ID_USEDUALCORE, CConfigMain::UseDualCoreCheck)
EVT_CHECKBOX(ID_LOCKTHREADS, CConfigMain::LockThreadsCheck)
EVT_CHECKBOX(ID_OPTIMIZEQUANTIZERS, CConfigMain::OptimizeQuantizersCheck)
EVT_CHECKBOX(ID_IDLESKIP, CConfigMain::SkipIdleCheck)
EVT_CHOICE(ID_CONSOLELANG, CConfigMain::ConsoleLangChanged)
EVT_BUTTON(ID_ADDISOPATH, CConfigMain::AddRemoveISOPaths)
EVT_BUTTON(ID_REMOVEISOPATH, CConfigMain::AddRemoveISOPaths)
EVT_FILEPICKER_CHANGED(ID_DEFAULTISO, CConfigMain::DefaultISOChanged)
EVT_DIRPICKER_CHANGED(ID_DVDROOT, CConfigMain::DVDRootChanged)
EVT_CHOICE(ID_GRAPHIC_CB, CConfigMain::OnSelectionChanged)
EVT_BUTTON(ID_GRAPHIC_ABOUT, CConfigMain::OnAbout)
EVT_BUTTON(ID_GRAPHIC_CONFIG, CConfigMain::OnConfig)
EVT_CHOICE(ID_DSP_CB, CConfigMain::OnSelectionChanged)
EVT_BUTTON(ID_DSP_ABOUT, CConfigMain::OnAbout)
EVT_BUTTON(ID_DSP_CONFIG, CConfigMain::OnConfig)
EVT_CHOICE(ID_PAD_CB, CConfigMain::OnSelectionChanged)
EVT_BUTTON(ID_PAD_ABOUT, CConfigMain::OnAbout)
EVT_BUTTON(ID_PAD_CONFIG, CConfigMain::OnConfig)
EVT_CHOICE(ID_WIIMOTE_CB, CConfigMain::OnSelectionChanged)
EVT_BUTTON(ID_WIIMOTE_ABOUT, CConfigMain::OnAbout)
EVT_BUTTON(ID_WIIMOTE_CONFIG, CConfigMain::OnConfig)

END_EVENT_TABLE()

CConfigMain::CConfigMain(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
}

CConfigMain::~CConfigMain()
{
}

void CConfigMain::CreateGUIControls()
{
	Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	GeneralPage = new wxPanel(Notebook, ID_GENERALPAGE, wxDefaultPosition, wxDefaultSize);
	Notebook->AddPage(GeneralPage, wxT("General"));
	PathsPage = new wxPanel(Notebook, ID_PATHSPAGE, wxDefaultPosition, wxDefaultSize);
	Notebook->AddPage(PathsPage, wxT("Paths"));
	PluginPage = new wxPanel(Notebook, ID_PLUGINPAGE, wxDefaultPosition, wxDefaultSize);
	Notebook->AddPage(PluginPage, wxT("Plugins"));

	OK = new wxButton(this, ID_OK, wxT("OK"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	Cancel = new wxButton(this, ID_CANCEL, wxT("Cancel"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	Apply = new wxButton(this, ID_APPLY, wxT("Apply"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	Apply->Disable();

	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(0, 0, 1, wxEXPAND, 5);
	sButtons->Add(OK, 0, wxALL, 5);
	sButtons->Add(Cancel, 0, wxALL, 5);
	sButtons->Add(Apply, 0, wxALL, 5);
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);
	
	this->SetSizer(sMain);
	this->Layout();

	// General page
	AllwaysHLEBIOS = new wxCheckBox(GeneralPage, ID_ALLWAYS_HLEBIOS, wxT("HLE the BIOS all the time"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	AllwaysHLEBIOS->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bHLEBios);
	UseDynaRec = new wxCheckBox(GeneralPage, ID_USEDYNAREC, wxT("Use dynamic recompilation"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	UseDynaRec->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bUseJIT);
	UseDualCore = new wxCheckBox(GeneralPage, ID_USEDUALCORE, wxT("Use dual core mode"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	UseDualCore->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore);
	LockThreads = new wxCheckBox(GeneralPage, ID_LOCKTHREADS, wxT("Lock threads to cores"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	LockThreads->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bLockThreads);
	OptimizeQuantizers = new wxCheckBox(GeneralPage, ID_OPTIMIZEQUANTIZERS, wxT("Optimize quantizers"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	OptimizeQuantizers->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bOptimizeQuantizers);
	SkipIdle = new wxCheckBox(GeneralPage, ID_IDLESKIP, wxT("Use idle skipping"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	SkipIdle->SetValue(SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle);
	wxArrayString arrayStringFor_ConsoleLang;
	arrayStringFor_ConsoleLang.Add(wxT("English"));
	arrayStringFor_ConsoleLang.Add(wxT("German"));
	arrayStringFor_ConsoleLang.Add(wxT("French"));
	arrayStringFor_ConsoleLang.Add(wxT("Spanish"));
	arrayStringFor_ConsoleLang.Add(wxT("Italian"));
	arrayStringFor_ConsoleLang.Add(wxT("Dutch"));
	ConsoleLangText = new wxStaticText(GeneralPage, ID_CONSOLELANG_TEXT, wxT("Console Language:"), wxDefaultPosition, wxDefaultSize);
	ConsoleLang = new wxChoice(GeneralPage, ID_CONSOLELANG, wxDefaultPosition, wxDefaultSize, arrayStringFor_ConsoleLang, 0, wxDefaultValidator);
	ConsoleLang->SetSelection(SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage);

	sGeneral = new wxGridBagSizer(0, 0);
	sGeneral->Add(AllwaysHLEBIOS, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sGeneral->Add(UseDynaRec, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sGeneral->Add(UseDualCore, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sGeneral->Add(LockThreads, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sGeneral->Add(OptimizeQuantizers, wxGBPosition(4, 0), wxGBSpan(1, 2), wxALL, 5);
	sGeneral->Add(SkipIdle, wxGBPosition(5, 0), wxGBSpan(1, 2), wxALL, 5);
	sGeneral->Add(ConsoleLangText, wxGBPosition(6, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sGeneral->Add(ConsoleLang, wxGBPosition(6, 1), wxGBSpan(1, 1), wxALL, 5);
	GeneralPage->SetSizer(sGeneral);
	sGeneral->Layout();

	// Paths page
	sbISOPaths = new wxStaticBoxSizer(wxVERTICAL, PathsPage, wxT("ISO Directories:"));
	for(u32 i = 0; i < SConfig::GetInstance().m_ISOFolder.size(); i++)
	{
		arrayStringFor_ISOPaths.Add(wxString(SConfig::GetInstance().m_ISOFolder[i].c_str(), wxConvUTF8));
	}
	ISOPaths = new wxListBox(PathsPage, ID_ISOPATHS, wxDefaultPosition, wxSize(290,150), arrayStringFor_ISOPaths, wxLB_SINGLE, wxDefaultValidator);
	AddISOPath = new wxButton(PathsPage, ID_ADDISOPATH, wxT("Add..."), wxDefaultPosition, wxDefaultSize, 0);
	RemoveISOPath = new wxButton(PathsPage, ID_REMOVEISOPATH, wxT("Remove"), wxDefaultPosition, wxDefaultSize, 0);

	sISOButtons = new wxBoxSizer(wxHORIZONTAL);
	sISOButtons->AddStretchSpacer(1);
	sISOButtons->Add(AddISOPath, 0, wxALL, 5);
	sISOButtons->Add(RemoveISOPath, 0, wxALL, 5);
	sbISOPaths->Add(ISOPaths, 1, wxEXPAND|wxALL, 5);
	sbISOPaths->Add(sISOButtons, 0, wxEXPAND|wxALL, 5);

	DefaultISOText = new wxStaticText(PathsPage, ID_DEFAULTISO_TEXT, wxT("Default ISO:"), wxDefaultPosition, wxDefaultSize);
	DefaultISO = new wxFilePickerCtrl(PathsPage, ID_DEFAULTISO, wxEmptyString, wxT("Choose a default ISO:"),
		wxString::Format(wxT("All GC/Wii images (gcm, iso, gcz)|*.gcm;*.iso;*.gcz|All files (%s)|%s"), wxFileSelectorDefaultWildcardStr, wxFileSelectorDefaultWildcardStr),
		wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL|wxFLP_FILE_MUST_EXIST|wxFLP_OPEN);
	DefaultISO->SetPath(wxString::FromAscii(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM.c_str()));

	DVDRootText = new wxStaticText(PathsPage, ID_DVDROOT_TEXT, wxT("DVD Root:"), wxDefaultPosition, wxDefaultSize);
	DVDRoot = new wxDirPickerCtrl(PathsPage, ID_DVDROOT, wxEmptyString, wxT("Choose a DVD root directory:"), wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
	DVDRoot->SetPath(wxString::FromAscii(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDVDRoot.c_str()));

	sPaths = new wxGridBagSizer(0, 0);
	sPaths->Add(sbISOPaths, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL|wxEXPAND, 5);
	sPaths->Add(DefaultISOText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sPaths->Add(DefaultISO, wxGBPosition(1, 1), wxGBSpan(1, 1), wxALL|wxEXPAND, 5);
	sPaths->Add(DVDRootText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sPaths->Add(DVDRoot, wxGBPosition(2, 1), wxGBSpan(1, 1), wxALL|wxEXPAND, 5);
	PathsPage->SetSizer(sPaths);
	sPaths->Layout();

	// Plugin page
	GraphicSelection = new wxChoice(PluginPage, ID_GRAPHIC_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	GraphicAbout = new wxButton(PluginPage, ID_GRAPHIC_ABOUT, wxT("About..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	GraphicConfig = new wxButton(PluginPage, ID_GRAPHIC_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	GraphicText = new wxStaticText(PluginPage, ID_GRAPHIC_TEXT, wxT("GFX:"), wxDefaultPosition, wxDefaultSize);

	FillChoiceBox(GraphicSelection, PLUGIN_TYPE_VIDEO, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin);

	DSPSelection = new wxChoice(PluginPage, ID_DSP_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	DSPAbout = new wxButton(PluginPage, ID_DSP_ABOUT, wxT("About..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	DSPConfig = new wxButton(PluginPage, ID_DSP_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	DSPText = new wxStaticText(PluginPage, ID_DSP_TEXT, wxT("DSP:"), wxDefaultPosition, wxDefaultSize);

	FillChoiceBox(DSPSelection, PLUGIN_TYPE_DSP, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin);

	PADSelection = new wxChoice(PluginPage, ID_PAD_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	PADAbout = new wxButton(PluginPage, ID_PAD_ABOUT, wxT("About..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	PADConfig = new wxButton(PluginPage, ID_PAD_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	PADText = new wxStaticText(PluginPage, ID_PAD_TEXT, wxT("PAD:"), wxDefaultPosition, wxDefaultSize);

	FillChoiceBox(PADSelection, PLUGIN_TYPE_PAD, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin);

	WiimoteSelection = new wxChoice(PluginPage, ID_WIIMOTE_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	WiimoteAbout = new wxButton(PluginPage, ID_WIIMOTE_ABOUT, wxT("About..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiimoteConfig = new wxButton(PluginPage, ID_WIIMOTE_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiimoteText = new wxStaticText(PluginPage, ID_WIIMOTE_TEXT, wxT("Wiimote:"), wxDefaultPosition, wxDefaultSize);

	FillChoiceBox(WiimoteSelection, PLUGIN_TYPE_WIIMOTE, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin);

	sPlugins = new wxGridBagSizer(0, 0);
	sPlugins->Add(GraphicText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxLEFT, 5);
	sPlugins->Add(GraphicSelection, wxGBPosition(0, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sPlugins->Add(GraphicConfig, wxGBPosition(1, 1), wxGBSpan(1, 1), wxLEFT|wxBOTTOM, 5);
	sPlugins->Add(GraphicAbout, wxGBPosition(1, 2), wxGBSpan(1, 1), wxLEFT|wxBOTTOM, 5);

	sPlugins->Add(DSPText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxLEFT, 5);
	sPlugins->Add(DSPSelection, wxGBPosition(2, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sPlugins->Add(DSPConfig, wxGBPosition(3, 1), wxGBSpan(1, 1), wxLEFT|wxBOTTOM, 5);
	sPlugins->Add(DSPAbout, wxGBPosition(3, 2), wxGBSpan(1, 1), wxLEFT|wxBOTTOM, 5);

	sPlugins->Add(PADText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxLEFT, 5);
	sPlugins->Add(PADSelection, wxGBPosition(4, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sPlugins->Add(PADConfig, wxGBPosition(5, 1), wxGBSpan(1, 1), wxLEFT|wxBOTTOM, 5);
	sPlugins->Add(PADAbout, wxGBPosition(5, 2), wxGBSpan(1, 1), wxLEFT|wxBOTTOM, 5);

	sPlugins->Add(WiimoteText, wxGBPosition(6, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxLEFT, 5);
	sPlugins->Add(WiimoteSelection, wxGBPosition(6, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sPlugins->Add(WiimoteConfig, wxGBPosition(7, 1), wxGBSpan(1, 1), wxLEFT|wxBOTTOM, 5);
	sPlugins->Add(WiimoteAbout, wxGBPosition(7, 2), wxGBSpan(1, 1), wxLEFT|wxBOTTOM, 5);

	PluginPage->SetSizer(sPlugins);
	sPlugins->Layout();

	SetIcon(wxNullIcon);
	Fit();
}

void CConfigMain::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}

void CConfigMain::OKClick(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case ID_OK:
		    DoApply();
		    Destroy();
		    break;

	    case ID_APPLY:
		    DoApply();
		    break;

	    case ID_CANCEL:
		    Destroy();
		    break;
	}
}

void CConfigMain::AllwaysHLEBIOSCheck(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bHLEBios = AllwaysHLEBIOS->IsChecked();
}

void CConfigMain::UseDynaRecCheck(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bUseJIT = UseDynaRec->IsChecked();
}

void CConfigMain::UseDualCoreCheck(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore = UseDualCore->IsChecked();
}

void CConfigMain::LockThreadsCheck(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bLockThreads = LockThreads->IsChecked();
}

void CConfigMain::OptimizeQuantizersCheck(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bOptimizeQuantizers = OptimizeQuantizers->IsChecked();
}

void CConfigMain::SkipIdleCheck(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle = SkipIdle->IsChecked();
}

void CConfigMain::ConsoleLangChanged(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage = ConsoleLang->GetSelection();
}

void CConfigMain::AddRemoveISOPaths(wxCommandEvent& event)
{
	if(event.GetId() == ID_ADDISOPATH)
	{
		wxString dirHome;
		wxGetHomeDir(&dirHome);

		wxDirDialog dialog(this, _T("Choose a directory to add"), dirHome, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

		if (dialog.ShowModal() == wxID_OK)
		{
			ISOPaths->Append(dialog.GetPath());
		}
	}
	else
	{
		ISOPaths->Delete(ISOPaths->GetSelection());
	}

	//save changes right away
	SConfig::GetInstance().m_ISOFolder.clear();

	for(unsigned int i = 0; i < ISOPaths->GetCount(); i++)
	{
		SConfig::GetInstance().m_ISOFolder.push_back(std::string(ISOPaths->GetStrings()[i].ToAscii()));
	}
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
	Apply->Enable();
}

void CConfigMain::OnAbout(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case ID_GRAPHIC_ABOUT:
		    CallAbout(GraphicSelection);
		    break;

		case ID_DSP_ABOUT:
		    CallAbout(DSPSelection);
		    break;

		case ID_PAD_ABOUT:
		    CallAbout(PADSelection);
		    break;

		case ID_WIIMOTE_ABOUT:
		    CallAbout(WiimoteSelection);
		    break;
	}
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

void CConfigMain::CallAbout(wxChoice* _pChoice)
{
	int Index = _pChoice->GetSelection();

	if (Index >= 0)
	{
		const CPluginInfo* pInfo = static_cast<CPluginInfo*>(_pChoice->GetClientData(Index));

		if (pInfo != NULL)
			CPluginManager::GetInstance().OpenAbout((HWND) this->GetHandle(), pInfo->GetFileName().c_str());
	}
}

void CConfigMain::DoApply()
{
	Apply->Disable();

	GetFilename(GraphicSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin);
	GetFilename(DSPSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin);
	GetFilename(PADSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin);
	GetFilename(WiimoteSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin);

	SConfig::GetInstance().SaveSettings();
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
