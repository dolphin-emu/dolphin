// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <wx/dialog.h>
#include <wx/panel.h>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

class CheatSearchTab;
class wxButton;
class wxCheckBox;
class wxNotebook;
class wxTextCtrl;

namespace Gecko
{
class CodeConfigPanel;
}
class ActionReplayCodesPanel;

wxDECLARE_EVENT(DOLPHIN_EVT_ADD_NEW_ACTION_REPLAY_CODE, wxCommandEvent);

class wxCheatsWindow final : public wxDialog
{
public:
  wxCheatsWindow(wxWindow* const parent);
  ~wxCheatsWindow();
  void UpdateGUI();

private:
  struct CodeData;

  // --- GUI Controls ---
  wxButton* m_button_apply;
  wxNotebook* m_notebook_main;

  CheatSearchTab* m_tab_cheat_search;
  wxPanel* m_tab_log;

  wxCheckBox* m_checkbox_log_ar;

  wxTextCtrl* m_textctrl_log;

  ActionReplayCodesPanel* m_ar_codes_panel;
  Gecko::CodeConfigPanel* m_geckocode_panel;
  IniFile m_gameini_default;
  IniFile m_gameini_local;
  std::string m_gameini_local_path;
  std::string m_game_id;
  u32 m_game_revision;

  bool m_ignore_ini_callback = false;

  void CreateGUI();

  void Load_ARCodes();
  void Load_GeckoCodes();

  // --- Wx Events Handlers ---
  // Cheat Search
  void OnNewARCodeCreated(wxCommandEvent& ev);

  // Dialog close event
  void OnClose(wxCloseEvent&);

  // Changes to the INI (affects cheat listings)
  void OnLocalGameIniModified(wxCommandEvent& event);

  // Apply Changes Button
  void OnEvent_ApplyChanges_Press(wxCommandEvent& event);

  // Update Log Button
  void OnEvent_ButtonUpdateLog_Press(wxCommandEvent& event);
  void OnClearActionReplayLog(wxCommandEvent& event);

  // Enable Logging Checkbox
  void OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent& event);
};
