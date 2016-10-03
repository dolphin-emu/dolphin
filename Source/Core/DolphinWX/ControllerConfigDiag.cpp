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
#include <wx/slider.h>
#include <wx/stattext.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/HotkeyManager.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_real.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "DolphinWX/Config/GCAdapterConfigDiag.h"
#include "DolphinWX/ControllerConfigDiag.h"
#include "DolphinWX/InputConfigDiag.h"
#include "InputCommon/GCAdapter.h"

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include "DolphinWX/X11Utils.h"
#endif

ControllerConfigDiag::ControllerConfigDiag(wxWindow* const parent)
    : wxDialog(parent, wxID_ANY, _("Dolphin Controller Configuration"))
{
  m_gc_pad_type_strs = {{_("None"), _("Standard Controller"), _("GameCube Adapter for Wii U"),
                         _("Steering Wheel"), _("Dance Mat"), _("DK Bongos"), _("GBA"),
                         _("Keyboard")}};

  wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);

  // Combine all UI controls into their own encompassing sizer.
  wxBoxSizer* control_sizer = new wxBoxSizer(wxVERTICAL);
  control_sizer->Add(CreateGamecubeSizer(), 0, wxEXPAND | wxALL, 5);
  control_sizer->Add(CreateWiimoteConfigSizer(), 0, wxEXPAND | wxALL, 5);

  main_sizer->Add(control_sizer, 0, wxEXPAND);
  main_sizer->Add(CreateButtonSizer(wxCLOSE), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

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
        SConfig::GetInstance().bWii || Core::GetState() == Core::CORE_UNINITIALIZED;
    if (Core::g_want_determinism || !wii_game_started)
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

wxStaticBoxSizer* ControllerConfigDiag::CreateGamecubeSizer()
{
  wxStaticBoxSizer* const gamecube_static_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("GameCube Controllers"));
  wxFlexGridSizer* const gamecube_flex_sizer = new wxFlexGridSizer(3, 5, 5);
  gamecube_flex_sizer->AddGrowableCol(1);

  wxStaticText* pad_labels[4];
  wxChoice* pad_type_choices[4];

  for (int i = 0; i < 4; i++)
  {
    pad_labels[i] = new wxStaticText(this, wxID_ANY, wxString::Format(_("Port %i"), i + 1));

    // Create an ID for the config button.
    const wxWindowID button_id = wxWindow::NewControlId();
    m_gc_port_from_config_id.emplace(button_id, i);
    m_gc_port_configure_button[i] =
        new wxButton(this, button_id, _("Configure"), wxDefaultPosition, wxSize(100, -1));
    m_gc_port_configure_button[i]->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnGameCubeConfigButton,
                                        this);

    // Create a control ID for the choice boxes on the fly.
    const wxWindowID choice_id = wxWindow::NewControlId();
    m_gc_port_from_choice_id.emplace(choice_id, i);

    pad_type_choices[i] = new wxChoice(this, choice_id, wxDefaultPosition, wxDefaultSize,
                                       m_gc_pad_type_strs.size(), m_gc_pad_type_strs.data());

    pad_type_choices[i]->Bind(wxEVT_CHOICE, &ControllerConfigDiag::OnGameCubePortChanged, this);

    // Disable controller type selection for certain circumstances.
    if (Core::g_want_determinism)
      pad_type_choices[i]->Disable();

    // Set the saved pad type as the default choice.
    switch (SConfig::GetInstance().m_SIDevice[i])
    {
    case SIDEVICE_GC_CONTROLLER:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[1]);
      break;
    case SIDEVICE_WIIU_ADAPTER:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[2]);
      break;
    case SIDEVICE_GC_STEERING:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[3]);
      break;
    case SIDEVICE_DANCEMAT:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[4]);
      break;
    case SIDEVICE_GC_TARUKONGA:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[5]);
      break;
    case SIDEVICE_GC_GBA:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[6]);
      m_gc_port_configure_button[i]->Disable();
      break;
    case SIDEVICE_GC_KEYBOARD:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[7]);
      break;
    default:
      pad_type_choices[i]->SetStringSelection(m_gc_pad_type_strs[0]);
      m_gc_port_configure_button[i]->Disable();
      break;
    }

    // Add to the sizer
    gamecube_flex_sizer->Add(pad_labels[i], 0, wxALIGN_CENTER_VERTICAL);
    gamecube_flex_sizer->Add(pad_type_choices[i], 0, wxALIGN_CENTER_VERTICAL | wxEXPAND);
    gamecube_flex_sizer->Add(m_gc_port_configure_button[i], 1, wxEXPAND);
  }

  gamecube_static_sizer->Add(gamecube_flex_sizer, 0, wxEXPAND | wxALL, 5);
  gamecube_static_sizer->AddSpacer(5);

  return gamecube_static_sizer;
}

