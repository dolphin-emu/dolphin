// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/WiiConfigPane.h"

#include <algorithm>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/gbsizer.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"

WiiConfigPane::WiiConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  InitializeGUI();
  LoadGUIValues();
  BindEvents();
}

void WiiConfigPane::InitializeGUI()
{
  m_aspect_ratio_strings.Add("4:3");
  m_aspect_ratio_strings.Add("16:9");

  m_system_language_strings.Add(_("Japanese"));
  m_system_language_strings.Add(_("English"));
  m_system_language_strings.Add(_("German"));
  m_system_language_strings.Add(_("French"));
  m_system_language_strings.Add(_("Spanish"));
  m_system_language_strings.Add(_("Italian"));
  m_system_language_strings.Add(_("Dutch"));
  m_system_language_strings.Add(_("Simplified Chinese"));
  m_system_language_strings.Add(_("Traditional Chinese"));
  m_system_language_strings.Add(_("Korean"));

  m_bt_sensor_bar_pos_strings.Add(_("Bottom"));
  m_bt_sensor_bar_pos_strings.Add(_("Top"));

  m_screensaver_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Screen Saver"));
  m_pal60_mode_checkbox = new wxCheckBox(this, wxID_ANY, _("Use PAL60 Mode (EuRGB60)"));
  m_aspect_ratio_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_aspect_ratio_strings);
  m_system_language_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_system_language_strings);
  m_sd_card_checkbox = new wxCheckBox(this, wxID_ANY, _("Insert SD Card"));
  m_connect_keyboard_checkbox = new wxCheckBox(this, wxID_ANY, _("Connect USB Keyboard"));
  m_usb_passthrough_devices_listbox = new wxListBox(this, wxID_ANY);
  m_usb_passthrough_add_device_btn = new wxButton(this, wxID_ANY, _("Add"));
  m_usb_passthrough_rem_device_btn = new wxButton(this, wxID_ANY, _("Remove"));
  m_usb_passthrough_new_device_vid = new wxTextCtrl(this, wxID_ANY);
  m_usb_passthrough_new_device_pid = new wxTextCtrl(this, wxID_ANY);
  m_bt_sensor_bar_pos =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_bt_sensor_bar_pos_strings);
  m_bt_sensor_bar_sens = new DolphinSlider(this, wxID_ANY, 0, 0, 4);
  m_bt_speaker_volume = new DolphinSlider(this, wxID_ANY, 0, 0, 127);
  m_bt_wiimote_motor = new wxCheckBox(this, wxID_ANY, _("Wii Remote Motor"));

  m_screensaver_checkbox->SetToolTip(_("Dims the screen after five minutes of inactivity."));
  m_pal60_mode_checkbox->SetToolTip(_("Sets the Wii display mode to 60Hz (480i) instead of 50Hz "
                                      "(576i) for PAL games.\nMay not work for all games."));
  m_system_language_choice->SetToolTip(_("Sets the Wii system language."));
  m_sd_card_checkbox->SetToolTip(_("Saved to /Wii/sd.raw (default size is 128mb)"));
  m_connect_keyboard_checkbox->SetToolTip(_("May cause slow down in Wii Menu and some games."));

  const int space5 = FromDIP(5);

  wxGridBagSizer* const misc_settings_grid_sizer = new wxGridBagSizer(space5, space5);
  misc_settings_grid_sizer->Add(m_screensaver_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2));
  misc_settings_grid_sizer->Add(m_pal60_mode_checkbox, wxGBPosition(1, 0), wxGBSpan(1, 2));
  misc_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Aspect Ratio:")),
                                wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  misc_settings_grid_sizer->Add(m_aspect_ratio_choice, wxGBPosition(2, 1), wxDefaultSpan,
                                wxALIGN_CENTER_VERTICAL);
  misc_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("System Language:")),
                                wxGBPosition(3, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  misc_settings_grid_sizer->Add(m_system_language_choice, wxGBPosition(3, 1), wxDefaultSpan,
                                wxALIGN_CENTER_VERTICAL);

  auto* const usb_passthrough_btn_sizer = new wxBoxSizer(wxHORIZONTAL);
  usb_passthrough_btn_sizer->Add(m_usb_passthrough_new_device_vid, 0, wxALIGN_CENTER_VERTICAL);
  usb_passthrough_btn_sizer->AddSpacer(FromDIP(2));
  usb_passthrough_btn_sizer->Add(m_usb_passthrough_new_device_pid, 0, wxALIGN_CENTER_VERTICAL);
  usb_passthrough_btn_sizer->AddSpacer(FromDIP(2));
  usb_passthrough_btn_sizer->Add(m_usb_passthrough_add_device_btn, 0, wxALIGN_CENTER_VERTICAL);
  usb_passthrough_btn_sizer->AddStretchSpacer();
  usb_passthrough_btn_sizer->Add(m_usb_passthrough_rem_device_btn, 0, wxALIGN_CENTER_VERTICAL);

  auto* const bt_sensor_bar_pos_sizer = new wxBoxSizer(wxHORIZONTAL);
  bt_sensor_bar_pos_sizer->Add(new wxStaticText(this, wxID_ANY, _("Min")), 0,
                               wxALIGN_CENTER_VERTICAL);
  bt_sensor_bar_pos_sizer->Add(m_bt_sensor_bar_sens, 0, wxALIGN_CENTER_VERTICAL);
  bt_sensor_bar_pos_sizer->Add(new wxStaticText(this, wxID_ANY, _("Max")), 0,
                               wxALIGN_CENTER_VERTICAL);

  auto* const bt_speaker_volume_sizer = new wxBoxSizer(wxHORIZONTAL);
  bt_speaker_volume_sizer->Add(new wxStaticText(this, wxID_ANY, _("Min")), 0,
                               wxALIGN_CENTER_VERTICAL);
  bt_speaker_volume_sizer->Add(m_bt_speaker_volume, 0, wxALIGN_CENTER_VERTICAL);
  bt_speaker_volume_sizer->Add(new wxStaticText(this, wxID_ANY, _("Max")), 0,
                               wxALIGN_CENTER_VERTICAL);

  wxGridBagSizer* const bt_settings_grid_sizer = new wxGridBagSizer(space5, space5);
  bt_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Sensor Bar Position:")),
                              wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(m_bt_sensor_bar_pos, wxGBPosition(0, 1), wxDefaultSpan,
                              wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("IR Sensitivity:")),
                              wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(bt_sensor_bar_pos_sizer, wxGBPosition(1, 1), wxDefaultSpan,
                              wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Speaker Volume:")),
                              wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(bt_speaker_volume_sizer, wxGBPosition(2, 1), wxDefaultSpan,
                              wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(m_bt_wiimote_motor, wxGBPosition(3, 0), wxGBSpan(1, 2),
                              wxALIGN_CENTER_VERTICAL);

  wxStaticBoxSizer* const misc_settings_static_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Misc Settings"));
  misc_settings_static_sizer->AddSpacer(space5);
  misc_settings_static_sizer->Add(misc_settings_grid_sizer, 0, wxLEFT | wxRIGHT, space5);
  misc_settings_static_sizer->AddSpacer(space5);

  wxStaticBoxSizer* const device_settings_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Device Settings"));
  device_settings_sizer->AddSpacer(space5);
  device_settings_sizer->Add(m_sd_card_checkbox, 0, wxLEFT | wxRIGHT, space5);
  device_settings_sizer->AddSpacer(space5);
  device_settings_sizer->Add(m_connect_keyboard_checkbox, 0, wxLEFT | wxRIGHT, space5);
  device_settings_sizer->AddSpacer(space5);

  auto* const usb_passthrough_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("USB Passthrough Devices"));
  usb_passthrough_sizer->AddSpacer(space5);
  usb_passthrough_sizer->Add(m_usb_passthrough_devices_listbox, 0, wxEXPAND | wxLEFT | wxRIGHT,
                             space5);
  usb_passthrough_sizer->AddSpacer(space5);
  usb_passthrough_sizer->Add(usb_passthrough_btn_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  usb_passthrough_sizer->AddSpacer(space5);

  auto* const bt_settings_static_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Wii Remote Settings"));
  bt_settings_static_sizer->AddSpacer(space5);
  bt_settings_static_sizer->Add(bt_settings_grid_sizer, 0, wxLEFT | wxRIGHT, space5);
  bt_settings_static_sizer->AddSpacer(space5);

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(misc_settings_static_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(device_settings_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(usb_passthrough_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(bt_settings_static_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);

  SetSizer(main_sizer);
}

void WiiConfigPane::AddUSBPassthroughDeviceToList(const std::pair<u16, u16>& device)
{
  std::string label = StringFromFormat("%04x:%04x", device.first, device.second);
  // Have a more detailed label for devices which are passed through by default.
  if (device.first == 0x057e && device.second == 0x0308)
    // i18n: Wii Speak is the name of a Nintendo product.
    label += " (Wii Speak)";
  if (device.first == 0x046d && device.second == 0x0a03)
    label += " (Logitech microphone)";
  m_usb_passthrough_devices_listbox->Append(
      label, new USBPassthroughDeviceEntry(device.first, device.second));
}

void WiiConfigPane::LoadGUIValues()
{
  m_screensaver_checkbox->SetValue(!!SConfig::GetInstance().m_wii_screensaver);
  m_pal60_mode_checkbox->SetValue(SConfig::GetInstance().bPAL60);
  m_aspect_ratio_choice->SetSelection(SConfig::GetInstance().m_wii_aspect_ratio);
  m_system_language_choice->SetSelection(SConfig::GetInstance().m_wii_language);

  m_sd_card_checkbox->SetValue(SConfig::GetInstance().m_WiiSDCard);
  m_connect_keyboard_checkbox->SetValue(SConfig::GetInstance().m_WiiKeyboard);

  for (const auto& device : SConfig::GetInstance().m_usb_passthrough_devices)
    AddUSBPassthroughDeviceToList(device);

  m_usb_passthrough_rem_device_btn->Disable();
  // i18n: VID means Vendor ID (in the context of a USB device)
  m_usb_passthrough_new_device_vid->SetHint("VID");
  // i18n: PID means Product ID (in the context of a USB device), not Process ID
  m_usb_passthrough_new_device_pid->SetHint("PID");

  m_bt_sensor_bar_pos->SetSelection(SConfig::GetInstance().m_sensor_bar_position);
  m_bt_sensor_bar_sens->SetValue(SConfig::GetInstance().m_sensor_bar_sensitivity);
  m_bt_speaker_volume->SetValue(SConfig::GetInstance().m_speaker_volume);
  m_bt_wiimote_motor->SetValue(SConfig::GetInstance().m_wiimote_motor);
}

void WiiConfigPane::BindEvents()
{
  m_screensaver_checkbox->Bind(wxEVT_CHECKBOX, &WiiConfigPane::OnScreenSaverCheckBoxChanged, this);
  m_screensaver_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_pal60_mode_checkbox->Bind(wxEVT_CHECKBOX, &WiiConfigPane::OnPAL60CheckBoxChanged, this);
  m_pal60_mode_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_aspect_ratio_choice->Bind(wxEVT_CHOICE, &WiiConfigPane::OnAspectRatioChoiceChanged, this);
  m_aspect_ratio_choice->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_system_language_choice->Bind(wxEVT_CHOICE, &WiiConfigPane::OnSystemLanguageChoiceChanged, this);
  m_system_language_choice->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_sd_card_checkbox->Bind(wxEVT_CHECKBOX, &WiiConfigPane::OnSDCardCheckBoxChanged, this);
  m_connect_keyboard_checkbox->Bind(wxEVT_CHECKBOX,
                                    &WiiConfigPane::OnConnectKeyboardCheckBoxChanged, this);

  m_bt_sensor_bar_pos->Bind(wxEVT_CHOICE, &WiiConfigPane::OnSensorBarPosChanged, this);
  m_bt_sensor_bar_pos->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_bt_sensor_bar_sens->Bind(wxEVT_SLIDER, &WiiConfigPane::OnSensorBarSensChanged, this);
  m_bt_sensor_bar_sens->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_bt_speaker_volume->Bind(wxEVT_SLIDER, &WiiConfigPane::OnSpeakerVolumeChanged, this);
  m_bt_speaker_volume->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_bt_wiimote_motor->Bind(wxEVT_CHECKBOX, &WiiConfigPane::OnWiimoteMotorChanged, this);
  m_bt_wiimote_motor->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_usb_passthrough_devices_listbox->Bind(wxEVT_LISTBOX,
                                          &WiiConfigPane::OnUSBPassthroughDevicesChanged, this);
  m_usb_passthrough_devices_listbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_usb_passthrough_add_device_btn->Bind(wxEVT_BUTTON, &WiiConfigPane::OnUSBPassthroughDeviceAdd,
                                         this);
  m_usb_passthrough_add_device_btn->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_usb_passthrough_rem_device_btn->Bind(wxEVT_BUTTON, &WiiConfigPane::OnUSBPassthroughDeviceRemove,
                                         this);

  m_usb_passthrough_new_device_vid->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);
  m_usb_passthrough_new_device_pid->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);
}

void WiiConfigPane::OnScreenSaverCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_wii_screensaver = m_screensaver_checkbox->IsChecked();
}

void WiiConfigPane::OnPAL60CheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bPAL60 = m_pal60_mode_checkbox->IsChecked();
}

void WiiConfigPane::OnSDCardCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_WiiSDCard = m_sd_card_checkbox->IsChecked();
  WII_IPC_HLE_Interface::SDIO_EventNotify();
}

