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
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_real.h"
#include "InputCommon/GCAdapter.h"

class InputConfig;

class ControllerConfigDiag : public wxDialog
{
public:
  ControllerConfigDiag(wxWindow* const parent);

private:
  void RefreshRealWiimotes(wxCommandEvent& event);

  void ConfigEmulatedWiimote(wxCommandEvent& event);

  void SelectSource(wxCommandEvent& event);
  void RevertSource();

  void Save(wxCommandEvent& event);

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
    CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TriggerSyncButtonPressedEvent();
    event.Skip();
  }

  void OnPassthroughResetButton(wxCommandEvent& event)
  {
    CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TriggerSyncButtonHeldEvent();
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
      wiimote_configure_bt[i]->Enable(!enable_bt_passthrough_mode);
    }
    Layout();
    Fit();
  }

  wxStaticBoxSizer* CreateGamecubeSizer();
  wxStaticBoxSizer* CreateWiimoteConfigSizer();
  wxStaticBoxSizer* CreateBalanceBoardSizer();
  wxStaticBoxSizer* CreateRealWiimoteSizer();
  wxStaticBoxSizer* CreateGeneralWiimoteSettingsSizer();

  void Cancel(wxCommandEvent& event);
  void OnGameCubePortChanged(wxCommandEvent& event);
  void OnGameCubeConfigButton(wxCommandEvent& event);

  std::map<wxWindowID, unsigned int> m_gc_port_choice_ids;
  std::map<wxWindowID, unsigned int> m_gc_port_config_ids;
  std::array<wxString, 8> m_gc_pad_type_strs;

  std::map<wxWindowID, unsigned int> m_wiimote_index_from_ctrl_id;
  unsigned int m_orig_wiimote_sources[MAX_BBMOTES];

  std::array<wxButton*, MAX_WIIMOTES> wiimote_configure_bt;
  std::array<wxButton*, 4> gamecube_configure_bt;
  std::map<wxWindowID, unsigned int> m_wiimote_index_from_conf_bt_id;

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
