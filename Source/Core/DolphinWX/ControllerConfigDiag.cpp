// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <utility>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/gbsizer.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/HotkeyManager.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"
#include "Core/NetPlayProto.h"
#include "DolphinWX/Config/GCAdapterConfigDiag.h"
#include "DolphinWX/ControllerConfigDiag.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/Input/GCKeyboardInputConfigDiag.h"
#include "DolphinWX/Input/GCPadInputConfigDiag.h"
#include "DolphinWX/Input/InputConfigDiag.h"
#include "DolphinWX/Input/WiimoteInputConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "InputCommon/GCAdapter.h"
#include "UICommon/UICommon.h"

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include "UICommon/X11Utils.h"
#endif

ControllerConfigDiag::ControllerConfigDiag(wxWindow* const parent)
    : wxDialog(parent, wxID_ANY, _("Dolphin Controller Configuration"))
{
  m_gc_pad_type_strs = {{_("None"), _("Standard Controller"), _("GameCube Adapter for Wii U"),
                         _("Steering Wheel"), _("Dance Mat"), _("DK Bongos"), _("GBA"),
                         _("Keyboard")}};

  const int space5 = FromDIP(5);

  // Combine all UI controls into their own encompassing sizer.
  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(CreateGamecubeSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(CreateWiimoteConfigSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(CreateAdvancedSettingsSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_sizer->AddSpacer(space5);
  main_sizer->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT,
                  space5);
  main_sizer->AddSpacer(space5);

  Bind(wxEVT_CLOSE_WINDOW, &ControllerConfigDiag::OnClose, this);
  Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnCloseButton, this, wxID_CLOSE);

  UpdateUI();

  SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
  SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
  SetSizerAndFit(main_sizer);
  Center();
}

void ControllerConfigDiag::UpdateUI()
{
  const bool enable_bt_passthrough_mode = SConfig::GetInstance().m_bt_passthrough_enabled;

  for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
  {
    m_wiimote_labels[i]->Enable(!enable_bt_passthrough_mode);
    m_wiimote_sources[i]->Enable(!enable_bt_passthrough_mode);
    m_wiimote_configure_button[i]->Enable(!enable_bt_passthrough_mode);

    m_wiimote_sources[i]->Select(g_wiimote_sources[i]);

    const bool wii_game_started =
        SConfig::GetInstance().bWii || Core::GetState() == Core::State::Uninitialized;
    if (Core::WantsDeterminism() || !wii_game_started)
      m_wiimote_sources[i]->Disable();
    if (!wii_game_started ||
        (g_wiimote_sources[i] != WIIMOTE_SRC_EMU && g_wiimote_sources[i] != WIIMOTE_SRC_HYBRID))
      m_wiimote_configure_button[i]->Disable();
  }

  m_passthrough_sync_text->Enable(enable_bt_passthrough_mode);
  m_passthrough_sync_btn->Enable(enable_bt_passthrough_mode);
  m_passthrough_reset_text->Enable(enable_bt_passthrough_mode);
  m_passthrough_reset_btn->Enable(enable_bt_passthrough_mode);

  m_balance_board_checkbox->Enable(!enable_bt_passthrough_mode);
  m_enable_continuous_scanning->Enable(!enable_bt_passthrough_mode);
  m_refresh_wm_button->Enable(!enable_bt_passthrough_mode);
  m_unsupported_bt_text->Enable(!enable_bt_passthrough_mode);
  m_enable_speaker_data->Enable(!enable_bt_passthrough_mode);

  // Disable some controls when emulation is running
  if (Core::IsRunning())
  {
    if (!SConfig::GetInstance().bWii || NetPlay::IsNetPlayRunning())
    {
      m_passthrough_sync_text->Disable();
      m_passthrough_sync_btn->Disable();
      m_passthrough_reset_text->Disable();
      m_passthrough_reset_btn->Disable();

      for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
      {
        m_wiimote_labels[i]->Disable();
        m_wiimote_sources[i]->Disable();
      }
      m_balance_board_checkbox->Disable();
    }

    m_passthrough_bt_radio->Disable();
    m_emulated_bt_radio->Disable();

    if (!SConfig::GetInstance().bWii)
    {
      m_enable_continuous_scanning->Disable();
      m_refresh_wm_button->Disable();
      m_unsupported_bt_text->Disable();
      m_enable_speaker_data->Disable();
    }
  }
}

wxSizer* ControllerConfigDiag::CreateGamecubeSizer()
{
  const int space5 = FromDIP(5);

  auto* gamecube_static_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("GameCube Controllers"));
  auto* gamecube_flex_sizer = new wxFlexGridSizer(3, space5, space5);
  gamecube_flex_sizer->AddGrowableCol(1);
  gamecube_static_sizer->AddSpacer(space5);
  gamecube_static_sizer->Add(gamecube_flex_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  gamecube_static_sizer->AddSpacer(space5);

  wxStaticText* pad_labels[4];
  wxChoice* pad_type_choices[4];

  for (int i = 0; i < 4; i++)
  {
    pad_labels[i] = new wxStaticText(gamecube_static_sizer->GetStaticBox(), wxID_ANY,
                                     wxString::Format(_("Port %i"), i + 1));

    // Create an ID for the config button.
    const wxWindowID button_id = wxWindow::NewControlId();
    m_gc_port_from_config_id.emplace(button_id, i);
    m_gc_port_configure_button[i] =
        new wxButton(gamecube_static_sizer->GetStaticBox(), button_id, _("Configure"),
                     wxDefaultPosition, wxDLG_UNIT(this, wxSize(60, -1)));
    m_gc_port_configure_button[i]->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnGameCubeConfigButton,
                                        this);

    // Create a control ID for the choice boxes on the fly.
    const wxWindowID choice_id = wxWindow::NewControlId();
    m_gc_port_from_choice_id.emplace(choice_id, i);

    pad_type_choices[i] =
        new wxChoice(gamecube_static_sizer->GetStaticBox(), choice_id, wxDefaultPosition,
                     wxDefaultSize, m_gc_pad_type_strs.size(), m_gc_pad_type_strs.data());

    pad_type_choices[i]->Bind(wxEVT_CHOICE, &ControllerConfigDiag::OnGameCubePortChanged, this);

    // Disable controller type selection for certain circumstances.
    if (Core::WantsDeterminism())
      pad_type_choices[i]->Disable();

    // Set the saved pad type as the default choice.
    switch (SConfig::GetInstance().m_SIDevice[i])
    {
    case SerialInterface::SIDEVICE_GC_CONTROLLER:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[1]);
      break;
    case SerialInterface::SIDEVICE_WIIU_ADAPTER:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[2]);
      break;
    case SerialInterface::SIDEVICE_GC_STEERING:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[3]);
      break;
    case SerialInterface::SIDEVICE_DANCEMAT:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[4]);
      break;
    case SerialInterface::SIDEVICE_GC_TARUKONGA:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[5]);
      break;
    case SerialInterface::SIDEVICE_GC_GBA:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[6]);
      m_gc_port_configure_button[i]->Disable();
      break;
    case SerialInterface::SIDEVICE_GC_KEYBOARD:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[7]);
      break;
    default:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[0]);
      m_gc_port_configure_button[i]->Disable();
      break;
    }

    // Add to the sizer
    gamecube_flex_sizer->Add(pad_labels[i], 0, wxALIGN_CENTER_VERTICAL);
    gamecube_flex_sizer->Add(WxUtils::GiveMinSize(pad_type_choices[i], wxDefaultSize), 0, wxEXPAND);
    gamecube_flex_sizer->Add(m_gc_port_configure_button[i], 0, wxEXPAND);
  }

  return gamecube_static_sizer;
}

