// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core\Core.h"
#include "LuaScripting.h"

GCPadStatus* pad_status;

LuaThread::LuaThread(LuaScriptFrame* p, wxString file) : wxThread()
{
  parent = p;
  file_path = file;

  //Initialize virtual controller
  pad_status = (GCPadStatus*)malloc(sizeof(GCPadStatus));
  clearPad(pad_status);
}

LuaThread::~LuaThread()
{
  //Delete virtual controller
  std::unique_lock<std::mutex> lock(lua_mutex);
  free(pad_status);
  pad_status = nullptr;
  
  parent->lua_thread = nullptr;
}
  
wxThread::ExitCode LuaThread::Entry()
{
  //Pause emu
  Core::SetState(Core::State::Paused);

  lua_State* state = luaL_newstate();

  //Make standard libraries available to loaded script
  luaL_openlibs(state);

  //Register additinal functions with Lua
  std::map<const char*, LuaFunction>::iterator it;
  for (it = registered_functions->begin(); it != registered_functions->end(); it++)
  {
    lua_register(state, it->first, it->second);
  }

  if (luaL_loadfile(state, file_path) != LUA_OK)
  {
    parent->Log("Error opening file.\n");

    return nullptr;
  }

  if (lua_pcall(state, 0, LUA_MULTRET, 0) != LUA_OK)
  {
    parent->Log("Error running file.\n");

    return nullptr;
  }

  return parent;
}
