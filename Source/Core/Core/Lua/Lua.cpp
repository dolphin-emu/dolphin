#include "Core/Lua/Lua.h"

#include <filesystem>
#include "Core/Lua/LuaEventCallbackClasses/LuaOnFrameStartCallbackClass.h"
#include "Core/Lua/LuaFunctions/LuaBitFunctions.h"
#include "Core/Lua/LuaFunctions/LuaEmuFunctions.h"
#include "Core/Lua/LuaFunctions/LuaGameCubeController.h"
#include "Core/Lua/LuaFunctions/LuaMemoryApi.h"
#include "Core/Lua/LuaFunctions/LuaRegisters.h"
#include "Core/Lua/LuaFunctions/LuaStatistics.h"
#include "Core/Movie.h"

namespace Lua
{
std::string global_lua_api_version = "1.0.0";
lua_State* main_lua_state = nullptr;
lua_State* main_lua_thread_state = nullptr;
int x = 0;
bool is_lua_script_active = false;
bool is_lua_core_initialized = false;
std::function<void(const std::string&)>* print_callback_function = nullptr;
std::function<void()>* script_end_callback_function = nullptr;

int CustomPrintFunction(lua_State* lua_state)
{
  int nargs = lua_gettop(lua_state);
  std::string output_string;
  for (int i = 1; i <= nargs; i++)
  {
    if (lua_isstring(lua_state, i))
    {
      output_string.append(lua_tostring(lua_state, i));
      /* Pop the next arg using lua_tostring(L, i) and do your print */
    }
    else if (lua_isinteger(lua_state, i))
    {
      output_string.append(std::to_string(lua_tointeger(lua_state, i)));
    }
    else if (lua_isnumber(lua_state, i))
    {
      output_string.append(std::to_string(lua_tonumber(lua_state, i)));
    }
    else if (lua_isboolean(lua_state, i))
    {
      output_string.append(lua_toboolean(lua_state, i) ? "true" : "false");
    }
    else if (lua_isnil(lua_state, i))
    {
      output_string.append("nil");
    }
    else
    {
      luaL_error(lua_state, "Error: Unknown type encountered in print function. Supported types "
                            "are String, Integer, Number, Boolean, and nil");
    }
  }

  (*print_callback_function)(output_string);

  return 0;
}

int SetLuaCoreVersion(lua_State* lua_state)
{
  std::string new_version_number = luaL_checkstring(lua_state, 1);
  if (new_version_number.length() == 0)
    luaL_error(lua_state,
               "Error: string was not passed to setLuaCoreVersion() function. Function "
               "should be called as in the following example: setLuaCoreVersion(\"3.4.2\")");
  if (new_version_number[0] == '.')
    new_version_number = "0" + new_version_number;

  size_t new_version_number_length = new_version_number.length();
  size_t i = 0;

  while (i < new_version_number_length)
  {
    if (!isdigit(new_version_number[i]))
    {
      if (new_version_number[i] == '.' && i != new_version_number_length - 1 &&
          isdigit(new_version_number[i + 1]))
        i += 2;
      else
        luaL_error(
            lua_state,
            "Error: invalid format for version number passed into setLuaCoreVersion() function. "
            "Function should be called as in the following example: setLuaCoreVersion(\"3.4.2\")");
    }
    else
      ++i;
  }

  if (new_version_number[0] == '0' || new_version_number[0] == '.')
    luaL_error(lua_state,
               "Error: value of argument passed to setLuaCoreVersion() function was less "
               "than 1.0 (1.0 is the earliest version of the API that was released.");

  global_lua_api_version = new_version_number;
  return 0;
  // Need to stop the current Lua script and start another script after this for the change in
  // version number to take effect.
}

int GetLuaCoreVersion(lua_State* lua_state)
{
  lua_pushstring(lua_state, global_lua_api_version.c_str());
  return 1;
}

void Init(const std::string& script_location,
          std::function<void(const std::string&)>* new_print_callback,
          std::function<void()>* new_script_end_callback)
{
  print_callback_function = new_print_callback;
  script_end_callback_function = new_script_end_callback;
  x = 0;
  is_lua_script_active = true;
  is_lua_core_initialized = true;
  main_lua_state = luaL_newstate();
  luaL_openlibs(main_lua_state);
  lua_newtable(main_lua_state);
  lua_pushcfunction(main_lua_state, CustomPrintFunction);
  lua_setglobal(main_lua_state, "print");
  LuaMemoryApi::InitLuaMemoryApi(main_lua_state, global_lua_api_version);
  LuaEmu::InitLuaEmuFunctions(main_lua_state, global_lua_api_version);
  LuaBit::InitLuaBitFunctions(main_lua_state, global_lua_api_version);
  LuaGameCubeController::InitLuaGameCubeControllerFunctions(main_lua_state, global_lua_api_version);
  LuaStatistics::InitLuaStatisticsFunctions(main_lua_state, global_lua_api_version);
  LuaRegisters::InitLuaRegistersFunctions(main_lua_state, global_lua_api_version);
  LuaOnFrameStartCallback::InitLuaOnFrameStartCallbackFunctions(main_lua_state,
                                                                global_lua_api_version);

  main_lua_thread_state = lua_newthread(main_lua_state);
  if (luaL_loadfile(main_lua_thread_state, script_location.c_str()) != LUA_OK)
  {
    const char* temp_string = lua_tostring(main_lua_thread_state, -1);
    (*print_callback_function)(temp_string);
    (*script_end_callback_function)();
  }
  int retVal = lua_resume(Lua::main_lua_thread_state, nullptr, 0, &Lua::x);
  if (retVal != LUA_YIELD && retVal != LUA_OK)
  {
    if (retVal == 2)
    {
      const char* error_msg = lua_tostring(Lua::main_lua_thread_state, -1);
      (*Lua::print_callback_function)(error_msg);
    }
    (*script_end_callback_function)();
    Lua::is_lua_script_active = false;
  }

  return;
}

void StopScript()
{
  is_lua_script_active = false;
}

}  // namespace Lua
