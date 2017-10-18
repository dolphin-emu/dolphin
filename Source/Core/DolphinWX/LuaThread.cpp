// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <lua5.3\include\lua.hpp>

#include "Core\Core.h"
#include "DolphinWX\LuaScripting.h"
#include "InputCommon\GCPadStatus.h"

namespace Lua
{
GCPadStatus LuaThread::m_pad_status;
std::mutex LuaThread::m_lua_mutex;

LuaThread::LuaThread(LuaScriptFrame* p, const wxString& file)
    : m_parent(p), m_file_path(file), wxThread()
{
  // Zero out controller
  LuaThread::GetPadStatus()->button = 0;
  LuaThread::GetPadStatus()->stickX = GCPadStatus::MAIN_STICK_CENTER_X;
  LuaThread::GetPadStatus()->stickY = GCPadStatus::MAIN_STICK_CENTER_Y;
  LuaThread::GetPadStatus()->triggerLeft = 0;
  LuaThread::GetPadStatus()->triggerRight = 0;
  LuaThread::GetPadStatus()->substickX = GCPadStatus::C_STICK_CENTER_X;
  LuaThread::GetPadStatus()->substickY = GCPadStatus::C_STICK_CENTER_Y;
}

LuaThread::~LuaThread()
{
  // Lock mutex so that this thread can't be deleted during LuaScriptFrame::GetValues()
  std::unique_lock<std::mutex> lock(m_lua_mutex);

  m_parent->NullifyLuaThread();
}

wxThread::ExitCode LuaThread::Entry()
{
  // Pause emu
  Core::SetState(Core::State::Paused);

  std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), lua_close);

  // Make standard libraries available to loaded script
  luaL_openlibs(state.get());

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

  if (lua_pcall(state.get(), 0, LUA_MULTRET, 0) != LUA_OK)
  {
    m_parent->Log(lua_tostring(state.get(), 1));

    return reinterpret_cast<wxThread::ExitCode>(-1);
  }

  return reinterpret_cast<wxThread::ExitCode>(0);
}

GCPadStatus* LuaThread::GetPadStatus()
{
  return &m_pad_status;
}

std::mutex* LuaThread::GetMutex()
{
  return &m_lua_mutex;
}
}  // namespace Lua
