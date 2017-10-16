// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <lua5.3\include\lua.hpp>
#include "Core\Core.h"
#include "InputCommon\GCPadStatus.h"
#include "DolphinWX\LuaScripting.h"

namespace Lua
{
GCPadStatus* pad_status;

LuaThread::LuaThread(LuaScriptFrame* p, const wxString& file) : m_parent(p), m_file_path(file),  wxThread()
{
  // Initialize virtual controller
  //std::unique_ptr<GCPadStatus>
  pad_status = (GCPadStatus*)malloc(sizeof(GCPadStatus));
  ClearPad(pad_status);
}

LuaThread::~LuaThread()
{
  // Delete virtual controller
  std::unique_lock<std::mutex> lock(lua_mutex);
  free(pad_status);
  pad_status = nullptr;

  m_parent->NullifyLuaThread();
}

wxThread::ExitCode LuaThread::Entry()
{
  // Pause emu
  Core::SetState(Core::State::Paused);

  lua_State* state = luaL_newstate();

  // Make standard libraries available to loaded script
  luaL_openlibs(state);

  // Register additinal functions with Lua
  std::map<const char*, LuaFunction>::iterator it;
  for (it = m_parent->m_registered_functions->begin(); it != m_parent->m_registered_functions->end(); it++)
  {
    lua_register(state, it->first, it->second);
  }

  if (luaL_loadfile(state, m_file_path) != LUA_OK)
  {
    m_parent->Log("Error opening file.\n");

    return nullptr;
  }

  if (lua_pcall(state, 0, LUA_MULTRET, 0) != LUA_OK)
  {
    m_parent->Log(lua_tostring(state, 1));

    return nullptr;
  }

  return m_parent;
}
}  // namespace Lua
