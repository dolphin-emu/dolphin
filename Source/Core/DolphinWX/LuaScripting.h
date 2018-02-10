// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <mutex>
#include <wx/frame.h>

#include "InputCommon/GCPadStatus.h"

// Forward Declarations
class wxString;
class wxFileDialog;
class wxTextCtrl;
class wxStaticText;
class wxMenuItem;
class wxMenuBar;
class wxMenu;
class wxButton;
struct lua_State;
struct lua_Debug;
struct luaL_Reg;

namespace Lua
{
using LuaFunction = int (*)(lua_State* L);

class LuaScriptFrame;

extern std::map<const char*, LuaFunction> m_registered_functions;

void HookFunction(lua_State*, lua_Debug*);

class LuaThread final : public wxThread
{
public:
  LuaThread(LuaScriptFrame* p, const wxString& file, std::map<const char*, luaL_Reg*> libs);
  ~LuaThread();
  void GetValues(GCPadStatus* status);
  bool m_destruction_flag = false;
  GCPadStatus m_pad_status;
  GCPadStatus m_last_pad_status;
private:
  LuaScriptFrame* m_parent = nullptr;
  wxString m_file_path;
  wxThread::ExitCode Entry() override;
  std::map<const char*, luaL_Reg*> m_libs;
};

class LuaScriptFrame final : public wxFrame
{
public:
  LuaScriptFrame(wxWindow* parent);
  ~LuaScriptFrame();

  void Log(const wxString& message);
  void NullifyLuaThread();
  GCPadStatus& GetPadStatus();
  GCPadStatus GetLastPadStatus();
  LuaThread* GetLuaThread();
  static LuaScriptFrame* GetCurrentInstance();

private:
  void CreateGUI();
  void CreateMenuBar();
  void OnClearClicked(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);
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
  static LuaScriptFrame* m_current_instance;
  std::map<const char*, luaL_Reg*> m_libs;
};
}  // namespace Lua
