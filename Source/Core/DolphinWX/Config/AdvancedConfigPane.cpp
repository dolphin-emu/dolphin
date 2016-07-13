// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include <wx/checkbox.h>
#include <wx/dateevt.h>
#include <wx/gbsizer.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/time.h>

#include "Core/ConfigManager.h"
#include "DolphinWX/Config/AdvancedConfigPane.h"

AdvancedConfigPane::AdvancedConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  InitializeGUI();
  LoadGUIValues();
}

void AdvancedConfigPane::InitializeGUI()
{
  m_clock_override_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable CPU Clock Override"));
  m_clock_override_slider =
      new wxSlider(this, wxID_ANY, 100, 0, 150, wxDefaultPosition, wxSize(200, -1));
  m_clock_override_text = new wxStaticText(this, wxID_ANY, "");

  m_clock_override_checkbox->Bind(wxEVT_CHECKBOX,
                                  &AdvancedConfigPane::OnClockOverrideCheckBoxChanged, this);
  m_clock_override_slider->Bind(wxEVT_SLIDER, &AdvancedConfigPane::OnClockOverrideSliderChanged,
                                this);

  m_custom_rtc_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Custom RTC"));
  m_custom_rtc_date_picker = new wxDatePickerCtrl(this, wxID_ANY);
  m_custom_rtc_time_picker = new wxTimePickerCtrl(this, wxID_ANY);

  m_custom_rtc_checkbox->Bind(wxEVT_CHECKBOX, &AdvancedConfigPane::OnCustomRTCCheckBoxChanged,
                              this);
  m_custom_rtc_date_picker->Bind(wxEVT_DATE_CHANGED, &AdvancedConfigPane::OnCustomRTCDateChanged,
                                 this);
  m_custom_rtc_time_picker->Bind(wxEVT_TIME_CHANGED, &AdvancedConfigPane::OnCustomRTCTimeChanged,
                                 this);

  wxStaticText* const clock_override_description =
      new wxStaticText(this, wxID_ANY, _("Higher values can make variable-framerate games "
                                         "run at a higher framerate, at the expense of CPU. "
                                         "Lower values can make variable-framerate games "
                                         "run at a lower framerate, saving CPU.\n\n"
                                         "WARNING: Changing this from the default (100%) "
                                         "can and will break games and cause glitches. "
                                         "Do so at your own risk. Please do not report "
                                         "bugs that occur with a non-default clock. "));

// wxStaticText* const custom_rtc_description = new wxStaticText(this, wxID_ANY, _("TODO"));

#ifdef __APPLE__
  clock_override_description->Wrap(550);
#else
  clock_override_description->Wrap(400);
#endif

  wxBoxSizer* const clock_override_checkbox_sizer = new wxBoxSizer(wxHORIZONTAL);
  clock_override_checkbox_sizer->Add(m_clock_override_checkbox, 1, wxALL, 5);

  wxBoxSizer* const clock_override_slider_sizer = new wxBoxSizer(wxHORIZONTAL);
  clock_override_slider_sizer->Add(m_clock_override_slider, 1, wxALL, 5);
  clock_override_slider_sizer->Add(m_clock_override_text, 1, wxALL, 5);

  wxBoxSizer* const clock_override_description_sizer = new wxBoxSizer(wxHORIZONTAL);
  clock_override_description_sizer->Add(clock_override_description, 1, wxALL, 5);

  wxStaticBoxSizer* const cpu_options_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("CPU Options"));
  cpu_options_sizer->Add(clock_override_checkbox_sizer);
  cpu_options_sizer->Add(clock_override_slider_sizer);
  cpu_options_sizer->Add(clock_override_description_sizer);

  wxBoxSizer* const custom_rtc_checkbox_sizer = new wxBoxSizer(wxHORIZONTAL);
  custom_rtc_checkbox_sizer->Add(m_custom_rtc_checkbox, 1, wxALL, 5);

  wxGridBagSizer* const custom_rtc_date_time_sizer = new wxGridBagSizer();
  custom_rtc_date_time_sizer->Add(m_custom_rtc_date_picker, wxGBPosition(0, 0), wxDefaultSpan,
                                  wxEXPAND | wxALL, 5);
  custom_rtc_date_time_sizer->Add(m_custom_rtc_time_picker, wxGBPosition(0, 1), wxDefaultSpan,
                                  wxEXPAND | wxALL, 5);

  wxStaticBoxSizer* const custom_rtc_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Custom RTC Options"));
  custom_rtc_sizer->Add(custom_rtc_checkbox_sizer);
  custom_rtc_sizer->Add(custom_rtc_date_time_sizer);

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->Add(cpu_options_sizer, 0, wxEXPAND | wxALL, 5);
  main_sizer->Add(custom_rtc_sizer, 0, wxEXPAND | wxALL, 5);

  SetSizer(main_sizer);
}

