// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Cheats/CheatsWindow.h"

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "DolphinWX/Cheats/ActionReplayCodesPanel.h"
#include "DolphinWX/Cheats/CheatSearchTab.h"
#include "DolphinWX/Cheats/GeckoCodeDiag.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"

wxDEFINE_EVENT(DOLPHIN_EVT_ADD_NEW_ACTION_REPLAY_CODE, wxCommandEvent);

struct wxCheatsWindow::CodeData : public wxClientData
{
  ActionReplay::ARCode code;
};

wxCheatsWindow::wxCheatsWindow(wxWindow* const parent)
    : wxDialog(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX |
                   wxDIALOG_NO_PARENT)
{
  // Create the GUI controls
  CreateGUI();

  // load codes
  UpdateGUI();
  wxTheApp->Bind(DOLPHIN_EVT_LOCAL_INI_CHANGED, &wxCheatsWindow::OnLocalGameIniModified, this);

  SetIcons(WxUtils::GetDolphinIconBundle());
  SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
  SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
  Center();
}

wxCheatsWindow::~wxCheatsWindow() = default;

void wxCheatsWindow::CreateGUI()
{
  const int space5 = FromDIP(5);
  const int space10 = FromDIP(10);

  // Main Notebook
  m_notebook_main = new wxNotebook(this, wxID_ANY);

  // --- Tabs ---
  // Cheats List Tab
  wxPanel* tab_cheats = new wxPanel(m_notebook_main, wxID_ANY);

  m_ar_codes_panel =
      new ActionReplayCodesPanel(tab_cheats, ActionReplayCodesPanel::STYLE_SIDE_PANEL |
                                                 ActionReplayCodesPanel::STYLE_MODIFY_BUTTONS);

  wxBoxSizer* sizer_tab_cheats = new wxBoxSizer(wxVERTICAL);
  sizer_tab_cheats->AddSpacer(space5);
  sizer_tab_cheats->Add(m_ar_codes_panel, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sizer_tab_cheats->AddSpacer(space5);

  tab_cheats->SetSizerAndFit(sizer_tab_cheats);

  // Cheat Search Tab
  m_tab_cheat_search = new CheatSearchTab(m_notebook_main);

  // Log Tab
  m_tab_log = new wxPanel(m_notebook_main, wxID_ANY);

  wxButton* const button_updatelog = new wxButton(m_tab_log, wxID_ANY, _("Update"));
  button_updatelog->Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ButtonUpdateLog_Press, this);
  wxButton* const button_clearlog = new wxButton(m_tab_log, wxID_ANY, _("Clear"));
  button_clearlog->Bind(wxEVT_BUTTON, &wxCheatsWindow::OnClearActionReplayLog, this);

  m_checkbox_log_ar = new wxCheckBox(m_tab_log, wxID_ANY, _("Enable AR Logging"));
  m_checkbox_log_ar->Bind(wxEVT_CHECKBOX,
                          &wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange, this);

  m_checkbox_log_ar->SetValue(ActionReplay::IsSelfLogging());
  m_textctrl_log = new wxTextCtrl(m_tab_log, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                  wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);

  wxBoxSizer* log_control_sizer = new wxBoxSizer(wxHORIZONTAL);
  log_control_sizer->Add(m_checkbox_log_ar, 0, wxALIGN_CENTER_VERTICAL);
  log_control_sizer->Add(button_updatelog, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space10);
  log_control_sizer->Add(button_clearlog, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space10);

  wxBoxSizer* sTabLog = new wxBoxSizer(wxVERTICAL);
  sTabLog->AddSpacer(space5);
  sTabLog->Add(log_control_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space10);
  sTabLog->AddSpacer(space5);
  sTabLog->Add(m_textctrl_log, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sTabLog->AddSpacer(space5);

  m_tab_log->SetSizerAndFit(sTabLog);

  // Gecko tab
  m_geckocode_panel = new Gecko::CodeConfigPanel(m_notebook_main);

  // Add Tabs to Notebook
  m_notebook_main->AddPage(tab_cheats, _("AR Codes"));
  m_notebook_main->AddPage(m_geckocode_panel, _("Gecko Codes"));
  m_notebook_main->AddPage(m_tab_cheat_search, _("Cheat Search"));
  m_notebook_main->AddPage(m_tab_log, _("Logging"));

  Bind(wxEVT_BUTTON, &wxCheatsWindow::OnEvent_ApplyChanges_Press, this, wxID_APPLY);
  Bind(wxEVT_CLOSE_WINDOW, &wxCheatsWindow::OnClose, this);
  Bind(DOLPHIN_EVT_ADD_NEW_ACTION_REPLAY_CODE, &wxCheatsWindow::OnNewARCodeCreated, this);

  wxStdDialogButtonSizer* const sButtons = CreateStdDialogButtonSizer(wxAPPLY | wxCANCEL);
  m_button_apply = sButtons->GetApplyButton();
  SetEscapeId(wxID_CANCEL);
  SetAffirmativeId(wxID_CANCEL);

  wxBoxSizer* const sMain = new wxBoxSizer(wxVERTICAL);
  sMain->AddSpacer(space5);
  sMain->Add(m_notebook_main, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sMain->AddSpacer(space5);
  sMain->Add(sButtons, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sMain->AddSpacer(space5);
  sMain->SetMinSize(FromDIP(wxSize(-1, 600)));
  SetSizerAndFit(sMain);
}

void wxCheatsWindow::OnClose(wxCloseEvent&)
{
  Hide();
}

// load codes for a new ISO ID
void wxCheatsWindow::UpdateGUI()
{
  // load code
  const SConfig& parameters = SConfig::GetInstance();
  m_gameini_default = parameters.LoadDefaultGameIni();
  m_gameini_local = parameters.LoadLocalGameIni();
  m_game_id = parameters.GetGameID();
  m_game_revision = parameters.GetRevision();
  m_gameini_local_path = File::GetUserPath(D_GAMESETTINGS_IDX) + m_game_id + ".ini";
  Load_ARCodes();
  Load_GeckoCodes();
  m_tab_cheat_search->UpdateGUI();

  // enable controls
  m_button_apply->Enable(Core::IsRunning());

  wxString title = _("Cheat Manager");

  // write the ISO name in the title
  if (Core::IsRunning())
    SetTitle(title + StrToWxStr(": " + m_game_id));
  else
    SetTitle(title);
}

void wxCheatsWindow::Load_ARCodes()
{
  if (!Core::IsRunning())
  {
    m_ar_codes_panel->Clear();
    m_ar_codes_panel->Disable();
    return;
  }
  else if (!m_ar_codes_panel->IsEnabled())
  {
    m_ar_codes_panel->Enable();
  }
  m_ar_codes_panel->LoadCodes(m_gameini_default, m_gameini_local);
}

void wxCheatsWindow::Load_GeckoCodes()
{
  m_geckocode_panel->LoadCodes(m_gameini_default, m_gameini_local, m_game_id, true);
}

void wxCheatsWindow::OnNewARCodeCreated(wxCommandEvent& ev)
{
  auto code = static_cast<ActionReplay::ARCode*>(ev.GetClientData());
  ActionReplay::AddCode(*code);
  m_ar_codes_panel->AppendNewCode(*code);
}

void wxCheatsWindow::OnLocalGameIniModified(wxCommandEvent& ev)
{
  ev.Skip();
  if (WxStrToStr(ev.GetString()) != m_game_id)
    return;
  if (m_ignore_ini_callback)
  {
    m_ignore_ini_callback = false;
    return;
  }
  UpdateGUI();
}

void wxCheatsWindow::OnEvent_ApplyChanges_Press(wxCommandEvent& ev)
{
  // Apply Action Replay code changes
  ActionReplay::ApplyCodes(m_ar_codes_panel->GetCodes());

  // Apply Gecko Code changes
  Gecko::SetActiveCodes(m_geckocode_panel->GetCodes());

  // Save gameini, with changed codes
  if (m_gameini_local_path.size())
  {
    m_ar_codes_panel->SaveCodes(&m_gameini_local);
    Gecko::SaveCodes(m_gameini_local, m_geckocode_panel->GetCodes());
    m_gameini_local.Save(m_gameini_local_path);

    wxCommandEvent ini_changed(DOLPHIN_EVT_LOCAL_INI_CHANGED);
    ini_changed.SetString(StrToWxStr(m_game_id));
    ini_changed.SetInt(m_game_revision);
    m_ignore_ini_callback = true;
    wxTheApp->ProcessEvent(ini_changed);
  }

  ev.Skip();
}

void wxCheatsWindow::OnEvent_ButtonUpdateLog_Press(wxCommandEvent&)
{
  wxBeginBusyCursor();
  m_textctrl_log->Freeze();
  m_textctrl_log->Clear();
  // This horrible mess is because the Windows Textbox Widget suffers from
  // a Shlemiel The Painter problem where it keeps allocating new memory each
  // time some text is appended then memcpys to the new buffer. This happens
  // for every single line resulting in the operation taking minutes instead of
  // seconds.
  // Why not just append all of the text all at once? Microsoft decided that it
  // would be clever to accept as much text as will fit in the internal buffer
  // then silently discard the rest. We have to iteratively append the text over
  // and over until the internal buffer becomes big enough to hold all of it.
  // (wxWidgets should have hidden this platform detail but it sucks)
  wxString super_string;
  super_string.reserve(1024 * 1024);
  for (const std::string& text : ActionReplay::GetSelfLog())
  {
    super_string.append(StrToWxStr(text));
  }
  while (!super_string.empty())
  {
    // Read "GetLastPosition" as "Size", there's no size function.
    wxTextPos start = m_textctrl_log->GetLastPosition();
    m_textctrl_log->AppendText(super_string);
    wxTextPos end = m_textctrl_log->GetLastPosition();
    if (start == end)
      break;
    super_string.erase(0, end - start);
  }
  m_textctrl_log->Thaw();
  wxEndBusyCursor();
}

void wxCheatsWindow::OnClearActionReplayLog(wxCommandEvent& event)
{
  ActionReplay::ClearSelfLog();
  OnEvent_ButtonUpdateLog_Press(event);
}

void wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent&)
{
  ActionReplay::EnableSelfLogging(m_checkbox_log_ar->IsChecked());
}
