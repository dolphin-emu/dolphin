// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>

#include <wx/dialog.h>

#include "Core/HW/Wiimote.h"
#include "InputCommon/GCAdapter.h"

class InputConfig;
class wxCheckBox;
class wxChoice;
class wxRadioButton;
class wxStaticText;

class ControllerConfigDiag final : public wxDialog
{
public:
  ControllerConfigDiag(wxWindow* const parent);

private:
  void UpdateUI();

  wxSizer* CreateGamecubeSizer();
  wxSizer* CreateWiimoteConfigSizer();
  wxSizer* CreatePassthroughBTConfigSizer();
  wxSizer* CreateEmulatedBTConfigSizer();
  wxSizer* CreateAdvancedSettingsSizer();

  void OnClose(wxCloseEvent& event);
  void OnCloseButton(wxCommandEvent& event);

  void OnWiimoteSourceChanged(wxCommandEvent& event);
  void OnWiimoteConfigButton(wxCommandEvent& event);
  void OnWiimoteRefreshButton(wxCommandEvent& event);

  void OnGameCubePortChanged(wxCommandEvent& event);
  void OnGameCubeConfigButton(wxCommandEvent& event);

  void OnBluetoothModeChanged(wxCommandEvent& event);

  void OnPassthroughScanButton(wxCommandEvent& event);
  void OnPassthroughResetButton(wxCommandEvent& event);
  void OnBalanceBoardChanged(wxCommandEvent& event);
  void OnContinuousScanning(wxCommandEvent& event);
  void OnEnableSpeaker(wxCommandEvent& event);

  void OnBackgroundInputChanged(wxCommandEvent& event);

  std::map<wxWindowID, unsigned int> m_gc_port_from_choice_id;
  std::map<wxWindowID, unsigned int> m_gc_port_from_config_id;
  std::array<wxButton*, 4> m_gc_port_configure_button;
  std::array<wxString, 8> m_gc_pad_type_strs;

  wxRadioButton* m_passthrough_bt_radio;
  wxRadioButton* m_emulated_bt_radio;

  wxStaticText* m_passthrough_sync_text;
  wxButton* m_passthrough_sync_btn;
  wxStaticText* m_passthrough_reset_text;
  wxButton* m_passthrough_reset_btn;

  std::map<wxWindowID, unsigned int> m_wiimote_index_from_choice_id;
  std::map<wxWindowID, unsigned int> m_wiimote_index_from_config_id;
  std::array<wxButton*, MAX_WIIMOTES> m_wiimote_configure_button;
  std::array<wxStaticText*, MAX_WIIMOTES> m_wiimote_labels;
  std::array<wxChoice*, MAX_WIIMOTES> m_wiimote_sources;
  wxCheckBox* m_balance_board_checkbox;

  wxCheckBox* m_enable_continuous_scanning;
  wxButton* m_refresh_wm_button;
  wxStaticText* m_unsupported_bt_text;
  wxCheckBox* m_enable_speaker_data;

  wxCheckBox* m_background_input_checkbox;
};
