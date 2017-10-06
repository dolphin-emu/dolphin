// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "LuaScripting.h"

LuaThread::LuaThread(LuaScriptFrame* p, wxString file) : wxThread()
{
  parent = p;
  file_path = file;
}

LuaThread::~LuaThread()
{
  parent->SignalThreadFinished();
}
  
wxThread::ExitCode LuaThread::Entry()
{
  lua_State* state = luaL_newstate();

  //Make standard libraries available to loaded script
  luaL_openlibs(state);

  //Register additinal functions with Lua
  lua_register(state, "print", printToTextCtrl);
  lua_register(state, "getAnalog", getAnalogCoordinates);

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
