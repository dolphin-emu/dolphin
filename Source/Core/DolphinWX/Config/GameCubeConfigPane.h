// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/arrstr.h>
#include <wx/panel.h>

class wxButton;
class wxCheckBox;
class wxChoice;
class wxString;

class GameCubeConfigPane final : public wxPanel
{
public:
	GameCubeConfigPane(wxWindow* parent, wxWindowID id);

private:
	void InitializeGUI();
	void LoadGUIValues();
	void RefreshGUI();

	void OnSystemLanguageChange(wxCommandEvent&);
	void OnOverrideLanguageCheckBoxChanged(wxCommandEvent&);
	void OnSkipBiosCheckBoxChanged(wxCommandEvent&);
	void OnSlotAChanged(wxCommandEvent&);
	void OnSlotBChanged(wxCommandEvent&);
	void OnSP1Changed(wxCommandEvent&);
	void OnSlotAButtonClick(wxCommandEvent&);
	void OnSlotBButtonClick(wxCommandEvent&);

	void ChooseEXIDevice(const wxString& device_name, int device_id);
	void ChooseSlotPath(bool is_slot_a, TEXIDevices device_type);

	wxArrayString m_ipl_language_strings;

	wxChoice* m_system_lang_choice;
	wxCheckBox* m_override_lang_checkbox;
	wxCheckBox* m_skip_bios_checkbox;
	wxChoice* m_exi_devices[3];
	wxButton* m_memcard_path[2];
};
