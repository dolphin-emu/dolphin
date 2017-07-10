// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <mutex>
#include <queue>
#include <utility>
#include <vector>
#include <wx/font.h>
#include <wx/panel.h>
#include <wx/timer.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/LogManager.h"

class CFrame;
class wxBoxSizer;
class wxCheckBox;
class wxChoice;
class wxTextCtrl;

// Uses multiple inheritance - only sane because LogListener is a pure virtual interface.
class CLogWindow : public wxPanel, LogListener
{
public:
  CLogWindow(CFrame* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL,
             const wxString& name = _("Log"));
  ~CLogWindow() override;

  // Listeners must be removed explicitly before the window is closed to prevent crashes on OS X
  // when closing via the Dock. (The Core is probably being shutdown before the window)
  void RemoveAllListeners();
  void SaveSettings();
  void Log(LogTypes::LOG_LEVELS, const char* text) override;

  int x, y, winpos;

private:
  CFrame* Parent;
  wxFont DefaultFont, MonoSpaceFont;
  std::vector<wxFont> LogFont;
  wxTimer m_LogTimer;
  LogManager* m_LogManager;
  std::queue<std::pair<u8, wxString>> msgQueue;
  bool m_LogAccess;

  // Controls
  wxBoxSizer* sBottom;
  wxTextCtrl* m_Log;
  wxTextCtrl* m_cmdline;
  wxChoice* m_FontChoice;
  wxCheckBox* m_WrapLine;
  wxButton* m_clear_log_btn;

  std::mutex m_LogSection;

  wxTextCtrl* CreateTextCtrl(wxPanel* parent, wxWindowID id, long Style);
  void CreateGUIControls();
  void PopulateBottom();
  void UnPopulateBottom();
  void OnFontChange(wxCommandEvent& event);
  void OnWrapLineCheck(wxCommandEvent& event);
  void OnClear(wxCommandEvent& event);
  void OnLogTimer(wxTimerEvent& WXUNUSED(event));
  void UpdateLog();
};
