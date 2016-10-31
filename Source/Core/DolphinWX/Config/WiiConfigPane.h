// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <utility>

#include <wx/arrstr.h>
#include <wx/clntdata.h>
#include <wx/panel.h>
#include "Common/CommonTypes.h"

class DolphinSlider;
class wxButton;
class wxCheckBox;
class wxChoice;
class wxListBox;
class wxSlider;
class wxTextCtrl;

class USBPassthroughDeviceEntry final : public wxClientData
{
public:
  USBPassthroughDeviceEntry(u16 vid, u16 pid) : m_vid(vid), m_pid(pid) {}
  const u16 m_vid;
  const u16 m_pid;
};

class WiiConfigPane final : public wxPanel
{
public:
  WiiConfigPane(wxWindow* parent, wxWindowID id);

private:
  void InitializeGUI();
  void LoadGUIValues();
  void BindEvents();

  void AddUSBPassthroughDeviceToList(const std::pair<u16, u16>& device);

  void OnScreenSaverCheckBoxChanged(wxCommandEvent&);
  void OnPAL60CheckBoxChanged(wxCommandEvent&);
  void OnSDCardCheckBoxChanged(wxCommandEvent&);
  void OnConnectKeyboardCheckBoxChanged(wxCommandEvent&);
  void OnSystemLanguageChoiceChanged(wxCommandEvent&);
  void OnAspectRatioChoiceChanged(wxCommandEvent&);

  void OnUSBPassthroughDevicesChanged(wxCommandEvent&);
  void OnUSBPassthroughDeviceAdd(wxCommandEvent&);
  void OnUSBPassthroughDeviceRemove(wxCommandEvent&);
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

  wxListBox* m_usb_passthrough_devices_listbox;
  wxButton* m_usb_passthrough_add_device_btn;
  wxButton* m_usb_passthrough_rem_device_btn;
  wxTextCtrl* m_usb_passthrough_new_device_vid;
  wxTextCtrl* m_usb_passthrough_new_device_pid;

  wxChoice* m_bt_sensor_bar_pos;
  DolphinSlider* m_bt_sensor_bar_sens;
  DolphinSlider* m_bt_speaker_volume;
  wxCheckBox* m_bt_wiimote_motor;
};
