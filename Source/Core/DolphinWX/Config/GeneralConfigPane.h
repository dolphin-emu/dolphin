// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/arrstr.h>
#include <wx/panel.h>

class wxCheckBox;
class wxChoice;
class wxRadioBox;

class GeneralConfigPane final : public wxPanel
{
public:
	GeneralConfigPane(wxWindow* parent, wxWindowID id);

private:

	struct CPUCore
	{
		int CPUid;
		wxString name;
	};
	std::vector<CPUCore> cpu_cores;
	void InitializeGUI();
	void LoadGUIValues();
	void RefreshGUI();

	void OnDualCoreCheckBoxChanged(wxCommandEvent&);
	void OnIdleSkipCheckBoxChanged(wxCommandEvent&);
	void OnCheatCheckBoxChanged(wxCommandEvent&);
	void OnForceNTSCJCheckBoxChanged(wxCommandEvent&);
	void OnThrottlerChoiceChanged(wxCommandEvent&);
	void OnCPUEngineRadioBoxChanged(wxCommandEvent&);

	wxArrayString m_throttler_array_string;
	wxArrayString m_cpu_engine_array_string;

	wxCheckBox* m_dual_core_checkbox;
	wxCheckBox* m_idle_skip_checkbox;
	wxCheckBox* m_cheats_checkbox;
	wxCheckBox* m_force_ntscj_checkbox;

	wxChoice* m_throttler_choice;

	wxRadioBox* m_cpu_engine_radiobox;
};
