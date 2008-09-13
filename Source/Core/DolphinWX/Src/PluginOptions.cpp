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

#include "PluginOptions.h"
#include "PluginManager.h"

#include "Config.h"

BEGIN_EVENT_TABLE(CPluginOptions, wxDialog)

EVT_CLOSE(CPluginOptions::OnClose)
EVT_BUTTON(ID_OK, CPluginOptions::OKClick)
EVT_BUTTON(ID_APPLY, CPluginOptions::OKClick)
EVT_BUTTON(ID_CANCEL, CPluginOptions::OKClick)
EVT_CHOICE(ID_GRAPHIC_CB, CPluginOptions::OnSelectionChanged)
EVT_BUTTON(ID_GRAPHIC_ABOUT, CPluginOptions::OnAbout)
EVT_BUTTON(ID_GRAPHIC_CONFIG, CPluginOptions::OnConfig)
EVT_CHOICE(ID_DSP_CB, CPluginOptions::OnSelectionChanged)
EVT_BUTTON(ID_DSP_ABOUT, CPluginOptions::OnAbout)
EVT_BUTTON(ID_DSP_CONFIG, CPluginOptions::OnConfig)
EVT_CHOICE(ID_PAD_CB, CPluginOptions::OnSelectionChanged)
EVT_BUTTON(ID_PAD_ABOUT, CPluginOptions::OnAbout)
EVT_BUTTON(ID_PAD_CONFIG, CPluginOptions::OnConfig)
EVT_CHOICE(ID_WIIMOTE_CB, CPluginOptions::OnSelectionChanged)
EVT_BUTTON(ID_WIIMOTE_ABOUT, CPluginOptions::OnAbout)
EVT_BUTTON(ID_WIIMOTE_CONFIG, CPluginOptions::OnConfig)

END_EVENT_TABLE()


CPluginOptions::CPluginOptions(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
}


CPluginOptions::~CPluginOptions()
{}


