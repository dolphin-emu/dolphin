// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dirdlg.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DiscIO/NANDContentLoader.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/Config/PathConfigPane.h"

PathConfigPane::PathConfigPane(wxWindow* panel, wxWindowID id)
	: wxPanel(panel, id)
{
	InitializeGUI();
	LoadGUIValues();
	RefreshGUI();
}

void PathConfigPane::InitializeGUI()
{
	m_iso_paths_listbox = new wxListBox(this, wxID_ANY);
	m_recursive_iso_paths_checkbox = new wxCheckBox(this, wxID_ANY, _("Search Subfolders"));
	m_add_iso_path_button = new wxButton(this, wxID_ANY, _("Add..."));
	m_remove_iso_path_button = new wxButton(this, wxID_ANY, _("Remove"));
	m_remove_iso_path_button->Disable();

	m_default_iso_filepicker = new wxFilePickerCtrl(this, wxID_ANY, wxEmptyString, _("Choose a default ISO:"),
		_("All GC/Wii files (elf, dol, gcm, iso, wbfs, ciso, gcz, wad)") + wxString::Format("|*.elf;*.dol;*.gcm;*.iso;*.wbfs;*.ciso;*.gcz;*.wad|%s", wxGetTranslation(wxALL_FILES)),
		wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN);
	m_dvd_root_dirpicker = new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, _("Choose a DVD root directory:"), wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);
	m_apploader_path_filepicker = new wxFilePickerCtrl(this, wxID_ANY, wxEmptyString, _("Choose file to use as apploader: (applies to discs constructed from directories only)"),
		_("apploader (.img)") + wxString::Format("|*.img|%s", wxGetTranslation(wxALL_FILES)),
		wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN);
	m_nand_root_dirpicker = new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, _("Choose a NAND root directory:"), wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL);

	m_iso_paths_listbox->Bind(wxEVT_LISTBOX, &PathConfigPane::OnISOPathSelectionChanged, this);
	m_recursive_iso_paths_checkbox->Bind(wxEVT_CHECKBOX, &PathConfigPane::OnRecursiveISOCheckBoxChanged, this);
	m_add_iso_path_button->Bind(wxEVT_BUTTON, &PathConfigPane::OnAddISOPath, this);
	m_remove_iso_path_button->Bind(wxEVT_BUTTON, &PathConfigPane::OnRemoveISOPath, this);
	m_default_iso_filepicker->Bind(wxEVT_FILEPICKER_CHANGED, &PathConfigPane::OnDefaultISOChanged, this);
	m_dvd_root_dirpicker->Bind(wxEVT_DIRPICKER_CHANGED, &PathConfigPane::OnDVDRootChanged, this);
	m_apploader_path_filepicker->Bind(wxEVT_FILEPICKER_CHANGED, &PathConfigPane::OnApploaderPathChanged, this);
	m_nand_root_dirpicker->Bind(wxEVT_DIRPICKER_CHANGED, &PathConfigPane::OnNANDRootChanged, this);

	wxBoxSizer* const iso_button_sizer = new wxBoxSizer(wxHORIZONTAL);
	iso_button_sizer->Add(m_recursive_iso_paths_checkbox, 0, wxALL | wxALIGN_CENTER);
	iso_button_sizer->AddStretchSpacer();
	iso_button_sizer->Add(m_add_iso_path_button, 0, wxALL);
	iso_button_sizer->Add(m_remove_iso_path_button, 0, wxALL);

	wxStaticBoxSizer* const iso_listbox_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("ISO Directories"));
	iso_listbox_sizer->Add(m_iso_paths_listbox, 1, wxEXPAND | wxALL, 0);
	iso_listbox_sizer->Add(iso_button_sizer, 0, wxEXPAND | wxALL, 5);

	wxGridBagSizer* const picker_sizer = new wxGridBagSizer();
	picker_sizer->Add(new wxStaticText(this, wxID_ANY, _("Default ISO:")),
		wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	picker_sizer->Add(m_default_iso_filepicker, wxGBPosition(0, 1), wxDefaultSpan, wxEXPAND | wxALL, 5);
	picker_sizer->Add(new wxStaticText(this, wxID_ANY, _("DVD Root:")),
		wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	picker_sizer->Add(m_dvd_root_dirpicker, wxGBPosition(1, 1), wxDefaultSpan, wxEXPAND | wxALL, 5);
	picker_sizer->Add(new wxStaticText(this, wxID_ANY, _("Apploader:")),
		wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	picker_sizer->Add(m_apploader_path_filepicker, wxGBPosition(2, 1), wxDefaultSpan, wxEXPAND | wxALL, 5);
	picker_sizer->Add(new wxStaticText(this, wxID_ANY, _("Wii NAND Root:")),
		wxGBPosition(3, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	picker_sizer->Add(m_nand_root_dirpicker, wxGBPosition(3, 1), wxDefaultSpan, wxEXPAND | wxALL, 5);
	picker_sizer->AddGrowableCol(1);

	// Populate the Paths page
	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(iso_listbox_sizer, 1, wxEXPAND | wxALL, 5);
	main_sizer->Add(picker_sizer, 0, wxEXPAND | wxALL, 5);

	SetSizer(main_sizer);
}

void PathConfigPane::LoadGUIValues()
{
	const SConfig& startup_params = SConfig::GetInstance();

	m_recursive_iso_paths_checkbox->SetValue(SConfig::GetInstance().m_RecursiveISOFolder);
	m_default_iso_filepicker->SetPath(StrToWxStr(startup_params.m_strDefaultISO));
	m_dvd_root_dirpicker->SetPath(StrToWxStr(startup_params.m_strDVDRoot));
	m_apploader_path_filepicker->SetPath(StrToWxStr(startup_params.m_strApploader));
	m_nand_root_dirpicker->SetPath(StrToWxStr(SConfig::GetInstance().m_NANDPath));

	// Update selected ISO paths
	for (const std::string& folder : SConfig::GetInstance().m_ISOFolder)
		m_iso_paths_listbox->Append(StrToWxStr(folder));
}

void PathConfigPane::RefreshGUI()
{
	if (Core::IsRunning())
		Disable();
}

void PathConfigPane::OnISOPathSelectionChanged(wxCommandEvent& event)
{
	m_remove_iso_path_button->Enable(m_iso_paths_listbox->GetSelection() != wxNOT_FOUND);
}

void PathConfigPane::OnRecursiveISOCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_RecursiveISOFolder = m_recursive_iso_paths_checkbox->IsChecked();

	AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_REFRESH_LIST));
}

void PathConfigPane::OnAddISOPath(wxCommandEvent& event)
{
	wxDirDialog dialog(this, _("Choose a directory to add"), wxGetHomeDir(),
		wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

	if (dialog.ShowModal() == wxID_OK)
	{
		if (m_iso_paths_listbox->FindString(dialog.GetPath()) != wxNOT_FOUND)
		{
			WxUtils::ShowErrorDialog(_("The chosen directory is already in the list."));
		}
		else
		{
			AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_REFRESH_LIST));
			m_iso_paths_listbox->Append(dialog.GetPath());
		}
	}

	SaveISOPathChanges();
}

