// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>

class LogManager;
class wxCheckBox;
class wxCheckListBox;
class wxRadioBox;

class LogConfigWindow : public wxPanel
{
public:
  LogConfigWindow(wxWindow* parent, wxWindowID id = wxID_ANY);
  ~LogConfigWindow();

  void SaveSettings();
  void LoadSettings();

private:
  LogManager* m_LogManager;
  bool enableAll;

  // Controls
  wxCheckBox *m_writeFileCB, *m_writeConsoleCB, *m_writeWindowCB;
  wxCheckListBox* m_checks;
  wxRadioBox* m_verbosity;

  void CreateGUIControls();
  void OnVerbosityChange(wxCommandEvent& event);
  void OnWriteFileChecked(wxCommandEvent& event);
  void OnWriteConsoleChecked(wxCommandEvent& event);
  void OnWriteWindowChecked(wxCommandEvent& event);
  void OnToggleAll(wxCommandEvent& event);
  void ToggleLog(int _logType, bool enable);
  void OnLogCheck(wxCommandEvent& event);
};