void CPluginOptions::CreateGUIControls()
{
	OK = new wxButton(this, ID_OK, wxT("OK"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	Cancel = new wxButton(this, ID_CANCEL, wxT("Cancel"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	Apply = new wxButton(this, ID_APPLY, wxT("Apply"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	Apply->Disable();

	GraphicSelection = new wxChoice(this, ID_GRAPHIC_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	GraphicAbout = new wxButton(this, ID_GRAPHIC_ABOUT, wxT("About..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	GraphicConfig = new wxButton(this, ID_GRAPHIC_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	GraphicText = new wxStaticText(this, ID_GRAPHIC_TEXT, wxT("GFX:"), wxDefaultPosition, wxDefaultSize);

	FillChoiceBox(GraphicSelection, PLUGIN_TYPE_VIDEO, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin);

	DSPSelection = new wxChoice(this, ID_DSP_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	DSPAbout = new wxButton(this, ID_DSP_ABOUT, wxT("About..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	DSPConfig = new wxButton(this, ID_DSP_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	DSPText = new wxStaticText(this, ID_DSP_TEXT, wxT("DSP:"), wxDefaultPosition, wxDefaultSize);

	FillChoiceBox(DSPSelection, PLUGIN_TYPE_DSP, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin);

	PADSelection = new wxChoice(this, ID_PAD_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	PADAbout = new wxButton(this, ID_PAD_ABOUT, wxT("About..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	PADConfig = new wxButton(this, ID_PAD_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	PADText = new wxStaticText(this, ID_PAD_TEXT, wxT("PAD:"), wxDefaultPosition, wxDefaultSize);

	FillChoiceBox(PADSelection, PLUGIN_TYPE_PAD, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin);

	WiimoteSelection = new wxChoice(this, ID_WIIMOTE_CB, wxDefaultPosition, wxDefaultSize, NULL, 0, wxDefaultValidator);
	WiimoteAbout = new wxButton(this, ID_WIIMOTE_ABOUT, wxT("About..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiimoteConfig = new wxButton(this, ID_WIIMOTE_CONFIG, wxT("Config..."), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	WiimoteText = new wxStaticText(this, ID_WIIMOTE_TEXT, wxT("Wiimote:"), wxDefaultPosition, wxDefaultSize);

	FillChoiceBox(WiimoteSelection, PLUGIN_TYPE_WIIMOTE, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin);

	wxGridBagSizer* sConfig;
	sConfig = new wxGridBagSizer(0, 0);
	sConfig->SetFlexibleDirection(wxBOTH);
	sConfig->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	sConfig->Add(GraphicText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sConfig->Add(GraphicSelection, wxGBPosition(0, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sConfig->Add(GraphicConfig, wxGBPosition(0, 3), wxGBSpan(1, 1), wxALL, 5);
	sConfig->Add(GraphicAbout, wxGBPosition(0, 4), wxGBSpan(1, 1), wxALL, 5);

	sConfig->Add(DSPText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sConfig->Add(DSPSelection, wxGBPosition(1, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sConfig->Add(DSPConfig, wxGBPosition(1, 3), wxGBSpan(1, 1), wxALL, 5);
	sConfig->Add(DSPAbout, wxGBPosition(1, 4), wxGBSpan(1, 1), wxALL, 5);

	sConfig->Add(PADText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sConfig->Add(PADSelection, wxGBPosition(2, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sConfig->Add(PADConfig, wxGBPosition(2, 3), wxGBSpan(1, 1), wxALL, 5);
	sConfig->Add(PADAbout, wxGBPosition(2, 4), wxGBSpan(1, 1), wxALL, 5);

	sConfig->Add(WiimoteText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sConfig->Add(WiimoteSelection, wxGBPosition(3, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sConfig->Add(WiimoteConfig, wxGBPosition(3, 3), wxGBSpan(1, 1), wxALL, 5);
	sConfig->Add(WiimoteAbout, wxGBPosition(3, 4), wxGBSpan(1, 1), wxALL, 5);

	sConfig->Layout();

	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(0, 0, 1, wxEXPAND, 5);
	sButtons->Add(OK, 0, wxALL, 5);
	sButtons->Add(Cancel, 0, wxALL, 5);
	sButtons->Add(Apply, 0, wxALL, 5);
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(sConfig, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);
	
	Center();
	this->SetSizer(sMain);
	sMain->SetSizeHints(this);
}


void CPluginOptions::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}


void CPluginOptions::OKClick(wxCommandEvent& event)
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


void CPluginOptions::OnSelectionChanged(wxCommandEvent& WXUNUSED (event))
{
	Apply->Enable();
}


void CPluginOptions::OnAbout(wxCommandEvent& event)
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


void CPluginOptions::OnConfig(wxCommandEvent& event)
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


void CPluginOptions::FillChoiceBox(wxChoice* _pChoice, int _PluginType, const std::string& _SelectFilename)
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


void CPluginOptions::CallConfig(wxChoice* _pChoice)
{
	int Index = _pChoice->GetSelection();

	if (Index >= 0)
	{
		const CPluginInfo* pInfo = static_cast<CPluginInfo*>(_pChoice->GetClientData(Index));

		if (pInfo != NULL)
		{
			CPluginManager::GetInstance().OpenConfig((HWND) this->GetHandle(), pInfo->GetFileName());
		}
	}
}


void CPluginOptions::CallAbout(wxChoice* _pChoice)
{
	int Index = _pChoice->GetSelection();

	if (Index >= 0)
	{
		const CPluginInfo* pInfo = static_cast<CPluginInfo*>(_pChoice->GetClientData(Index));

		if (pInfo != NULL)
		{
			CPluginManager::GetInstance().OpenAbout((HWND) this->GetHandle(), pInfo->GetFileName());
		}
	}
}


void CPluginOptions::DoApply()
{
	Apply->Disable();

	GetFilename(GraphicSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin);
	GetFilename(DSPSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin);
	GetFilename(PADSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin);
	GetFilename(WiimoteSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin);

	SConfig::GetInstance().SaveSettings();
}


bool CPluginOptions::GetFilename(wxChoice* _pChoice, std::string& _rFilename)
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

