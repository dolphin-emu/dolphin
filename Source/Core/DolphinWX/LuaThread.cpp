// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <lua5.3\include\lua.hpp>

#include "Core\Core.h"
#include "DolphinWX\LuaScripting.h"
#include "InputCommon\GCPadStatus.h"

namespace Lua
{
std::unique_ptr<GCPadStatus> pad_status;
std::mutex lua_mutex;

LuaThread::LuaThread(LuaScriptFrame* p, const wxString& file)
    : m_parent(p), m_file_path(file), wxThread()
{
  // Initialize virtual controller
  pad_status = std::make_unique<GCPadStatus>();

  // Zero out controller
  pad_status->button = 0;
  pad_status->stickX = GCPadStatus::MAIN_STICK_CENTER_X;
  pad_status->stickY = GCPadStatus::MAIN_STICK_CENTER_Y;
  pad_status->triggerLeft = 0;
  pad_status->triggerRight = 0;
  pad_status->substickX = GCPadStatus::C_STICK_CENTER_X;
  pad_status->substickY = GCPadStatus::C_STICK_CENTER_Y;
}

LuaThread::~LuaThread()
{
  // Delete virtual controller
  std::unique_lock<std::mutex> lock(lua_mutex);
  pad_status = nullptr;

  m_parent->NullifyLuaThread();
}

wxThread::ExitCode LuaThread::Entry()
{
  // Pause emu
  Core::SetState(Core::State::Paused);

  //lua_State* state = luaL_newstate();
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

    return nullptr;
  }

  if (lua_pcall(state.get(), 0, LUA_MULTRET, 0) != LUA_OK)
  {
    m_parent->Log(lua_tostring(state.get(), 1));

    return nullptr;
  }

  return (wxThread::ExitCode)0;
}
}  // namespace Lua
