// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/arrstr.h>
#include <wx/panel.h>
#include "Common/CommonTypes.h"

class DolphinSlider;
class wxCheckBox;
class wxChoice;
class wxSlider;

class WiiConfigPane final : public wxPanel
{
public:
  WiiConfigPane(wxWindow* parent, wxWindowID id);

private:
  void InitializeGUI();
  void LoadGUIValues();
  void RefreshGUI();

  void OnScreenSaverCheckBoxChanged(wxCommandEvent&);
  void OnPAL60CheckBoxChanged(wxCommandEvent&);
  void OnSDCardCheckBoxChanged(wxCommandEvent&);
  void OnConnectKeyboardCheckBoxChanged(wxCommandEvent&);
  void OnSystemLanguageChoiceChanged(wxCommandEvent&);
  void OnAspectRatioChoiceChanged(wxCommandEvent&);

  void OnSensorBarPosChanged(wxCommandEvent&);
  void OnSensorBarSensChanged(wxCommandEvent&);
  void OnSpeakerVolumeChanged(wxCommandEvent&);
  void OnWiimoteMotorChanged(wxCommandEvent&);

  wxArrayString m_system_language_strings;
  wxArrayString m_aspect_ratio_strings;
  wxArrayString m_bt_sensor_bar_pos_strings;

  wxCheckBox* m_screensaver_checkbox;
  wxCheckBox* m_pal60_mode_checkbox;
  wxCheckBox* m_sd_card_checkbox;
  wxCheckBox* m_connect_keyboard_checkbox;
  wxChoice* m_system_language_choice;
  wxChoice* m_aspect_ratio_choice;

  wxChoice* m_bt_sensor_bar_pos;
  DolphinSlider* m_bt_sensor_bar_sens;
  DolphinSlider* m_bt_speaker_volume;
  wxCheckBox* m_bt_wiimote_motor;
};