void ControllerConfigDiag::OnBackgroundInputChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_BackgroundInput = event.IsChecked();
}

wxSizer* ControllerConfigDiag::CreateAdvancedSettingsSizer()
{
  const int space5 = FromDIP(5);

  m_background_input_checkbox = new wxCheckBox(this, wxID_ANY, _("Background Input"));
  m_background_input_checkbox->SetValue(SConfig::GetInstance().m_BackgroundInput);
  m_background_input_checkbox->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnBackgroundInputChanged,
                                    this);

  auto* const box = new wxStaticBoxSizer(wxVERTICAL, this, _("Advanced Settings"));
  box->Add(m_background_input_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  return box;
}

wxSizer* ControllerConfigDiag::CreateWiimoteConfigSizer()
{
  const int space5 = FromDIP(5);
  const int space20 = FromDIP(20);

  auto* const box = new wxStaticBoxSizer(wxVERTICAL, this, _("Wii Remotes"));

  m_passthrough_bt_radio = new wxRadioButton(this, wxID_ANY, _("Passthrough a Bluetooth Adapter"),
                                             wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  m_passthrough_bt_radio->Bind(wxEVT_RADIOBUTTON, &ControllerConfigDiag::OnBluetoothModeChanged,
                               this);
  box->AddSpacer(space5);
  box->Add(m_passthrough_bt_radio, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  box->AddSpacer(space5);
  box->Add(CreatePassthroughBTConfigSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);

  box->AddSpacer(space20);

  m_emulated_bt_radio = new wxRadioButton(this, wxID_ANY, _("Emulate the Wii's Bluetooth Adapter"));
  m_emulated_bt_radio->Bind(wxEVT_RADIOBUTTON, &ControllerConfigDiag::OnBluetoothModeChanged, this);

  box->Add(m_emulated_bt_radio, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  box->AddSpacer(space5);
  box->Add(CreateEmulatedBTConfigSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  box->AddSpacer(space5);

  if (SConfig::GetInstance().m_bt_passthrough_enabled)
    m_passthrough_bt_radio->SetValue(true);
  else
    m_emulated_bt_radio->SetValue(true);

  return box;
}

wxSizer* ControllerConfigDiag::CreatePassthroughBTConfigSizer()
{
  m_passthrough_sync_text =
      new wxStaticText(this, wxID_ANY, _("Sync real Wii Remotes and pair them"));
  m_passthrough_sync_btn =
      new wxButton(this, wxID_ANY, _("Sync"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(60, -1)));
  m_passthrough_sync_btn->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnPassthroughScanButton, this);

  m_passthrough_reset_text =
      new wxStaticText(this, wxID_ANY, _("Reset all saved Wii Remote pairings"));
  m_passthrough_reset_btn =
      new wxButton(this, wxID_ANY, _("Reset"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(60, -1)));
  m_passthrough_reset_btn->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnPassthroughResetButton,
                                this);

  const int space5 = FromDIP(5);

  auto* grid = new wxFlexGridSizer(3, space5, space5);
  grid->AddGrowableCol(1);

  grid->Add(m_passthrough_sync_text, 0, wxALIGN_CENTER_VERTICAL);
  grid->AddSpacer(1);
  grid->Add(m_passthrough_sync_btn, 0, wxEXPAND);

  grid->Add(m_passthrough_reset_text, 0, wxALIGN_CENTER_VERTICAL);
  grid->AddSpacer(1);
  grid->Add(m_passthrough_reset_btn, 0, wxEXPAND);

  return grid;
}

wxSizer* ControllerConfigDiag::CreateEmulatedBTConfigSizer()
{
  const std::array<wxString, 4> src_choices{
      {_("None"), _("Emulated Wii Remote"), _("Real Wii Remote"), _("Hybrid Wii Remote")}};

  const int space5 = FromDIP(5);

  // Source selector grid
  auto* const grid = new wxFlexGridSizer(3, space5, space5);
  grid->AddGrowableCol(1);

  for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
  {
    // reserve four ids, so that we can calculate the index from the ids later on
    // Stupid wx 2.8 doesn't support reserving sequential IDs, so we need to do that more
    // complicated..
    int source_ctrl_id = wxWindow::NewControlId();
    m_wiimote_index_from_choice_id.emplace(source_ctrl_id, i);

    int config_bt_id = wxWindow::NewControlId();
    m_wiimote_index_from_config_id.emplace(config_bt_id, i);

    m_wiimote_labels[i] =
        new wxStaticText(this, wxID_ANY, wxString::Format(_("Wii Remote %i"), i + 1));
    m_wiimote_sources[i] = new wxChoice(this, source_ctrl_id, wxDefaultPosition, wxDefaultSize,
                                        src_choices.size(), src_choices.data());
    m_wiimote_sources[i]->Bind(wxEVT_CHOICE, &ControllerConfigDiag::OnWiimoteSourceChanged, this);

    m_wiimote_configure_button[i] = new wxButton(
        this, config_bt_id, _("Configure"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(60, -1)));
    m_wiimote_configure_button[i]->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnWiimoteConfigButton,
                                        this);

    grid->Add(m_wiimote_labels[i], 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(WxUtils::GiveMinSize(m_wiimote_sources[i], wxDefaultSize), 0, wxEXPAND);
    grid->Add(m_wiimote_configure_button[i], 0, wxEXPAND);
  }

  // Scanning controls
  m_enable_continuous_scanning = new wxCheckBox(this, wxID_ANY, _("Continuous Scanning"));
  m_enable_continuous_scanning->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnContinuousScanning,
                                     this);
  m_enable_continuous_scanning->SetValue(SConfig::GetInstance().m_WiimoteContinuousScanning);
  m_refresh_wm_button = new wxButton(this, wxID_ANY, _("Refresh"), wxDefaultPosition,
                                     wxDLG_UNIT(this, wxSize(60, -1)));
  m_refresh_wm_button->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnWiimoteRefreshButton, this);

  m_unsupported_bt_text =
      new wxStaticText(this, wxID_ANY, _("A supported Bluetooth device could not be found,\n"
                                         "so you must connect Wii Remotes manually."));
  m_unsupported_bt_text->Show(!WiimoteReal::g_wiimote_scanner.IsReady());

  // Balance Board
  m_balance_board_checkbox = new wxCheckBox(this, wxID_ANY, _("Real Balance Board"));
  m_balance_board_checkbox->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnBalanceBoardChanged,
                                 this);
  m_balance_board_checkbox->SetValue(g_wiimote_sources[WIIMOTE_BALANCE_BOARD] == WIIMOTE_SRC_REAL);

  // Speaker data
  m_enable_speaker_data = new wxCheckBox(this, wxID_ANY, _("Enable Speaker Data"));
  m_enable_speaker_data->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnEnableSpeaker, this);
  m_enable_speaker_data->SetValue(SConfig::GetInstance().m_WiimoteEnableSpeaker);

  auto* const checkbox_sizer = new wxBoxSizer(wxVERTICAL);
  checkbox_sizer->Add(m_enable_continuous_scanning);
  checkbox_sizer->AddSpacer(space5);
  checkbox_sizer->Add(m_balance_board_checkbox);
  checkbox_sizer->AddSpacer(space5);
  checkbox_sizer->Add(m_enable_speaker_data);

  auto* const scanning_sizer = new wxBoxSizer(wxHORIZONTAL);
  scanning_sizer->Add(checkbox_sizer, 1, wxEXPAND);
  scanning_sizer->Add(m_refresh_wm_button, 0, wxALIGN_TOP);

  auto* const sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(grid, 0, wxEXPAND);
  sizer->AddSpacer(space5);
  sizer->Add(m_unsupported_bt_text, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, space5);
  sizer->AddSpacer(space5);
  sizer->Add(scanning_sizer, 0, wxEXPAND);

  return sizer;
}