wxStaticBoxSizer* ControllerConfigDiag::CreateWiimoteConfigSizer()
{
  auto* const box = new wxStaticBoxSizer(wxVERTICAL, this, _("Wiimotes"));

  m_passthrough_bt_radio = new wxRadioButton(this, wxID_ANY, _("Passthrough a Bluetooth adapter"),
                                             wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  m_passthrough_bt_radio->Bind(wxEVT_RADIOBUTTON, &ControllerConfigDiag::OnBluetoothModeChanged,
                               this);
  box->Add(m_passthrough_bt_radio, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 5);
  box->Add(CreatePassthroughBTConfigSizer(), 0, wxALL | wxEXPAND, 5);

  box->AddSpacer(10);

  m_emulated_bt_radio = new wxRadioButton(this, wxID_ANY, _("Emulate the Wii's Bluetooth adapter"));
  m_emulated_bt_radio->Bind(wxEVT_RADIOBUTTON, &ControllerConfigDiag::OnBluetoothModeChanged, this);

  box->Add(m_emulated_bt_radio, 0, wxALL | wxEXPAND, 5);
  box->Add(CreateEmulatedBTConfigSizer(), 0, wxALL | wxEXPAND, 5);
  box->AddSpacer(5);

  if (SConfig::GetInstance().m_bt_passthrough_enabled)
    m_passthrough_bt_radio->SetValue(true);
  else
    m_emulated_bt_radio->SetValue(true);

  return box;
}

wxBoxSizer* ControllerConfigDiag::CreatePassthroughBTConfigSizer()
{
  auto* const sizer = new wxBoxSizer(wxVERTICAL);

  m_passthrough_sync_text = new wxStaticText(this, wxID_ANY, _("Sync real Wiimotes and pair them"));
  m_passthrough_sync_btn =
      new wxButton(this, wxID_ANY, _("Sync"), wxDefaultPosition, wxSize(100, -1));
  m_passthrough_sync_btn->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnPassthroughScanButton, this);

  m_passthrough_reset_text =
      new wxStaticText(this, wxID_ANY, _("Reset all saved Wiimote pairings"));
  m_passthrough_reset_btn =
      new wxButton(this, wxID_ANY, _("Reset"), wxDefaultPosition, wxSize(100, -1));
  m_passthrough_reset_btn->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnPassthroughResetButton,
                                this);

  auto* const sync_sizer = new wxBoxSizer(wxHORIZONTAL);
  sync_sizer->Add(m_passthrough_sync_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
  sync_sizer->AddStretchSpacer();
  sync_sizer->Add(m_passthrough_sync_btn, 0, wxEXPAND);
  auto* const reset_sizer = new wxBoxSizer(wxHORIZONTAL);
  reset_sizer->Add(m_passthrough_reset_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
  reset_sizer->AddStretchSpacer();
  reset_sizer->Add(m_passthrough_reset_btn, 0, wxEXPAND);

  sizer->Add(sync_sizer, 0, wxEXPAND);
  sizer->AddSpacer(5);
  sizer->Add(reset_sizer, 0, wxEXPAND);
  return sizer;
}

wxBoxSizer* ControllerConfigDiag::CreateEmulatedBTConfigSizer()
{
  static const std::array<wxString, 4> src_choices = {
      {_("None"), _("Emulated Wiimote"), _("Real Wiimote"), _("Hybrid Wiimote")}};

  auto* const sizer = new wxBoxSizer(wxVERTICAL);

  // Source selector grid
  auto* const grid = new wxFlexGridSizer(3, 5, 5);
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
        new wxStaticText(this, wxID_ANY, wxString::Format(_("Wiimote %i"), i + 1));
    m_wiimote_sources[i] = new wxChoice(this, source_ctrl_id, wxDefaultPosition, wxDefaultSize,
                                        src_choices.size(), src_choices.data());
    m_wiimote_sources[i]->Bind(wxEVT_CHOICE, &ControllerConfigDiag::OnWiimoteSourceChanged, this);

    m_wiimote_configure_button[i] =
        new wxButton(this, config_bt_id, _("Configure"), wxDefaultPosition, wxSize(100, -1));
    m_wiimote_configure_button[i]->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnWiimoteConfigButton,
                                        this);

    grid->Add(m_wiimote_labels[i], 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(m_wiimote_sources[i], 0, wxALIGN_CENTER_VERTICAL | wxEXPAND);
    grid->Add(m_wiimote_configure_button[i], 1, wxEXPAND);
  }

  sizer->Add(grid, 0, wxEXPAND);
  sizer->AddSpacer(5);

  // Scanning controls
  m_enable_continuous_scanning = new wxCheckBox(this, wxID_ANY, _("Continuous Scanning"));
  m_enable_continuous_scanning->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnContinuousScanning,
                                     this);
  m_enable_continuous_scanning->SetValue(SConfig::GetInstance().m_WiimoteContinuousScanning);
  m_refresh_wm_button =
      new wxButton(this, wxID_ANY, _("Refresh"), wxDefaultPosition, wxSize(100, -1));
  m_refresh_wm_button->Bind(wxEVT_BUTTON, &ControllerConfigDiag::OnWiimoteRefreshButton, this);

  m_unsupported_bt_text =
      new wxStaticText(this, wxID_ANY, _("A supported Bluetooth device could not be found,\n"
                                         "so you must connect Wiimotes manually."));
  m_unsupported_bt_text->Show(!WiimoteReal::g_wiimote_scanner.IsReady());
  sizer->Add(m_unsupported_bt_text, 0, wxALIGN_CENTER | wxALL, 5);

  auto* const scanning_sizer = new wxBoxSizer(wxHORIZONTAL);
  scanning_sizer->Add(m_enable_continuous_scanning, 0, wxALIGN_CENTER_VERTICAL);
  scanning_sizer->AddStretchSpacer();
  scanning_sizer->Add(m_refresh_wm_button, 0, wxALL | wxEXPAND);
  sizer->Add(scanning_sizer, 0, wxEXPAND);

  // Balance Board
  m_balance_board_checkbox = new wxCheckBox(this, wxID_ANY, _("Real Balance Board"));
  m_balance_board_checkbox->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnBalanceBoardChanged,
                                 this);
  m_balance_board_checkbox->SetValue(g_wiimote_sources[WIIMOTE_BALANCE_BOARD] == WIIMOTE_SRC_REAL);
  sizer->Add(m_balance_board_checkbox);
  sizer->AddSpacer(5);

  // Speaker data
  m_enable_speaker_data = new wxCheckBox(this, wxID_ANY, _("Enable Speaker Data"));
  m_enable_speaker_data->Bind(wxEVT_CHECKBOX, &ControllerConfigDiag::OnEnableSpeaker, this);
  m_enable_speaker_data->SetValue(SConfig::GetInstance().m_WiimoteEnableSpeaker);
  sizer->Add(m_enable_speaker_data);

  return sizer;
}

