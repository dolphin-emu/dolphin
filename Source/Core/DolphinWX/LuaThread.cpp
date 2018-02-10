// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <lua5.3/include/lua.hpp>

#include "Core/Core.h"
#include "DolphinWX/LuaScripting.h"
#include "Core/Movie.h"

namespace Lua
{

LuaThread::LuaThread(LuaScriptFrame* p, const wxString& file, luaL_Reg** libs)
    : m_parent(p), m_file_path(file), m_libs(libs), wxThread()
{
  // Zero out controller
  m_pad_status.button = 0;
  m_pad_status.stickX = GCPadStatus::MAIN_STICK_CENTER_X;
  m_pad_status.stickY = GCPadStatus::MAIN_STICK_CENTER_Y;
  m_pad_status.triggerLeft = 0;
  m_pad_status.triggerRight = 0;
  m_pad_status.substickX = GCPadStatus::C_STICK_CENTER_X;
  m_pad_status.substickY = GCPadStatus::C_STICK_CENTER_Y;

  // Register GetValues()
  Movie::SetGCInputManip([this](GCPadStatus* status, int number)
  {
    GetValues(status);
  }, Movie::GCManipIndex::LuaGCManip);
}

LuaThread::~LuaThread()
{
  // Nullify GC manipulator function to prevent crash when lua console is closed
  Movie::SetGCInputManip(nullptr, Movie::GCManipIndex::LuaGCManip);

  m_parent->NullifyLuaThread();
}

wxThread::ExitCode LuaThread::Entry()
{
  std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), lua_close);

  // Register
  lua_sethook(state.get(), &HookFunction, LUA_MASKLINE, 0);

  // Make standard libraries available to loaded script
  luaL_openlibs(state.get());

  //Load custom libraries
  luaL_newmetatable(state.get(), "bit");
  luaL_setfuncs(state.get(), m_libs[0], 0); //bit

  // Register additinal functions with Lua
  for (const auto& entry : m_registered_functions)
  {
    lua_register(state.get(), entry.first, entry.second);
  }

  if (luaL_loadfile(state.get(), m_file_path) != LUA_OK)
  {
    m_parent->Log("Error opening file.\n");

    return reinterpret_cast<wxThread::ExitCode>(-1);
  }

  // Pause emu
  Core::SetState(Core::State::Paused);

  if (lua_pcall(state.get(), 0, LUA_MULTRET, 0) != LUA_OK)
  {
    m_parent->Log(lua_tostring(state.get(), 1));

    return reinterpret_cast<wxThread::ExitCode>(-1);
  }
  Exit();
  return reinterpret_cast<wxThread::ExitCode>(0);
}

void LuaThread::GetValues(GCPadStatus* status)
{
  
  if (LuaThread::m_pad_status.stickX != GCPadStatus::MAIN_STICK_CENTER_X)
    status->stickX = LuaThread::m_pad_status.stickX;

  if (LuaThread::m_pad_status.stickY != GCPadStatus::MAIN_STICK_CENTER_Y)
    status->stickY = LuaThread::m_pad_status.stickY;

  if (LuaThread::m_pad_status.triggerLeft != 0)
    status->triggerLeft = LuaThread::m_pad_status.triggerLeft;

  if (LuaThread::m_pad_status.triggerRight != 0)
    status->triggerRight = LuaThread::m_pad_status.triggerRight;

  if (LuaThread::m_pad_status.substickX != GCPadStatus::C_STICK_CENTER_X)
    status->substickX = LuaThread::m_pad_status.substickX;

  if (LuaThread::m_pad_status.substickY != GCPadStatus::C_STICK_CENTER_Y)
    status->substickY = LuaThread::m_pad_status.substickY;

  status->button |= LuaThread::m_pad_status.button;

  //Update internal gamepad representation with the same struct we're sending out
  m_last_pad_status = *status;
}

void HookFunction(lua_State* L, lua_Debug* ar)
{
  if (LuaScriptFrame::GetCurrentInstance()->GetLuaThread()->m_destruction_flag)
  {
    luaL_error(L, "Script exited.\n");
  }
}

}  // namespace Lua
