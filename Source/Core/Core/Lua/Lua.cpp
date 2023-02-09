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
std::string GLOBAL_LUA_API_VERSION_VARIABLE = "1.0.0";
lua_State* mainLuaState = nullptr;
lua_State* mainLuaThreadState = nullptr;
bool luaScriptActive = false;
int* x = nullptr;
bool luaInitialized = false;
std::function<void(const std::string&)>* printCallbackFunction = nullptr;
std::function<void()>* scriptEndCallbackFunction = nullptr;

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

int setLuaCoreVersion(lua_State* luaState)
{
  std::string newVersionNumber = luaL_checkstring(luaState, 1);
  if (newVersionNumber.length() == 0)
    luaL_error(luaState, "Error: string was not passed to setLuaCoreVersion() function. Function "
                          "should be called as in the following example: setLuaCoreVersion(\"3.4.2\")");
  if (newVersionNumber[0] == '.')
    newVersionNumber = "0" + newVersionNumber;

  size_t newVersionNumberLength = newVersionNumber.length();
  size_t i = 0;

  while (i < newVersionNumberLength)
  {
    if (!isdigit(newVersionNumber[i]))
    {
      if (newVersionNumber[i] == '.' && i != newVersionNumberLength - 1 && isdigit(newVersionNumber[i + 1]))
        i += 2;
      else
        luaL_error(luaState,"Error: invalid format for version number passed into setLuaCoreVersion() function. " "Function should be called as in the following example: setLuaCoreVersion(\"3.4.2\")");
    }
    else
      ++i;
  }

  if (std::atof(newVersionNumber.c_str()) < 1.0f)
    luaL_error(luaState, "Error: value of argument passed to setLuaCoreVersion() function was less "
                         "than 1.0 (1.0 is the earliest version of the API that was released.");

  GLOBAL_LUA_API_VERSION_VARIABLE = newVersionNumber;
  return 0;
  // Need to stop the current Lua script and start another script after this for the change in version number to take effect.
}

int getLuaCoreVersion(lua_State* luaState)
{
  lua_pushstring(luaState, GLOBAL_LUA_API_VERSION_VARIABLE.c_str());
  return 1;
}

void Init(const std::string& scriptLocation,
          std::function<void(const std::string&)>* newPrintCallback, std::function<void()>* newScriptEndCallback)
{
  printCallbackFunction = newPrintCallback;
  scriptEndCallbackFunction = newScriptEndCallback;
  x = new int;
  *x = 0;
  luaScriptActive = true;
  luaInitialized = true;
  mainLuaState = luaL_newstate();
  luaL_openlibs(mainLuaState);
  LuaMemoryApi::InitLuaMemoryApi(mainLuaState, GLOBAL_LUA_API_VERSION_VARIABLE);
  LuaEmu::InitLuaEmuFunctions(mainLuaState, GLOBAL_LUA_API_VERSION_VARIABLE);
  LuaBit::InitLuaBitFunctions(mainLuaState, GLOBAL_LUA_API_VERSION_VARIABLE);
  LuaGameCubeController::InitLuaGameCubeControllerFunctions(mainLuaState, GLOBAL_LUA_API_VERSION_VARIABLE);
  LuaStatistics::InitLuaStatisticsFunctions(mainLuaState, GLOBAL_LUA_API_VERSION_VARIABLE);
  LuaRegisters::InitLuaRegistersFunctions(mainLuaState, GLOBAL_LUA_API_VERSION_VARIABLE);
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
  int retVal = lua_resume(Lua::mainLuaThreadState, nullptr, 0, Lua::x);
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
