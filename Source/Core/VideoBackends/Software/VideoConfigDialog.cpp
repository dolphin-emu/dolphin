// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "Common/FileUtil.h"
#include "Core/Core.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "VideoBackends/Software/VideoConfigDialog.h"

template <typename T>
IntegerSetting<T>::IntegerSetting(wxWindow* parent, const wxString& label, T& setting, int minVal, int maxVal, long style) :
	wxSpinCtrl(parent, -1, label, wxDefaultPosition, wxDefaultSize, style),
	m_setting(setting)
{
	SetRange(minVal, maxVal);
	SetValue(m_setting);
	Bind(wxEVT_COMMAND_SPINCTRL_UPDATED, &IntegerSetting::UpdateValue, this);
}


VideoConfigDialog::VideoConfigDialog(wxWindow* parent, const std::string& title, const std::string& _ininame) :
	wxDialog(parent, -1,
		wxString(wxT("Dolphin ")).append(StrToWxStr(title)).append(wxT(" Graphics Configuration")),
		wxDefaultPosition, wxDefaultSize),
	vconfig(g_SWVideoConfig),
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

	// backend
	wxStaticText* const label_backend = new wxStaticText(page_general, wxID_ANY, _("Backend:"));
	wxChoice* const choice_backend = new wxChoice(page_general, wxID_ANY, wxDefaultPosition);

	for (const VideoBackend* backend : g_available_video_backends)
	{
		choice_backend->AppendString(StrToWxStr(backend->GetDisplayName()));
	}

	// TODO: How to get the translated plugin name?
	choice_backend->SetStringSelection(StrToWxStr(g_video_backend->GetName()));
	choice_backend->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &VideoConfigDialog::Event_Backend, this);

	szr_rendering->Add(label_backend, 1, wxALIGN_CENTER_VERTICAL, 5);
	szr_rendering->Add(choice_backend, 1, 0, 0);

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		label_backend->Disable();
		choice_backend->Disable();
	}

	// rasterizer
	szr_rendering->Add(new SettingCheckBox(page_general, wxT("Hardware rasterization"), wxT(""), vconfig.bHwRasterizer));

	// xfb
	szr_rendering->Add(new SettingCheckBox(page_general, wxT("Bypass XFB"), wxT(""), vconfig.bBypassXFB));
	}

	// - info
	{
	wxStaticBoxSizer* const group_info = new wxStaticBoxSizer(wxVERTICAL, page_general, wxT("Overlay Information"));
	szr_general->Add(group_info, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxGridSizer* const szr_info = new wxGridSizer(2, 5, 5);
	group_info->Add(szr_info, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_info->Add(new SettingCheckBox(page_general, wxT("Various Statistics"), wxT(""), vconfig.bShowStats));
	}

	// - utility
	{
	wxStaticBoxSizer* const group_utility = new wxStaticBoxSizer(wxVERTICAL, page_general, wxT("Utility"));
	szr_general->Add(group_utility, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxGridSizer* const szr_utility = new wxGridSizer(2, 5, 5);
	group_utility->Add(szr_utility, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_utility->Add(new SettingCheckBox(page_general, wxT("Dump Textures"), wxT(""), vconfig.bDumpTextures));
	szr_utility->Add(new SettingCheckBox(page_general, wxT("Dump Objects"), wxT(""), vconfig.bDumpObjects));
	szr_utility->Add(new SettingCheckBox(page_general, wxT("Dump Frames"), wxT(""), vconfig.bDumpFrames));

	// - debug only
	wxStaticBoxSizer* const group_debug_only_utility = new wxStaticBoxSizer(wxHORIZONTAL, page_general, wxT("Debug Only"));
	group_utility->Add(group_debug_only_utility, 0, wxEXPAND | wxBOTTOM, 5);
	wxGridSizer* const szr_debug_only_utility = new wxGridSizer(2, 5, 5);
	group_debug_only_utility->Add(szr_debug_only_utility, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_debug_only_utility->Add(new SettingCheckBox(page_general, wxT("Dump TEV Stages"), wxT(""), vconfig.bDumpTevStages));
	szr_debug_only_utility->Add(new SettingCheckBox(page_general, wxT("Dump Texture Fetches"), wxT(""), vconfig.bDumpTevTextureFetches));
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

	wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
	szr_main->Add(notebook, 1, wxEXPAND | wxALL, 5);
	szr_main->Add(new wxButton(this, wxID_OK, wxT("Close"), wxDefaultPosition),
			0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(szr_main);
	Center();
	SetFocus();
}

VideoConfigDialog::~VideoConfigDialog()
{
	g_SWVideoConfig.Save((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());
}
