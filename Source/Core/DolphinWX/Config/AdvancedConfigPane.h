// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <ctime>
#include <wx/panel.h>

#include "Common/CommonTypes.h"

class DolphinSlider;
class wxCheckBox;
class wxDatePickerCtrl;
class wxRadioBox;
class wxStaticText;
class wxTimePickerCtrl;

class AdvancedConfigPane final : public wxPanel
{
public:
  AdvancedConfigPane(wxWindow* parent, wxWindowID id);

private:
  void InitializeGUI();
  void LoadGUIValues();
  void BindEvents();

  void OnUpdateCPUClockControls(wxUpdateUIEvent&);
  void OnUpdateRTCDateTimeEntries(wxUpdateUIEvent&);

  void OnCPUEngineRadioBoxChanged(wxCommandEvent&);
  void OnForceNTSCJCheckBoxChanged(wxCommandEvent&);
  void OnClockOverrideCheckBoxChanged(wxCommandEvent&);
  void OnClockOverrideSliderChanged(wxCommandEvent&);
  void OnCustomRTCCheckBoxChanged(wxCommandEvent&);
  void OnCustomRTCDateChanged(wxCommandEvent&);
  void OnCustomRTCTimeChanged(wxCommandEvent&);

  void UpdateCPUClock();

  struct CPUCore
  {
    int CPUid;
    wxString name;
  };
  std::vector<CPUCore> m_cpu_cores;

  // Custom RTC
  void LoadCustomRTC();
  void UpdateCustomRTC(time_t date, time_t time);
  u32 m_temp_date;
  u32 m_temp_time;

  wxArrayString m_cpu_engine_array_string;
  wxRadioBox* m_cpu_engine_radiobox;
  wxCheckBox* m_force_ntscj_checkbox;
  wxCheckBox* m_clock_override_checkbox;
  DolphinSlider* m_clock_override_slider;
  wxStaticText* m_clock_override_text;
  wxCheckBox* m_custom_rtc_checkbox;
  wxDatePickerCtrl* m_custom_rtc_date_picker;
  wxTimePickerCtrl* m_custom_rtc_time_picker;
};
