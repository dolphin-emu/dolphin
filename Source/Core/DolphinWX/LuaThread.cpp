#include "LuaScriptFrame.h"

LuaThread::LuaThread(LuaScriptFrame* p, wxString file) : wxThread()
{
  parent = p;
  file_path = file;
}

LuaThread::~LuaThread()
{
}
  
wxThread::ExitCode LuaThread::Entry()
{
  lua_State* state = luaL_newstate();

  //Make standard libraries available to loaded script
  luaL_openlibs(state);

  //Register additinal functions with Lua
  lua_register(state, "print", printToTextCtrl);

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

  parent->SignalThreadFinished();

  Exit();
  return parent;
}
