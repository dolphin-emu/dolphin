// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <ctime>
#include <wx/panel.h>

#include "Common/CommonTypes.h"

class DolphinSlider;
class wxCheckBox;
class wxDateEvent;
class wxDateTime;
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
  void BindEvents();

  void OnUpdateCPUClockControls(wxUpdateUIEvent&);
  void OnUpdateRTCDateTimeEntries(wxUpdateUIEvent&);

  void OnClockOverrideCheckBoxChanged(wxCommandEvent&);
  void OnClockOverrideSliderChanged(wxCommandEvent&);
  void OnCustomRTCCheckBoxChanged(wxCommandEvent&);
  void OnCustomRTCDateChanged(wxDateEvent&);
  void OnCustomRTCTimeChanged(wxDateEvent&);

  void UpdateCPUClock();

  // Custom RTC
  void LoadCustomRTC();
  void UpdateCustomRTC(const wxDateTime&);

  wxCheckBox* m_clock_override_checkbox;
  DolphinSlider* m_clock_override_slider;
  wxStaticText* m_clock_override_text;
  wxCheckBox* m_custom_rtc_checkbox;
  wxDatePickerCtrl* m_custom_rtc_date_picker;
  wxTimePickerCtrl* m_custom_rtc_time_picker;
};