void PathConfigPane::OnRemoveISOPath(wxCommandEvent& event)
{
	AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_REFRESH_LIST));
	m_iso_paths_listbox->Delete(m_iso_paths_listbox->GetSelection());

	// This seems to not be activated on Windows when it should be. wxw bug?
#ifdef _WIN32
	OnISOPathSelectionChanged(wxCommandEvent());
#endif

	SaveISOPathChanges();
}

void PathConfigPane::OnDefaultISOChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_strDefaultISO = WxStrToStr(m_default_iso_filepicker->GetPath());
}

void PathConfigPane::OnDVDRootChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_strDVDRoot = WxStrToStr(m_dvd_root_dirpicker->GetPath());
}

void PathConfigPane::OnApploaderPathChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_strApploader = WxStrToStr(m_apploader_path_filepicker->GetPath());
}

void PathConfigPane::OnNANDRootChanged(wxCommandEvent& event)
{
	std::string nand_path =
		SConfig::GetInstance().m_NANDPath =
		WxStrToStr(m_nand_root_dirpicker->GetPath());

	File::SetUserPath(D_WIIROOT_IDX, nand_path);
	m_nand_root_dirpicker->SetPath(StrToWxStr(nand_path));

	SConfig::GetInstance().m_SYSCONF->UpdateLocation();

	main_frame->UpdateWiiMenuChoice();
}

void PathConfigPane::SaveISOPathChanges()
{
	SConfig::GetInstance().m_ISOFolder.clear();

	for (unsigned int i = 0; i < m_iso_paths_listbox->GetCount(); i++)
		SConfig::GetInstance().m_ISOFolder.push_back(WxStrToStr(m_iso_paths_listbox->GetStrings()[i]));
}