void ControllerConfigDiag::OnClose(wxCloseEvent& event)
{
  // Save all settings
  SConfig::GetInstance().SaveSettings();
  UICommon::SaveWiimoteSources();
  EndModal(wxID_OK);
}

void ControllerConfigDiag::OnCloseButton(wxCommandEvent& event)
{
  Close();
}

void ControllerConfigDiag::OnGameCubePortChanged(wxCommandEvent& event)
{
  const unsigned int device_num = m_gc_port_from_choice_id[event.GetId()];
  const wxString device_name = event.GetString();

  SerialInterface::SIDevices tempType;
  if (device_name == m_gc_pad_type_strs[1])
  {
    tempType = SerialInterface::SIDEVICE_GC_CONTROLLER;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[2])
  {
    tempType = SerialInterface::SIDEVICE_WIIU_ADAPTER;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[3])
  {
    tempType = SerialInterface::SIDEVICE_GC_STEERING;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[4])
  {
    tempType = SerialInterface::SIDEVICE_DANCEMAT;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[5])
  {
    tempType = SerialInterface::SIDEVICE_GC_TARUKONGA;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[6])
  {
    tempType = SerialInterface::SIDEVICE_GC_GBA;
    m_gc_port_configure_button[device_num]->Disable();
  }
  else if (device_name == m_gc_pad_type_strs[7])
  {
    tempType = SerialInterface::SIDEVICE_GC_KEYBOARD;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else
  {
    tempType = SerialInterface::SIDEVICE_NONE;
    m_gc_port_configure_button[device_num]->Disable();
  }

  SConfig::GetInstance().m_SIDevice[device_num] = tempType;

  if (GCAdapter::UseAdapter())
    GCAdapter::StartScanThread();
  else
    GCAdapter::StopScanThread();

  if (Core::IsRunning())
    SerialInterface::ChangeDevice(tempType, device_num);
}

void ControllerConfigDiag::OnGameCubeConfigButton(wxCommandEvent& event)
{
  InputConfig* const pad_plugin = Pad::GetConfig();
  InputConfig* const key_plugin = Keyboard::GetConfig();
  const int port_num = m_gc_port_from_config_id[event.GetId()];
  const auto device_type = SConfig::GetInstance().m_SIDevice[port_num];

  HotkeyManagerEmu::Enable(false);

  if (device_type == SerialInterface::SIDEVICE_GC_KEYBOARD)
  {
    GCKeyboardInputConfigDialog config_diag(
        this, *key_plugin,
        wxString::Format(_("GameCube Keyboard Configuration Port %i"), port_num + 1), port_num);
    config_diag.ShowModal();
  }
  else if (device_type == SerialInterface::SIDEVICE_WIIU_ADAPTER)
  {
    GCAdapterConfigDiag config_diag(
        this, wxString::Format(_("Wii U GameCube Controller Adapter Configuration Port %i"),
                               port_num + 1),
        port_num);
    config_diag.ShowModal();
  }
  else
  {
    GCPadInputConfigDialog config_diag(
        this, *pad_plugin,
        wxString::Format(_("GameCube Controller Configuration Port %i"), port_num + 1), port_num);
    config_diag.ShowModal();
  }

  HotkeyManagerEmu::Enable(true);
}

void ControllerConfigDiag::OnWiimoteSourceChanged(wxCommandEvent& event)
{
  // This needs to be changed now in order for refresh to work right.
  // Revert if the dialog is canceled.
  int index = m_wiimote_index_from_choice_id[event.GetId()];

  if (index != WIIMOTE_BALANCE_BOARD)
  {
    WiimoteReal::ChangeWiimoteSource(index, event.GetInt());
    if (g_wiimote_sources[index] != WIIMOTE_SRC_EMU &&
        g_wiimote_sources[index] != WIIMOTE_SRC_HYBRID)
      m_wiimote_configure_button[index]->Disable();
    else
      m_wiimote_configure_button[index]->Enable();
  }
  else
  {
    WiimoteReal::ChangeWiimoteSource(index, event.GetInt() ? WIIMOTE_SRC_REAL : WIIMOTE_SRC_NONE);
  }
}

void ControllerConfigDiag::OnWiimoteConfigButton(wxCommandEvent& ev)
{
  InputConfig* const wiimote_plugin = Wiimote::GetConfig();

  HotkeyManagerEmu::Enable(false);

  const int port_num = m_wiimote_index_from_config_id[ev.GetId()];

  WiimoteInputConfigDialog m_ConfigFrame(
      this, *wiimote_plugin,
      wxString::Format(_("Dolphin Emulated Wii Remote Configuration Port %i"), port_num + 1),
      port_num);
  m_ConfigFrame.ShowModal();

  HotkeyManagerEmu::Enable(true);
}

void ControllerConfigDiag::OnBluetoothModeChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_bt_passthrough_enabled = m_passthrough_bt_radio->GetValue();
  WiimoteReal::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
  UpdateUI();
}