void WiiConfigPane::OnConnectKeyboardCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_WiiKeyboard = m_connect_keyboard_checkbox->IsChecked();
}

void WiiConfigPane::OnSystemLanguageChoiceChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_wii_language = m_system_language_choice->GetSelection();
}

void WiiConfigPane::OnAspectRatioChoiceChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_wii_aspect_ratio = m_aspect_ratio_choice->GetSelection();
}

void WiiConfigPane::OnUSBPassthroughDevicesChanged(wxCommandEvent&)
{
  m_usb_passthrough_rem_device_btn->Enable(m_usb_passthrough_devices_listbox->GetSelection() !=
                                           wxNOT_FOUND);
}

static bool IsValidHexString(const std::string& string)
{
  if (string.length() == 0 || string.length() > 4)
    return false;
  return std::all_of(string.begin(), string.end(),
                     [](const auto& character) { return std::isxdigit(character) != 0; });
}

void WiiConfigPane::OnUSBPassthroughDeviceAdd(wxCommandEvent&)
{
  std::string vid_string = StripSpaces(WxStrToStr(m_usb_passthrough_new_device_vid->GetValue()));
  std::string pid_string = StripSpaces(WxStrToStr(m_usb_passthrough_new_device_pid->GetValue()));
  if (!IsValidHexString(vid_string) || !IsValidHexString(pid_string))
  {
    wxMessageBox(_("The entered VID or PID is invalid."), _("USB Passthrough"), wxICON_ERROR);
    return;
  }

  const u16 vid = static_cast<u16>(strtol(vid_string.c_str(), nullptr, 16));
  const u16 pid = static_cast<u16>(strtol(pid_string.c_str(), nullptr, 16));

  if (SConfig::GetInstance().m_usb_passthrough_devices.count({vid, pid}))
  {
    wxMessageBox(_("This device is already being passed through."), _("USB Passthrough"));
    return;
  }

  SConfig::GetInstance().m_usb_passthrough_devices.emplace(vid, pid);
  AddUSBPassthroughDeviceToList({vid, pid});
  m_usb_passthrough_new_device_vid->Clear();
  m_usb_passthrough_new_device_pid->Clear();
}

void WiiConfigPane::OnUSBPassthroughDeviceRemove(wxCommandEvent&)
{
  const int index = m_usb_passthrough_devices_listbox->GetSelection();
  if (index == wxNOT_FOUND)
    return;
  const auto* entry = static_cast<USBPassthroughDeviceEntry*>(
      m_usb_passthrough_devices_listbox->GetClientObject(index));
  SConfig::GetInstance().m_usb_passthrough_devices.erase({entry->m_vid, entry->m_pid});
  m_usb_passthrough_devices_listbox->Delete(index);

  // wx does not seem to disable the device remove button, unless this is done.
  wxCommandEvent dummy_event;
  OnUSBPassthroughDevicesChanged(dummy_event);
}

void WiiConfigPane::OnSensorBarPosChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_sensor_bar_position = event.GetInt();
}

void WiiConfigPane::OnSensorBarSensChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_sensor_bar_sensitivity = event.GetInt();
}

void WiiConfigPane::OnSpeakerVolumeChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_speaker_volume = event.GetInt();
}

void WiiConfigPane::OnWiimoteMotorChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_wiimote_motor = event.IsChecked();
}
