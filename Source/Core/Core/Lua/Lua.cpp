#include "Lua.h"

#include "LuaFunctions/LuaMemoryApi.h"
#include "LuaFunctions/LuaEmuFunctions.h"
#include "LuaFunctions/LuaBitFunctions.h"
#include "LuaFunctions/LuaGameCubeController.h"
#include "LuaFunctions/LuaRegisters.h"
#include "LuaFunctions/LuaStatistics.h"
#include "Core/Movie.h"
#include <filesystem>

namespace Lua
{
lua_State* mainLuaState = NULL;
lua_State* mainLuaThreadState = NULL;
bool luaScriptActive = false;
int* x = NULL;
bool luaInitialized = false;
std::function<void(const std::string&)>* printCallbackFunction = NULL;
std::function<void()>* scriptEndCallbackFunction = NULL;


int custom_print_function(lua_State* luaState)
{
  int nargs = lua_gettop(luaState);
  std::string outputString;
  for (int i = 1; i <= nargs; i++)
  {
    if (lua_isstring(luaState, i))
    {
      outputString.append(lua_tostring(luaState, i));
      /* Pop the next arg using lua_tostring(L, i) and do your print */
    }
    else if (lua_isinteger(luaState, i))
    {
      outputString.append(std::to_string(lua_tointeger(luaState, i)));
    }
    else if (lua_isnumber(luaState, i))
    {
      outputString.append(std::to_string(lua_tonumber(luaState, i)));
    }
    else if (lua_isboolean(luaState, i))
    {
      outputString.append(lua_toboolean(luaState, i) ? "true" : "false");
    }
    else if (lua_isnil(luaState, i))
    {
      outputString.append("nil");
    }
    else
    {
      luaL_error(luaState, "Error: Unknown type encountered in print function. Supported types are String, Integer, Number, Boolean, and nil");
    }
  }

  (*printCallbackFunction)(outputString);

  return 0;
}

void Init(const std::string& scriptLocation,
          std::function<void(const std::string&)>* newPrintCallback, std::function<void()>* newScriptEndCallback)
{
  printCallbackFunction = newPrintCallback;
  scriptEndCallbackFunction = newScriptEndCallback;
  std::srand(time(NULL));
  x = new int;
  *x = 0;
  luaScriptActive = true;
  luaInitialized = true;
  mainLuaState = luaL_newstate();
  luaL_openlibs(mainLuaState);
  LuaMemoryApi::InitLuaMemoryApi(mainLuaState);
  LuaEmu::InitLuaEmuFunctions(mainLuaState);
  LuaBit::InitLuaBitFunctions(mainLuaState);
  LuaGameCubeController::InitLuaGameCubeControllerFunctions(mainLuaState);
  LuaStatistics::InitLuaStatisticsFunctions(mainLuaState);
  LuaRegisters::InitLuaRegistersFunctions(mainLuaState);
  lua_gc(mainLuaState, LUA_GCSTOP);
  lua_newtable(mainLuaState);
  lua_pushcfunction(mainLuaState, custom_print_function);
  lua_setglobal(mainLuaState, "print");

  mainLuaThreadState = lua_newthread(mainLuaState);
  if (luaL_loadfile(mainLuaThreadState, scriptLocation.c_str()) != LUA_OK)
  {
    const char* tempString = lua_tostring(mainLuaThreadState, -1);
    (*printCallbackFunction)(tempString);
    (*scriptEndCallbackFunction)();
  }
  int retVal = lua_resume(Lua::mainLuaThreadState, NULL, 0, Lua::x);
  if (retVal != LUA_YIELD)
  {
    if (retVal == 2)
    {
      const char* errorMsg = lua_tostring(Lua::mainLuaThreadState, -1);
      (*Lua::printCallbackFunction)(errorMsg);
    }
    (*scriptEndCallbackFunction)();
    Lua::luaScriptActive = false;
  }

  return;
}

void StopScript()
{
  luaScriptActive = false;
}

}
