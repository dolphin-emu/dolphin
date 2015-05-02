// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include "Core/ConfigManager.h"
#include "DolphinWX/Config/AdvancedConfigPane.h"

AdvancedConfigPane::AdvancedConfigPane(wxWindow* parent, wxWindowID id)
	: wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
}

void AdvancedConfigPane::InitializeGUI()
{
	m_clock_override_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable CPU Clock Override"));
	m_clock_override_slider = new wxSlider(this, wxID_ANY, 100, 0, 150, wxDefaultPosition, wxSize(200,-1));
	m_clock_override_text = new wxStaticText(this, wxID_ANY, "");

	m_clock_override_checkbox->Bind(wxEVT_CHECKBOX, &AdvancedConfigPane::OnClockOverrideCheckBoxChanged, this);
	m_clock_override_slider->Bind(wxEVT_SLIDER, &AdvancedConfigPane::OnClockOverrideSliderChanged, this);

	wxStaticText* const clock_override_description = new wxStaticText(this, wxID_ANY,
	  _("Higher values can make variable-framerate games "
	    "run at a higher framerate, at the expense of CPU. "
	    "Lower values can make variable-framerate games "
	    "run at a lower framerate, saving CPU.\n\n"
	    "WARNING: Changing this from the default (100%) "
	    "can and will break games and cause glitches. "
	    "Do so at your own risk. Please do not report "
	    "bugs that occur with a non-default clock. "));
	clock_override_description->Wrap(400);

	wxBoxSizer* const clock_override_checkbox_sizer = new wxBoxSizer(wxHORIZONTAL);
	clock_override_checkbox_sizer->Add(m_clock_override_checkbox);

	wxBoxSizer* const clock_override_slider_sizer = new wxBoxSizer(wxHORIZONTAL);
	clock_override_slider_sizer->Add(m_clock_override_slider, 1, wxALL, 5);
	clock_override_slider_sizer->Add(m_clock_override_text, 1, wxALL, 5);

	wxBoxSizer* const clock_override_description_sizer = new wxBoxSizer(wxHORIZONTAL);
	clock_override_description_sizer->Add(clock_override_description, 1, wxALL, 5);

	wxStaticBoxSizer* const cpu_options_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("CPU Options"));
	cpu_options_sizer->Add(clock_override_checkbox_sizer);
	cpu_options_sizer->Add(clock_override_slider_sizer);
	cpu_options_sizer->Add(clock_override_description_sizer);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(cpu_options_sizer , 0, wxEXPAND | wxALL, 5);

	SetSizer(main_sizer);
}

void AdvancedConfigPane::LoadGUIValues()
{
	int ocFactor = (int)(std::log2f(SConfig::GetInstance().m_OCFactor) * 25.f + 100.f + 0.5f);
	bool oc_enabled = SConfig::GetInstance().m_OCEnable;
	m_clock_override_checkbox->SetValue(oc_enabled);
	m_clock_override_slider ->SetValue(ocFactor);
	m_clock_override_slider->Enable(oc_enabled);
	UpdateCPUClock();
}

void AdvancedConfigPane::OnClockOverrideCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_OCEnable = m_clock_override_checkbox->IsChecked();
	m_clock_override_slider->Enable(SConfig::GetInstance().m_OCEnable);
	UpdateCPUClock();
}

void AdvancedConfigPane::OnClockOverrideSliderChanged(wxCommandEvent& event)
{
	// Vaguely exponential scaling?
	SConfig::GetInstance().m_OCFactor = std::exp2f((m_clock_override_slider->GetValue() - 100.f) / 25.f);
	UpdateCPUClock();
}

void AdvancedConfigPane::UpdateCPUClock()
{
	bool wii = SConfig::GetInstance().m_LocalCoreStartupParameter.bWii;
	int percent = (int)(std::roundf(SConfig::GetInstance().m_OCFactor * 100.f));
	int clock = (int)(std::roundf(SConfig::GetInstance().m_OCFactor * (wii ? 729.f : 486.f)));

	m_clock_override_text->SetLabel(SConfig::GetInstance().m_OCEnable ? wxString::Format("%d %% (%d mhz)", percent, clock) : "");
}
