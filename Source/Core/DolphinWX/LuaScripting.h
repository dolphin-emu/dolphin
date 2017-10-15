// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <wx/frame.h>

// Forward Declarations
class wxString;
class wxFileDialog;
class wxTextCtrl;
class wxStaticText;
class wxMenuItem;
class wxMenuBar;
class wxMenu;
class wxButton;
struct GCPadStatus;
struct lua_State;
namespace std
{
class mutex;
}

namespace Lua
{
using LuaFunction = int (*)(lua_State* L);

extern std::map<const char*, LuaFunction>* registered_functions;

// pad_status is shared between the window thread and the script executing thread.
// so access to it must be mutex protected.
extern GCPadStatus* pad_status;
extern std::mutex lua_mutex;

void ClearPad(GCPadStatus*);

class LuaScriptFrame;

class LuaThread : public wxThread
{
public:
  LuaThread(LuaScriptFrame* p, wxString file);
  ~LuaThread();

  wxThread::ExitCode Entry();

private:
  LuaScriptFrame* parent = nullptr;
  wxString file_path;
};

class LuaScriptFrame final : public wxFrame
{
public:
  void Log(const char* message);
  void GetValues(GCPadStatus* status);
  void NullifyLuaThread();

  LuaScriptFrame(wxWindow* parent);

  ~LuaScriptFrame();

private:
  void CreateGUI();
  void OnClearClicked(wxCommandEvent& event);
  void OnDocumentationClicked(wxCommandEvent& event);
  void OnAPIClicked(wxCommandEvent& event);
  void BrowseOnButtonClick(wxCommandEvent& event);
  void RunOnButtonClick(wxCommandEvent& event);
  void StopOnButtonClick(wxCommandEvent& event);
  wxMenuBar* m_menubar;
  wxMenuItem* clear;
  wxMenuItem* documentation;
  wxMenuItem* api;
  wxMenu* console_menu;
  wxMenu* help_menu;
  wxStaticText* script_file_label;
  wxTextCtrl* file_path;
  wxButton* browse_button;
  wxButton* run_button;
  wxButton* stop_button;
  wxStaticText* output_console_literal;
  wxTextCtrl* output_console;
  LuaThread* lua_thread;
};
}  // namespace Lua
