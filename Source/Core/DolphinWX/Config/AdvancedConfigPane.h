// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>

class wxCheckBox;
class wxDatePickerCtrl;
class wxSlider;
class wxStaticText;
class wxTimePickerCtrl;

class AdvancedConfigPane final : public wxPanel
{
public:
  AdvancedConfigPane(wxWindow* parent, wxWindowID id);

private:
  void InitializeGUI();
  void LoadGUIValues();

  void OnClockOverrideCheckBoxChanged(wxCommandEvent&);
  void OnClockOverrideSliderChanged(wxCommandEvent&);
  void OnCustomRTCCheckBoxChanged(wxCommandEvent&);
  void OnCustomRTCDateChanged(wxCommandEvent&);
  void OnCustomRTCTimeChanged(wxCommandEvent&);

  void UpdateCPUClock();

  // Custom RTC
  void LoadCustomRTC();
  void UpdateCustomRTC(time_t date, time_t time);
  u32 m_temp_date;
  u32 m_temp_time;

  wxCheckBox* m_clock_override_checkbox;
  wxSlider* m_clock_override_slider;
  wxStaticText* m_clock_override_text;
  wxCheckBox* m_custom_rtc_checkbox;
  wxDatePickerCtrl* m_custom_rtc_date_picker;
  wxTimePickerCtrl* m_custom_rtc_time_picker;
};
