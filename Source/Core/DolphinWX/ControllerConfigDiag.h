// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_real.h"
#include "InputCommon/GCAdapter.h"

class InputConfig;

class ControllerConfigDiag : public wxDialog
{
public:
  ControllerConfigDiag(wxWindow* const parent);

private:
  void OnSensorBarPos(wxCommandEvent& event)
  {
    SConfig::GetInstance().m_SYSCONF->SetData("BT.BAR", event.GetInt());
    event.Skip();
  }

  void OnSensorBarSensitivity(wxCommandEvent& event)
  {
    SConfig::GetInstance().m_SYSCONF->SetData("BT.SENS", event.GetInt());
    event.Skip();
  }

  void OnSpeakerVolume(wxCommandEvent& event)
  {
    SConfig::GetInstance().m_SYSCONF->SetData("BT.SPKV", event.GetInt());
    event.Skip();
  }

  void OnMotor(wxCommandEvent& event)
  {
    SConfig::GetInstance().m_SYSCONF->SetData("BT.MOT", event.GetInt());
    event.Skip();
  }

  void OnContinuousScanning(wxCommandEvent& event)
  {
    SConfig::GetInstance().m_WiimoteContinuousScanning = event.IsChecked();
    WiimoteReal::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
    event.Skip();
  }

  void OnPassthroughMode(wxCommandEvent& event)
  {
    SConfig::GetInstance().m_bt_passthrough_enabled = event.IsChecked();
    UpdateUI();
  }

  void OnPassthroughScanButton(wxCommandEvent& event)
  {
    auto device = WII_IPC_HLE_Interface::GetDeviceByName("/dev/usb/oh1/57e/305");
    if (device != nullptr)
      std::static_pointer_cast<CWII_IPC_HLE_Device_usb_oh1_57e_305_base>(device)
          ->TriggerSyncButtonPressedEvent();
    event.Skip();
  }

  void OnPassthroughResetButton(wxCommandEvent& event)
  {
    auto device = WII_IPC_HLE_Interface::GetDeviceByName("/dev/usb/oh1/57e/305");
    if (device != nullptr)
      std::static_pointer_cast<CWII_IPC_HLE_Device_usb_oh1_57e_305_base>(device)
          ->TriggerSyncButtonHeldEvent();
    event.Skip();
  }

  void OnEnableSpeaker(wxCommandEvent& event)
  {
    SConfig::GetInstance().m_WiimoteEnableSpeaker = event.IsChecked();
    event.Skip();
  }

  void UpdateUI()
  {
    const bool enable_bt_passthrough_mode = SConfig::GetInstance().m_bt_passthrough_enabled;
    m_real_wiimotes_sizer->ShowItems(!enable_bt_passthrough_mode);
    m_bt_passthrough_sizer->ShowItems(enable_bt_passthrough_mode);
    m_unsupported_bt_text->Show(!enable_bt_passthrough_mode);
    m_bt_passthrough_text->Show(enable_bt_passthrough_mode);
    m_balance_board_group->ShowItems(!enable_bt_passthrough_mode);
    m_enable_speaker_data->Enable(!enable_bt_passthrough_mode);
    for (int i = 0; i < MAX_WIIMOTES; ++i)
    {
      m_wiimote_labels[i]->Enable(!enable_bt_passthrough_mode);
      m_wiimote_sources[i]->Enable(!enable_bt_passthrough_mode);
      m_wiimote_configure_button[i]->Enable(!enable_bt_passthrough_mode);
    }
    Layout();
    Fit();
  }

  wxStaticBoxSizer* CreateGamecubeSizer();
  wxStaticBoxSizer* CreateWiimoteConfigSizer();
  wxStaticBoxSizer* CreateBalanceBoardSizer();
  wxStaticBoxSizer* CreateRealWiimoteSizer();
  wxStaticBoxSizer* CreateGeneralWiimoteSettingsSizer();

  void OnClose(wxCloseEvent& event);
  void OnCloseButton(wxCommandEvent& event);

  void OnGameCubePortChanged(wxCommandEvent& event);
  void OnGameCubeConfigButton(wxCommandEvent& event);

  void OnWiimoteSourceChanged(wxCommandEvent& event);
  void OnWiimoteConfigButton(wxCommandEvent& event);
  void OnWiimoteRefreshButton(wxCommandEvent& event);
  void SaveWiimoteSource();

  std::map<wxWindowID, unsigned int> m_gc_port_from_choice_id;
  std::map<wxWindowID, unsigned int> m_gc_port_from_config_id;
  std::array<wxButton*, 4> m_gc_port_configure_button;
  std::array<wxString, 8> m_gc_pad_type_strs;

  std::map<wxWindowID, unsigned int> m_wiimote_index_from_choice_id;
  std::map<wxWindowID, unsigned int> m_wiimote_index_from_config_id;
  std::array<wxButton*, MAX_WIIMOTES> m_wiimote_configure_button;

  std::array<wxStaticText*, 4> m_wiimote_labels;
  std::array<wxChoice*, 4> m_wiimote_sources;
  wxBoxSizer* m_real_wiimotes_sizer;
  wxBoxSizer* m_bt_passthrough_sizer;
  wxStaticText* m_unsupported_bt_text;
  wxStaticText* m_bt_passthrough_text;
  wxStaticBoxSizer* m_general_wm_settings;
  wxStaticBoxSizer* m_balance_board_group;
  wxCheckBox* m_enable_speaker_data;
};
