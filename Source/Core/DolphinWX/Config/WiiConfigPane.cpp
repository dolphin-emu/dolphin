// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/gbsizer.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include "Common/Config/Config.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IOS/IOS.h"
#include "DolphinWX/Config/AddUSBDeviceDiag.h"
#include "DolphinWX/Config/WiiConfigPane.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"
#include "UICommon/USBUtils.h"

// SYSCONF uses 0 for bottom and 1 for top, but we place them in
// the other order in the GUI so that Top will be above Bottom,
// matching the respective physical placements of the sensor bar.
// This also matches the layout of the settings in the Wii Menu.
static int TranslateSensorBarPosition(int position)
{
  if (position == 0)
    return 1;
  if (position == 1)
    return 0;

  return position;
}

WiiConfigPane::WiiConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  InitializeGUI();
  LoadGUIValues();
  BindEvents();
  // This is only safe because WiiConfigPane is owned by CConfigMain, which exists
  // as long as the DolphinWX app exists.
  Config::AddConfigChangedCallback([&] { Core::QueueHostJob([&] { LoadGUIValues(); }, true); });
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

  m_bt_sensor_bar_pos_strings.Add(_("Top"));
  m_bt_sensor_bar_pos_strings.Add(_("Bottom"));

  m_pal60_mode_checkbox = new wxCheckBox(this, wxID_ANY, _("Use PAL60 Mode (EuRGB60)"));
  m_screensaver_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Screen Saver"));
  m_aspect_ratio_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_aspect_ratio_strings);
  m_system_language_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_system_language_strings);
  m_sd_card_checkbox = new wxCheckBox(this, wxID_ANY, _("Insert SD Card"));
  m_connect_keyboard_checkbox = new wxCheckBox(this, wxID_ANY, _("Connect USB Keyboard"));
  m_usb_passthrough_devices_listbox =
      new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 100));
  m_usb_passthrough_add_device_btn = new wxButton(this, wxID_ANY, _("Add..."));
  m_usb_passthrough_rem_device_btn = new wxButton(this, wxID_ANY, _("Remove"));
  m_bt_sensor_bar_pos =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_bt_sensor_bar_pos_strings);
  m_bt_sensor_bar_sens = new DolphinSlider(this, wxID_ANY, 0, 0, 4);
  m_bt_speaker_volume = new DolphinSlider(this, wxID_ANY, 0, 0, 127);
  m_bt_wiimote_motor_checkbox = new wxCheckBox(this, wxID_ANY, _("Wii Remote Rumble"));

  m_pal60_mode_checkbox->SetToolTip(_("Sets the Wii display mode to 60Hz (480i) instead of 50Hz "
                                      "(576i) for PAL games.\nMay not work for all games."));
  m_screensaver_checkbox->SetToolTip(_("Dims the screen after five minutes of inactivity."));
  m_system_language_choice->SetToolTip(_("Sets the Wii system language."));
  m_sd_card_checkbox->SetToolTip(_("Saved to /Wii/sd.raw (default size is 128mb)"));
  m_connect_keyboard_checkbox->SetToolTip(_("May cause slow down in Wii Menu and some games."));
  m_bt_wiimote_motor_checkbox->SetToolTip(_("Enables Wii Remote vibration."));

  const int space5 = FromDIP(5);

  wxGridBagSizer* const misc_settings_grid_sizer = new wxGridBagSizer(space5, space5);
  misc_settings_grid_sizer->Add(m_pal60_mode_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2));
  misc_settings_grid_sizer->Add(m_screensaver_checkbox, wxGBPosition(0, 2), wxGBSpan(1, 2));
  misc_settings_grid_sizer->Add(m_sd_card_checkbox, wxGBPosition(1, 0), wxGBSpan(1, 2));
  misc_settings_grid_sizer->Add(m_connect_keyboard_checkbox, wxGBPosition(1, 2), wxGBSpan(1, 2));
  misc_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Aspect Ratio:")),
                                wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  misc_settings_grid_sizer->Add(m_aspect_ratio_choice, wxGBPosition(2, 1), wxDefaultSpan,
                                wxALIGN_CENTER_VERTICAL);
  misc_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("System Language:")),
                                wxGBPosition(2, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  misc_settings_grid_sizer->Add(m_system_language_choice, wxGBPosition(2, 3), wxDefaultSpan,
                                wxALIGN_CENTER_VERTICAL);

  auto* const usb_passthrough_btn_sizer = new wxBoxSizer(wxHORIZONTAL);
  usb_passthrough_btn_sizer->AddStretchSpacer();
  usb_passthrough_btn_sizer->Add(m_usb_passthrough_add_device_btn, 0,
                                 wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, space5);
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
  // i18n: IR stands for infrared and refers to the pointer functionality of Wii Remotes
  bt_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("IR Sensitivity:")),
                              wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(bt_sensor_bar_pos_sizer, wxGBPosition(1, 1), wxDefaultSpan,
                              wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Speaker Volume:")),
                              wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(bt_speaker_volume_sizer, wxGBPosition(2, 1), wxDefaultSpan,
                              wxALIGN_CENTER_VERTICAL);
  bt_settings_grid_sizer->Add(m_bt_wiimote_motor_checkbox, wxGBPosition(3, 0), wxGBSpan(1, 2),
                              wxALIGN_CENTER_VERTICAL);

  wxStaticBoxSizer* const misc_settings_static_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Misc Settings"));
  misc_settings_static_sizer->AddSpacer(space5);
  misc_settings_static_sizer->Add(misc_settings_grid_sizer, 0, wxLEFT | wxRIGHT, space5);
  misc_settings_static_sizer->AddSpacer(space5);

  auto* const usb_passthrough_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Whitelisted USB Passthrough Devices"));
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
  main_sizer->Add(usb_passthrough_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(bt_settings_static_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);

  SetSizer(main_sizer);
}

void WiiConfigPane::LoadGUIValues()
{
  m_screensaver_checkbox->SetValue(Config::Get(Config::SYSCONF_SCREENSAVER));
  m_pal60_mode_checkbox->SetValue(Config::Get(Config::SYSCONF_PAL60));
  m_aspect_ratio_choice->SetSelection(Config::Get(Config::SYSCONF_WIDESCREEN));
  m_system_language_choice->SetSelection(Config::Get(Config::SYSCONF_LANGUAGE));

  m_sd_card_checkbox->SetValue(SConfig::GetInstance().m_WiiSDCard);
  m_connect_keyboard_checkbox->SetValue(SConfig::GetInstance().m_WiiKeyboard);

  PopulateUSBPassthroughListbox();

  m_bt_sensor_bar_pos->SetSelection(
      TranslateSensorBarPosition(Config::Get(Config::SYSCONF_SENSOR_BAR_POSITION)));
  m_bt_sensor_bar_sens->SetValue(Config::Get(Config::SYSCONF_SENSOR_BAR_SENSITIVITY));
  m_bt_speaker_volume->SetValue(Config::Get(Config::SYSCONF_SPEAKER_VOLUME));
  m_bt_wiimote_motor_checkbox->SetValue(Config::Get(Config::SYSCONF_WIIMOTE_MOTOR));
}

void WiiConfigPane::PopulateUSBPassthroughListbox()
{
  m_usb_passthrough_devices_listbox->Freeze();
  m_usb_passthrough_devices_listbox->Clear();
  for (const auto& device : SConfig::GetInstance().m_usb_passthrough_devices)
  {
    m_usb_passthrough_devices_listbox->Append(USBUtils::GetDeviceName(device),
                                              new USBPassthroughDeviceEntry(device));
  }
  m_usb_passthrough_devices_listbox->Thaw();
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

  m_bt_wiimote_motor_checkbox->Bind(wxEVT_CHECKBOX, &WiiConfigPane::OnWiimoteMotorChanged, this);
  m_bt_wiimote_motor_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

  m_usb_passthrough_add_device_btn->Bind(wxEVT_BUTTON, &WiiConfigPane::OnUSBWhitelistAddButton,
                                         this);

  m_usb_passthrough_rem_device_btn->Bind(wxEVT_BUTTON, &WiiConfigPane::OnUSBWhitelistRemoveButton,
                                         this);
  m_usb_passthrough_rem_device_btn->Bind(wxEVT_UPDATE_UI,
                                         &WiiConfigPane::OnUSBWhitelistRemoveButtonUpdate, this);
}

void WiiConfigPane::OnUSBWhitelistAddButton(wxCommandEvent&)
{
  AddUSBDeviceDiag add_dialog{this};
  // Reload the USB device whitelist
  if (add_dialog.ShowModal() == wxID_OK)
    PopulateUSBPassthroughListbox();
}

void WiiConfigPane::OnUSBWhitelistRemoveButton(wxCommandEvent&)
{
  const int index = m_usb_passthrough_devices_listbox->GetSelection();
  if (index == wxNOT_FOUND)
    return;
  auto* const entry = static_cast<const USBPassthroughDeviceEntry*>(
      m_usb_passthrough_devices_listbox->GetClientObject(index));
  SConfig::GetInstance().m_usb_passthrough_devices.erase({entry->m_vid, entry->m_pid});
  m_usb_passthrough_devices_listbox->Delete(index);
}

void WiiConfigPane::OnUSBWhitelistRemoveButtonUpdate(wxUpdateUIEvent& event)
{
  event.Enable(m_usb_passthrough_devices_listbox->GetSelection() != wxNOT_FOUND);
}

void WiiConfigPane::OnScreenSaverCheckBoxChanged(wxCommandEvent& event)
{
  Config::SetBase(Config::SYSCONF_SCREENSAVER, m_screensaver_checkbox->IsChecked());
}

void WiiConfigPane::OnPAL60CheckBoxChanged(wxCommandEvent& event)
{
  Config::SetBase(Config::SYSCONF_PAL60, m_pal60_mode_checkbox->IsChecked());
}

void WiiConfigPane::OnSDCardCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_WiiSDCard = m_sd_card_checkbox->IsChecked();
  const auto ios = IOS::HLE::GetIOS();
  if (ios)
    ios->SDIO_EventNotify();
}

