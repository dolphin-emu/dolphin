// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>

class DolphinSlider;
class wxCheckBox;
class wxDatePickerCtrl;
class wxStaticText;
class wxTimePickerCtrl;

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
  void OnGPUClockOverrideCheckBoxChanged(wxCommandEvent&);
  void OnGPUClockOverrideSliderChanged(wxCommandEvent&);
  void OnSyncGPUCheckBoxChanged(wxCommandEvent&);
  void OnCustomRTCCheckBoxChanged(wxCommandEvent&);
  void OnCustomRTCDateChanged(wxCommandEvent&);
  void OnCustomRTCTimeChanged(wxCommandEvent&);

  void LoadCPUOverclock();
  void LoadGPUOverclock();
  void UpdateCPUClock();
  void UpdateGPUClock();

  // Custom RTC
  void LoadCustomRTC();
  void UpdateCustomRTC(time_t date, time_t time);
  u32 m_temp_date;
  u32 m_temp_time;

  wxCheckBox* m_cpu_clock_override_checkbox;
  DolphinSlider* m_cpu_clock_override_slider;
  wxStaticText* m_cpu_clock_override_text;
  wxCheckBox* m_gpu_clock_override_checkbox;
  DolphinSlider* m_gpu_clock_override_slider;
  wxStaticText* m_gpu_clock_override_text;
  wxCheckBox* m_sync_gpu_checkbox;
  wxCheckBox* m_custom_rtc_checkbox;
  wxDatePickerCtrl* m_custom_rtc_date_picker;
  wxTimePickerCtrl* m_custom_rtc_time_picker;
};
