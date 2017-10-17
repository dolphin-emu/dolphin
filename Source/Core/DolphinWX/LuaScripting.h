// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <mutex>
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

namespace Lua
{
using LuaFunction = int (*)(lua_State* L);

class LuaScriptFrame;

extern std::map<const char*, LuaFunction> m_registered_functions;

class LuaThread final : public wxThread
{
public:
  LuaThread(LuaScriptFrame* p, const wxString& file);
  ~LuaThread();
  static GCPadStatus* GetPadStatus();
  static std::mutex* GetMutex();

private:
  LuaScriptFrame* m_parent = nullptr;
  wxString m_file_path;
  wxThread::ExitCode Entry() override;
  static GCPadStatus m_pad_status;
  static std::mutex m_lua_mutex;
};

class LuaScriptFrame final : public wxFrame
{
public:
  LuaScriptFrame(wxWindow* parent);
  ~LuaScriptFrame();

  void Log(const wxString& message);
  void GetValues(GCPadStatus* status);
  void NullifyLuaThread();

private:
  void CreateGUI();
  void CreateMenuBar();
  void OnClearClicked(wxCommandEvent& event);
  void OnDocumentationClicked(wxCommandEvent& event);
  void OnAPIClicked(wxCommandEvent& event);
  void BrowseOnButtonClick(wxCommandEvent& event);
  void RunOnButtonClick(wxCommandEvent& event);
  void StopOnButtonClick(wxCommandEvent& event);
  wxMenuBar* m_menubar;
  wxMenuItem* m_clear;
  wxMenuItem* m_documentation;
  wxMenuItem* m_api;
  wxMenu* m_console_menu;
  wxMenu* m_help_menu;
  wxStaticText* m_script_file_label;
  wxTextCtrl* m_file_path;
  wxButton* m_browse_button;
  wxButton* m_run_button;
  wxButton* m_stop_button;
  wxStaticText* m_output_console_literal;
  wxTextCtrl* m_output_console;
  LuaThread* m_lua_thread;
};
}  // namespace Lua