void WiiConfigPane::OnConnectKeyboardCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_WiiKeyboard = m_connect_keyboard_checkbox->IsChecked();
}

void WiiConfigPane::OnSystemLanguageChoiceChanged(wxCommandEvent& event)
{
  Config::SetBase(Config::SYSCONF_LANGUAGE,
                  static_cast<u32>(m_system_language_choice->GetSelection()));
}

void WiiConfigPane::OnAspectRatioChoiceChanged(wxCommandEvent& event)
{
  Config::SetBase(Config::SYSCONF_WIDESCREEN, m_aspect_ratio_choice->GetSelection() == 1);
}

void WiiConfigPane::OnSensorBarPosChanged(wxCommandEvent& event)
{
  Config::SetBase(Config::SYSCONF_SENSOR_BAR_POSITION,
                  static_cast<u32>(TranslateSensorBarPosition(event.GetInt())));
}

void WiiConfigPane::OnSensorBarSensChanged(wxCommandEvent& event)
{
  Config::SetBase(Config::SYSCONF_SENSOR_BAR_SENSITIVITY, static_cast<u32>(event.GetInt()));
}

void WiiConfigPane::OnSpeakerVolumeChanged(wxCommandEvent& event)
{
  Config::SetBase(Config::SYSCONF_SPEAKER_VOLUME, static_cast<u32>(event.GetInt()));
}

void WiiConfigPane::OnWiimoteMotorChanged(wxCommandEvent& event)
{
  Config::SetBase(Config::SYSCONF_WIIMOTE_MOTOR, event.IsChecked());
}
