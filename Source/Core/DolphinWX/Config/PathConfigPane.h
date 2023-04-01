// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>

class wxButton;
class wxCheckBox;
class wxListBox;
class wxDirPickerCtrl;
class wxFilePickerCtrl;

class PathConfigPane final : public wxPanel
{
public:
	PathConfigPane(wxWindow* parent, wxWindowID id);

private:
	void InitializeGUI();
	void LoadGUIValues();
	void RefreshGUI();

	void OnISOPathSelectionChanged(wxCommandEvent&);
	void OnRecursiveISOCheckBoxChanged(wxCommandEvent&);
	void OnAddISOPath(wxCommandEvent&);
	void OnRemoveISOPath(wxCommandEvent&);
	void OnDefaultISOChanged(wxCommandEvent&);
	void OnDVDRootChanged(wxCommandEvent&);
	void OnApploaderPathChanged(wxCommandEvent&);
	void OnNANDRootChanged(wxCommandEvent&);

	void SaveISOPathChanges();

	wxListBox* m_iso_paths_listbox;
	wxCheckBox* m_recursive_iso_paths_checkbox;
	wxButton* m_add_iso_path_button;
	wxButton* m_remove_iso_path_button;

	wxDirPickerCtrl* m_dvd_root_dirpicker;
	wxDirPickerCtrl* m_nand_root_dirpicker;
	wxFilePickerCtrl* m_default_iso_filepicker;
	wxFilePickerCtrl* m_apploader_path_filepicker;
};
