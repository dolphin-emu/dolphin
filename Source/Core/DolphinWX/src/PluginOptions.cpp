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
EVT_CHOICE(ID_PAD_CB, CPluginOptions::OnSelectionChanged)
EVT_CHOICE(ID_DSP_CB, CPluginOptions::OnSelectionChanged)
EVT_CHOICE(ID_GRAPHIC_CB, CPluginOptions::OnSelectionChanged)
EVT_BUTTON(ID_PAD_ABOUT, CPluginOptions::OnAbout)
EVT_BUTTON(ID_DSP_ABOUT, CPluginOptions::OnAbout)
EVT_BUTTON(ID_GRAPHIC_ABOUT, CPluginOptions::OnAbout)
EVT_BUTTON(ID_PAD_CONFIG, CPluginOptions::OnConfig)
EVT_BUTTON(ID_DSP_CONFIG, CPluginOptions::OnConfig)
EVT_BUTTON(ID_GRAPHIC_CONFIG, CPluginOptions::OnConfig)

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
	SetTitle(wxT("Plugin Selection"));
	SetIcon(wxNullIcon);
	SetSize(0, 8, 440, 295);
	Center();
	const wxChar* font_name = 0;
	int font_size = 8;
#ifdef _WIN32
	OSVERSIONINFOEX os;
	os.dwOSVersionInfoSize = sizeof(os);
	GetVersionEx((OSVERSIONINFO*)&os);

	if (os.dwMajorVersion >= 6)
	{
		font_name = wxT("Segoe UI");
		font_size = 9;
	}
	else
	{
		font_name = wxT("Tahoma");
	}

	//
#endif

	wxFont font(font_size, wxSWISS, wxNORMAL, wxNORMAL, false, font_name);
	OK = new wxButton(this, ID_OK, wxT("OK"), wxPoint(188, 240), wxSize(73, 25), 0, wxDefaultValidator, wxT("OK"));
	OK->SetFont(font);

	Cancel = new wxButton(this, ID_CANCEL, wxT("Cancel"), wxPoint(268, 240), wxSize(73, 25), 0, wxDefaultValidator, wxT("Cancel"));
	Cancel->SetFont(font);

	Apply = new wxButton(this, ID_APPLY, wxT("Apply"), wxPoint(348, 240), wxSize(73, 25), 0, wxDefaultValidator, wxT("Apply"));
	Apply->SetFont(font);
	Apply->Disable();

	{
		GraphicSelection = new wxChoice(this, ID_GRAPHIC_CB, wxPoint(88, 16), wxSize(333, 23), NULL, 0, wxDefaultValidator, wxT("GraphicSelection"));
		GraphicSelection->SetFont(font);

		GraphicAbout = new wxButton(this, ID_GRAPHIC_ABOUT, wxT("About..."), wxPoint(168, 48), wxSize(73, 25), 0, wxDefaultValidator, wxT("GraphicAbout"));
		GraphicAbout->SetFont(font);

		GraphicConfig = new wxButton(this, ID_GRAPHIC_CONFIG, wxT("Config..."), wxPoint(88, 48), wxSize(73, 25), 0, wxDefaultValidator, wxT("GraphicConfig"));
		GraphicConfig->SetFont(font);

		WxStaticText1 = new wxStaticText(this, ID_WXSTATICTEXT1, wxT("Graphic"), wxPoint(16, 21), wxDefaultSize, 0, wxT("WxStaticText1"));
		WxStaticText1->SetFont(font);

		FillChoiceBox(GraphicSelection, PLUGIN_TYPE_VIDEO, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin);
	}

	{
		DSPSelection = new wxChoice(this, ID_DSP_CB, wxPoint(88, 88), wxSize(333, 23), NULL, 0, wxDefaultValidator, wxT("DSPSelection"));
		DSPSelection->SetFont(font);

		DSPAbout = new wxButton(this, ID_DSP_ABOUT, wxT("About..."), wxPoint(168, 120), wxSize(73, 25), 0, wxDefaultValidator, wxT("DSPAbout"));
		DSPAbout->SetFont(font);

		DSPConfig = new wxButton(this, ID_DSP_CONFIG, wxT("Config..."), wxPoint(88, 120), wxSize(73, 25), 0, wxDefaultValidator, wxT("DSPConfig"));
		DSPConfig->SetFont(font);

		WxStaticText2 = new wxStaticText(this, ID_WXSTATICTEXT2, wxT("DSP"), wxPoint(16, 93), wxDefaultSize, 0, wxT("WxStaticText2"));
		WxStaticText2->SetFont(font);

		FillChoiceBox(DSPSelection, PLUGIN_TYPE_DSP, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin);
	}

	{
		PADSelection = new wxChoice(this, ID_PAD_CB, wxPoint(88, 160), wxSize(333, 23), NULL, 0, wxDefaultValidator, wxT("PADSelection"));
		PADSelection->SetFont(font);

		PADAbout = new wxButton(this, ID_PAD_ABOUT, wxT("About..."), wxPoint(168, 192), wxSize(73, 25), 0, wxDefaultValidator, wxT("PADAbout"));
		PADAbout->SetFont(font);

		PADConfig = new wxButton(this, ID_PAD_CONFIG, wxT("Config..."), wxPoint(88, 192), wxSize(73, 25), 0, wxDefaultValidator, wxT("PADConfig"));
		PADConfig->SetFont(font);

		WxStaticText3 = new wxStaticText(this, ID_WXSTATICTEXT3, wxT("Pad"), wxPoint(16, 165), wxDefaultSize, 0, wxT("WxStaticText3"));
		WxStaticText3->SetFont(font);

		FillChoiceBox(PADSelection, PLUGIN_TYPE_PAD, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin);
	}
}


void
CPluginOptions::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}


void
CPluginOptions::OKClick(wxCommandEvent& event)
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


void
CPluginOptions::OnSelectionChanged(wxCommandEvent& WXUNUSED (event))
{
	Apply->Enable();
}


void
CPluginOptions::OnAbout(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case ID_PAD_ABOUT:
		    CallAbout(PADSelection);
		    break;

	    case ID_DSP_ABOUT:
		    CallAbout(DSPSelection);
		    break;

	    case ID_GRAPHIC_ABOUT:
		    CallAbout(GraphicSelection);
		    break;
	}
}


void
CPluginOptions::OnConfig(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	    case ID_PAD_CONFIG:
		    CallConfig(PADSelection);
		    break;

	    case ID_DSP_CONFIG:
		    CallConfig(DSPSelection);
		    break;

	    case ID_GRAPHIC_CONFIG:
		    CallConfig(GraphicSelection);
		    break;
	}
}


void
CPluginOptions::FillChoiceBox(wxChoice* _pChoice, int _PluginType, const std::string& _SelectFilename)
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


void
CPluginOptions::CallConfig(wxChoice* _pChoice)
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


void
CPluginOptions::CallAbout(wxChoice* _pChoice)
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


void
CPluginOptions::DoApply()
{
	Apply->Disable();

	GetFilename(GraphicSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin);
	GetFilename(DSPSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin);
	GetFilename(PADSelection, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin);

	SConfig::GetInstance().SaveSettings();
}


bool
CPluginOptions::GetFilename(wxChoice* _pChoice, std::string& _rFilename)
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