void AdvancedConfigPane::LoadGUIValues()
{
  int ocFactor = (int)(std::log2f(SConfig::GetInstance().m_OCFactor) * 25.f + 100.f + 0.5f);
  bool oc_enabled = SConfig::GetInstance().m_OCEnable;
  m_clock_override_checkbox->SetValue(oc_enabled);
  m_clock_override_slider->SetValue(ocFactor);
  m_clock_override_slider->Enable(oc_enabled);
  UpdateCPUClock();
  wxDateTime custom_rtc_date = wxDateTime((time_t)SConfig::GetInstance().m_customRTCDate);
  wxDateTime custom_rtc_time = wxDateTime((time_t)SConfig::GetInstance().m_customRTCTime);
  bool custom_rtc_enabled = SConfig::GetInstance().bCustomRTC;
  m_custom_rtc_checkbox->SetValue(custom_rtc_enabled);
  if (custom_rtc_date > (time_t)0)
    m_custom_rtc_date_picker->SetValue(custom_rtc_date);
  if (custom_rtc_time > (time_t)0)
    m_custom_rtc_time_picker->SetValue(custom_rtc_time);
  m_custom_rtc_date_picker->Enable(custom_rtc_enabled);
  m_custom_rtc_time_picker->Enable(custom_rtc_enabled);
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
  SConfig::GetInstance().m_OCFactor =
      std::exp2f((m_clock_override_slider->GetValue() - 100.f) / 25.f);
  UpdateCPUClock();
}

void AdvancedConfigPane::OnCustomRTCCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bCustomRTC = m_custom_rtc_checkbox->IsChecked();
  m_custom_rtc_date_picker->Enable(SConfig::GetInstance().bCustomRTC);
  m_custom_rtc_time_picker->Enable(SConfig::GetInstance().bCustomRTC);
}

void AdvancedConfigPane::OnCustomRTCDateChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_customRTCDate = m_custom_rtc_date_picker->GetValue().GetTicks();
}

void AdvancedConfigPane::OnCustomRTCTimeChanged(wxCommandEvent& event)
{
  int hour = 0;
  int min = 0;
  int sec = 0;
  int* pHour = &hour;
  int* pMin = &min;
  int* pSec = &sec;
  m_custom_rtc_time_picker->GetTime(pHour, pMin, pSec);
  hour = *pHour;
  min = *pMin;
  sec = *pSec;
  u32 epoch = (u32)((hour * 3600) + (min * 60) + sec);
  SConfig::GetInstance().m_customRTCTime = epoch;
}

void AdvancedConfigPane::UpdateCPUClock()
{
  bool wii = SConfig::GetInstance().bWii;
  int percent = (int)(std::roundf(SConfig::GetInstance().m_OCFactor * 100.f));
  int clock = (int)(std::roundf(SConfig::GetInstance().m_OCFactor * (wii ? 729.f : 486.f)));

  m_clock_override_text->SetLabel(
      SConfig::GetInstance().m_OCEnable ? wxString::Format("%d %% (%d mhz)", percent, clock) : "");
}