void ControllerConfigDiag::OnPassthroughScanButton(wxCommandEvent& event)
{
  const auto ios = IOS::HLE::GetIOS();
  if (!ios)
  {
    wxMessageBox(_("A sync can only be triggered when a Wii game is running."),
                 _("Sync Wii Remotes"), wxICON_WARNING);
    return;
  }
  auto device = ios->GetDeviceByName("/dev/usb/oh1/57e/305");
  if (device != nullptr)
    std::static_pointer_cast<IOS::HLE::Device::BluetoothBase>(device)
        ->TriggerSyncButtonPressedEvent();
}

void ControllerConfigDiag::OnPassthroughResetButton(wxCommandEvent& event)
{
  const auto ios = IOS::HLE::GetIOS();
  if (!ios)
  {
    wxMessageBox(_("Saved Wii Remote pairings can only be reset when a Wii game is running."),
                 _("Reset Wii Remote pairings"), wxICON_WARNING);
    return;
  }
  auto device = ios->GetDeviceByName("/dev/usb/oh1/57e/305");
  if (device != nullptr)
    std::static_pointer_cast<IOS::HLE::Device::BluetoothBase>(device)->TriggerSyncButtonHeldEvent();
}

void ControllerConfigDiag::OnBalanceBoardChanged(wxCommandEvent& event)
{
  WiimoteReal::ChangeWiimoteSource(WIIMOTE_BALANCE_BOARD,
                                   event.IsChecked() ? WIIMOTE_SRC_REAL : WIIMOTE_SRC_NONE);
}

void ControllerConfigDiag::OnContinuousScanning(wxCommandEvent& event)
{
  SConfig::GetInstance().m_WiimoteContinuousScanning = event.IsChecked();
  WiimoteReal::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
}

void ControllerConfigDiag::OnWiimoteRefreshButton(wxCommandEvent&)
{
  WiimoteReal::Refresh();
}

void ControllerConfigDiag::OnEnableSpeaker(wxCommandEvent& event)
{
  SConfig::GetInstance().m_WiimoteEnableSpeaker = event.IsChecked();
}
