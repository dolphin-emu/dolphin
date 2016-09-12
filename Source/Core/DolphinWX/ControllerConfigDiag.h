// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>

#include <wx/dialog.h>

#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"
#include "InputCommon/GCAdapter.h"

class InputConfig;
class wxCheckBox;
class wxChoice;
class wxRadioButton;
class wxStaticBoxSizer;
class wxStaticText;

class ControllerConfigDiag final : public wxDialog
{
public:
  ControllerConfigDiag(wxWindow* const parent);

private:
  void UpdateUI();

  void ConfigEmulatedWiimote(wxCommandEvent& event);

  void SelectSource(wxCommandEvent& event);
  void RevertSource();

  void Cancel(wxCommandEvent& event);
  void Save(wxCommandEvent& event);

  wxStaticBoxSizer* CreateGamecubeSizer();
  wxStaticBoxSizer* CreateWiimoteConfigSizer();
  wxBoxSizer* CreatePassthroughBTConfigSizer();
  wxBoxSizer* CreateEmulatedBTConfigSizer();

  void OnGameCubePortChanged(wxCommandEvent& event);
  void OnGameCubeConfigButton(wxCommandEvent& event);

  void OnBluetoothModeChanged(wxCommandEvent& event);

  void OnPassthroughScanButton(wxCommandEvent& event);
  void OnPassthroughResetButton(wxCommandEvent& event);
  void OnBalanceBoardChanged(wxCommandEvent& event);
  void OnContinuousScanning(wxCommandEvent& event);
  void RefreshRealWiimotes(wxCommandEvent& event);
  void OnEnableSpeaker(wxCommandEvent& event);

  std::map<wxWindowID, unsigned int> m_gc_port_choice_ids;
  std::map<wxWindowID, unsigned int> m_gc_port_config_ids;
  std::array<wxString, 8> m_gc_pad_type_strs;
  std::array<wxButton*, 4> gamecube_configure_bt;

  wxRadioButton* m_passthrough_bt_radio;
  wxRadioButton* m_emulated_bt_radio;

  wxStaticText* m_passthrough_sync_text;
  wxButton* m_passthrough_sync_btn;
  wxStaticText* m_passthrough_reset_text;
  wxButton* m_passthrough_reset_btn;

  std::map<wxWindowID, unsigned int> m_wiimote_index_from_ctrl_id;
  std::map<wxWindowID, unsigned int> m_wiimote_index_from_conf_bt_id;
  std::array<unsigned int, MAX_WIIMOTES> m_orig_wiimote_sources;
  std::array<wxButton*, MAX_WIIMOTES> m_wiimote_conf_bt;
  std::array<wxStaticText*, MAX_WIIMOTES> m_wiimote_labels;
  std::array<wxChoice*, MAX_WIIMOTES> m_wiimote_sources;
  wxCheckBox* m_balance_board_checkbox;

  wxCheckBox* m_enable_continuous_scanning;
  wxButton* m_refresh_wm_button;
  wxStaticText* m_unsupported_bt_text;
  wxCheckBox* m_enable_speaker_data;
};
