// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <wx/dialog.h>

#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"
#include "InputCommon/GCAdapter.h"

class InputConfig;
class wxButton;
class wxStaticBoxSizer;

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

  void OnEnableSpeaker(wxCommandEvent& event)
  {
    SConfig::GetInstance().m_WiimoteEnableSpeaker = event.IsChecked();
    event.Skip();
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
};