void ControllerConfigDiag::OnClose(wxCloseEvent& event)
{
  // Save all settings
  SConfig::GetInstance().SaveSettings();
  SaveWiimoteSource();
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

  SIDevices tempType;
  if (device_name == m_gc_pad_type_strs[1])
  {
    tempType = SIDEVICE_GC_CONTROLLER;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[2])
  {
    tempType = SIDEVICE_WIIU_ADAPTER;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[3])
  {
    tempType = SIDEVICE_GC_STEERING;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[4])
  {
    tempType = SIDEVICE_DANCEMAT;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[5])
  {
    tempType = SIDEVICE_GC_TARUKONGA;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else if (device_name == m_gc_pad_type_strs[6])
  {
    tempType = SIDEVICE_GC_GBA;
    m_gc_port_configure_button[device_num]->Disable();
  }
  else if (device_name == m_gc_pad_type_strs[7])
  {
    tempType = SIDEVICE_GC_KEYBOARD;
    m_gc_port_configure_button[device_num]->Enable();
  }
  else
  {
    tempType = SIDEVICE_NONE;
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

  HotkeyManagerEmu::Enable(false);

  if (SConfig::GetInstance().m_SIDevice[port_num] == SIDEVICE_GC_KEYBOARD)
  {
    InputConfigDialog m_ConfigFrame(this, *key_plugin, _("GameCube Controller Configuration"),
                                    port_num);
    m_ConfigFrame.ShowModal();
  }
  else if (SConfig::GetInstance().m_SIDevice[port_num] == SIDEVICE_WIIU_ADAPTER)
  {
    GCAdapterConfigDiag m_ConfigFramg(this, _("Wii U Gamecube Controller Adapter Configuration"),
                                      port_num);
    m_ConfigFramg.ShowModal();
  }
  else
  {
    InputConfigDialog m_ConfigFrame(this, *pad_plugin, _("GameCube Controller Configuration"),
                                    port_num);
    m_ConfigFrame.ShowModal();
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

  InputConfigDialog m_ConfigFrame(this, *wiimote_plugin,
                                  _("Dolphin Emulated Wiimote Configuration"),
                                  m_wiimote_index_from_config_id[ev.GetId()]);
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
  if (!Core::IsRunning())
  {
    wxMessageBox(_("A sync can only be triggered when a Wii game is running."), _("Sync Wiimotes"),
                 wxICON_WARNING);
    return;
  }
  auto device = WII_IPC_HLE_Interface::GetDeviceByName("/dev/usb/oh1/57e/305");
  if (device != nullptr)
    std::static_pointer_cast<CWII_IPC_HLE_Device_usb_oh1_57e_305_base>(device)
        ->TriggerSyncButtonPressedEvent();
}

void ControllerConfigDiag::OnPassthroughResetButton(wxCommandEvent& event)
{
  if (!Core::IsRunning())
  {
    wxMessageBox(_("Saved Wiimote pairings can only be reset when a Wii game is running."),
                 _("Reset Wiimote pairings"), wxICON_WARNING);
    return;
  }
  auto device = WII_IPC_HLE_Interface::GetDeviceByName("/dev/usb/oh1/57e/305");
  if (device != nullptr)
    std::static_pointer_cast<CWII_IPC_HLE_Device_usb_oh1_57e_305_base>(device)
        ->TriggerSyncButtonHeldEvent();
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

void ControllerConfigDiag::SaveWiimoteSource()
{
  std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + WIIMOTE_INI_NAME ".ini";

  IniFile inifile;
  inifile.Load(ini_filename);

  for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
  {
    std::string secname("Wiimote");
    secname += (char)('1' + i);
    IniFile::Section& sec = *inifile.GetOrCreateSection(secname);

    sec.Set("Source", (int)g_wiimote_sources[i]);
  }

  std::string secname("BalanceBoard");
  IniFile::Section& sec = *inifile.GetOrCreateSection(secname);
  sec.Set("Source", (int)g_wiimote_sources[WIIMOTE_BALANCE_BOARD]);

  inifile.Save(ini_filename);
}
