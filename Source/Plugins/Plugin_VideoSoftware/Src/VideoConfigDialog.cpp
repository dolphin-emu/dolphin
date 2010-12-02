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

#include "VideoConfigDialog.h"

#include "FileUtil.h"

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler( f ), (wxObject*)0, (wxEvtHandler*)s)

// template instantiation
template class BoolSetting<wxCheckBox>;

template <>
SettingCheckBox::BoolSetting(wxWindow* parent, const wxString& label, bool &setting, bool reverse, long style) :
	wxCheckBox(parent, -1, label, wxDefaultPosition, wxDefaultSize, style),
	m_setting(setting),
	m_reverse(reverse)
{
	SetValue(m_setting ^ m_reverse);
	_connect_macro_(this, SettingCheckBox::UpdateValue, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
}

template <typename T>
IntegerSetting<T>::IntegerSetting(wxWindow* parent, const wxString& label, T& setting, int minVal, int maxVal, long style) :
	wxSpinCtrl(parent, -1, label, wxDefaultPosition, wxDefaultSize, style),
	m_setting(setting)
{
	SetRange(minVal, maxVal);
	SetValue(m_setting);
	_connect_macro_(this, IntegerSetting::UpdateValue, wxEVT_COMMAND_SPINCTRL_UPDATED, this);
}


VideoConfigDialog::VideoConfigDialog(wxWindow* parent, const std::string& title, const std::string& _ininame) :
	wxDialog(parent, -1,
		wxString(wxT("Dolphin ")).append(wxString::FromAscii(title.c_str())).append(wxT(" Graphics Configuration")),
		wxDefaultPosition, wxDefaultSize),
	vconfig(g_Config),
	ininame(_ininame)
{
	vconfig.Load((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());

	wxNotebook* const notebook = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize);	

	// -- GENERAL --
	{
	wxPanel* const page_general= new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_general, wxT("General"));
	wxBoxSizer* const szr_general = new wxBoxSizer(wxVERTICAL);

	// - rendering
	{
	wxStaticBoxSizer* const group_rendering = new wxStaticBoxSizer(wxVERTICAL, page_general, wxT("Rendering"));
	szr_general->Add(group_rendering, 0, wxEXPAND | wxALL, 5);
	wxGridSizer* const szr_rendering = new wxGridSizer(2, 5, 5);
	group_rendering->Add(szr_rendering, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_rendering->Add(new SettingCheckBox(page_general, wxT("Hardware rasterization"), vconfig.bHwRasterizer));
	}

	// - info
	{
	wxStaticBoxSizer* const group_info = new wxStaticBoxSizer(wxVERTICAL, page_general, wxT("Overlay Information"));
	szr_general->Add(group_info, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxGridSizer* const szr_info = new wxGridSizer(2, 5, 5);
	group_info->Add(szr_info, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_info->Add(new SettingCheckBox(page_general, wxT("Various Statistics"), vconfig.bShowStats));
	}
	
	// - utility
	{
	wxStaticBoxSizer* const group_utility = new wxStaticBoxSizer(wxVERTICAL, page_general, wxT("Utility"));
	szr_general->Add(group_utility, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxGridSizer* const szr_utility = new wxGridSizer(2, 5, 5);
	group_utility->Add(szr_utility, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_utility->Add(new SettingCheckBox(page_general, wxT("Dump Textures"), vconfig.bDumpTextures));
	szr_utility->Add(new SettingCheckBox(page_general, wxT("Dump Objects"), vconfig.bDumpObjects));
	szr_utility->Add(new SettingCheckBox(page_general, wxT("Dump Frames"), vconfig.bDumpFrames));

	// - debug only
	wxStaticBoxSizer* const group_debug_only_utility = new wxStaticBoxSizer(wxHORIZONTAL, page_general, wxT("Debug Only"));
	group_utility->Add(group_debug_only_utility, 0, wxEXPAND | wxBOTTOM, 5);
	wxGridSizer* const szr_debug_only_utility = new wxGridSizer(2, 5, 5);
	group_debug_only_utility->Add(szr_debug_only_utility, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_debug_only_utility->Add(new SettingCheckBox(page_general, wxT("Dump TEV Stages"), vconfig.bDumpTevStages));
	szr_debug_only_utility->Add(new SettingCheckBox(page_general, wxT("Dump Texture Fetches"), vconfig.bDumpTevTextureFetches));
	}

	// - misc
	{
	wxStaticBoxSizer* const group_misc = new wxStaticBoxSizer(wxVERTICAL, page_general, wxT("Drawn Object Range"));
	szr_general->Add(group_misc, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxFlexGridSizer* const szr_misc = new wxFlexGridSizer(2, 5, 5);
	group_misc->Add(szr_misc, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_misc->Add(new U32Setting(page_general, wxT("Start"), vconfig.drawStart, 0, 100000));
	szr_misc->Add(new U32Setting(page_general, wxT("End"), vconfig.drawEnd, 0, 100000));
	}

	page_general->SetSizerAndFit(szr_general);
	}

	wxButton* const btn_close = new wxButton(this, -1, wxT("Close"), wxDefaultPosition);
	_connect_macro_(btn_close, VideoConfigDialog::Event_ClickClose, wxEVT_COMMAND_BUTTON_CLICKED, this);

	Connect(-1, wxEVT_CLOSE_WINDOW, wxCloseEventHandler(VideoConfigDialog::Event_Close), (wxObject*)0, this);

	wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
	szr_main->Add(notebook, 1, wxEXPAND | wxALL, 5);
	szr_main->Add(btn_close, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(szr_main);
	Center();
}

void VideoConfigDialog::Event_ClickClose(wxCommandEvent&)
{
	Close();
}

void VideoConfigDialog::Event_Close(wxCloseEvent& ev)
{
	g_Config.Save((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());

	ev.Skip();
}
