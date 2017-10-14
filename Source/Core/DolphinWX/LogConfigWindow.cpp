// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/validate.h>

#include "Common/FileUtil.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "DolphinWX/LogConfigWindow.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/WxUtils.h"

LogConfigWindow::LogConfigWindow(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL,
              _("Log Configuration")),
      enableAll(true)
{
  m_LogManager = LogManager::GetInstance();
  CreateGUIControls();
  LoadSettings();
}

LogConfigWindow::~LogConfigWindow()
{
  SaveSettings();
}

void LogConfigWindow::CreateGUIControls()
{
  // Verbosity
  wxArrayString wxLevels, wxLevelsUse;
  wxLevels.Add(_("Notice"));
  wxLevels.Add(_("Error"));
  wxLevels.Add(_("Warning"));
  wxLevels.Add(_("Info"));
  wxLevels.Add(_("Debug"));
  for (int i = 0; i < MAX_LOGLEVEL; ++i)
    wxLevelsUse.Add(wxLevels[i]);
  m_verbosity = new wxRadioBox(this, wxID_ANY, _("Verbosity"), wxDefaultPosition, wxDefaultSize,
                               wxLevelsUse, 0, wxRA_SPECIFY_ROWS);
  m_verbosity->Bind(wxEVT_RADIOBOX, &LogConfigWindow::OnVerbosityChange, this);

  // Options
  m_writeFileCB = new wxCheckBox(this, wxID_ANY, _("Write to File"));
  m_writeFileCB->Bind(wxEVT_CHECKBOX, &LogConfigWindow::OnWriteFileChecked, this);
  m_writeConsoleCB = new wxCheckBox(this, wxID_ANY, _("Write to Console"));
  m_writeConsoleCB->Bind(wxEVT_CHECKBOX, &LogConfigWindow::OnWriteConsoleChecked, this);
  m_writeWindowCB = new wxCheckBox(this, wxID_ANY, _("Write to Window"));
  m_writeWindowCB->Bind(wxEVT_CHECKBOX, &LogConfigWindow::OnWriteWindowChecked, this);

  wxButton* btn_toggle_all = new wxButton(this, wxID_ANY, _("Toggle All Log Types"),
                                          wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  btn_toggle_all->Bind(wxEVT_BUTTON, &LogConfigWindow::OnToggleAll, this);
  m_checks = new wxCheckListBox(this, wxID_ANY);
  m_checks->Bind(wxEVT_CHECKLISTBOX, &LogConfigWindow::OnLogCheck, this);
  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
    m_checks->Append(StrToWxStr(m_LogManager->GetFullName((LogTypes::LOG_TYPE)i)));

  const int space1 = FromDIP(1);
  const int space5 = FromDIP(5);

  // Sizers
  wxStaticBoxSizer* sbOutputs = new wxStaticBoxSizer(wxVERTICAL, this, _("Logger Outputs"));
  sbOutputs->Add(m_writeFileCB, 0);
  sbOutputs->Add(m_writeConsoleCB, 0, wxTOP, space1);
  sbOutputs->Add(m_writeWindowCB, 0, wxTOP, space1);

  wxStaticBoxSizer* sbLogTypes = new wxStaticBoxSizer(wxVERTICAL, this, _("Log Types"));
  sbLogTypes->Add(m_checks, 1, wxEXPAND);

  wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
  sMain->Add(m_verbosity, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sMain->Add(sbOutputs, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sMain->Add(btn_toggle_all, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sMain->Add(sbLogTypes, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);

  sMain->SetMinSize(FromDIP(wxSize(100, 100)));
  SetSizerAndFit(sMain);
}

void LogConfigWindow::LoadSettings()
{
  m_verbosity->SetSelection(m_LogManager->GetLogLevel() - 1);

  // Get the logger output settings from the config ini file.
  m_writeFileCB->SetValue(m_LogManager->IsListenerEnabled(LogListener::FILE_LISTENER));
  m_writeConsoleCB->SetValue(m_LogManager->IsListenerEnabled(LogListener::CONSOLE_LISTENER));
  m_writeWindowCB->SetValue(m_LogManager->IsListenerEnabled(LogListener::LOG_WINDOW_LISTENER));

  // Run through all of the log types and check each checkbox for each logging type
  // depending on its set value within the config ini.
  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
  {
    bool log_enabled = m_LogManager->IsEnabled(static_cast<LogTypes::LOG_TYPE>(i));

    if (log_enabled)
      enableAll = false;

    m_checks->Check(i, log_enabled);
  }
}

void LogConfigWindow::SaveSettings()
{
  m_LogManager->SaveSettings();
}

// If the verbosity changes while logging
void LogConfigWindow::OnVerbosityChange(wxCommandEvent& event)
{
  m_LogManager->SetLogLevel(static_cast<LogTypes::LOG_LEVELS>(m_verbosity->GetSelection() + 1));

  event.Skip();
}

void LogConfigWindow::OnWriteFileChecked(wxCommandEvent& event)
{
  m_LogManager->EnableListener(LogListener::FILE_LISTENER, event.IsChecked());
}

void LogConfigWindow::OnWriteConsoleChecked(wxCommandEvent& event)
{
  m_LogManager->EnableListener(LogListener::CONSOLE_LISTENER, event.IsChecked());
}

void LogConfigWindow::OnWriteWindowChecked(wxCommandEvent& event)
{
  m_LogManager->EnableListener(LogListener::LOG_WINDOW_LISTENER, event.IsChecked());
}

void LogConfigWindow::OnToggleAll(wxCommandEvent& WXUNUSED(event))
{
  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
    ToggleLog(i, enableAll);

  enableAll = !enableAll;
}

void LogConfigWindow::ToggleLog(int _logType, bool enable)
{
  LogTypes::LOG_TYPE logType = (LogTypes::LOG_TYPE)_logType;
  m_checks->Check(_logType, enable);
  m_LogManager->SetEnable(logType, enable);
}

void LogConfigWindow::OnLogCheck(wxCommandEvent& event)
{
  int i = event.GetInt();
  ToggleLog(i, m_checks->IsChecked(i));
}
