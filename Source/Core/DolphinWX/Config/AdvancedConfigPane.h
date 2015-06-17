// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>

class wxCheckBox;
class wxSlider;
class wxStaticText;

class AdvancedConfigPane final : public wxPanel
{
public:
	AdvancedConfigPane(wxWindow* parent, wxWindowID id);

private:
	void InitializeGUI();
	void LoadGUIValues();
	void RefreshGUI();

	void OnClockOverrideCheckBoxChanged(wxCommandEvent&);
	void OnClockOverrideSliderChanged(wxCommandEvent&);

	void UpdateCPUClock();

	void OnComponentCableCheckBoxChanged(wxCommandEvent&);
	void OnProgressiveScanCheckBoxChanged(wxCommandEvent&);

	wxCheckBox* m_clock_override_checkbox;
	wxSlider* m_clock_override_slider;
	wxStaticText* m_clock_override_text;

	wxCheckBox* m_component_cable_checkbox;
	wxCheckBox* m_progressive_scan_checkbox;
};
