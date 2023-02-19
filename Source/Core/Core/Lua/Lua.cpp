#include "Core/Lua/Lua.h"

#include <filesystem>
#include <mutex>
#include "Core/Lua/LuaEventCallbackClasses/LuaOnFrameStartCallbackClass.h"
#include "Core/Lua/LuaFunctions/LuaImportModule.h"
#include "Core/Lua/LuaHelperClasses/LuaScriptCallLocations.h"
#include "Core/Lua/LuaHelperClasses/LuaStateToScriptContextMap.h"
#include "Core/Movie.h"

namespace Lua
{
std::string global_lua_api_version = "1.0.0";
int x = 0;
bool is_lua_core_initialized = false;
std::vector<std::shared_ptr<LuaScriptContext>> list_of_lua_script_contexts = std::vector<std::shared_ptr<LuaScriptContext>>();
std::function<void(const std::string&)>* print_callback_function = nullptr;
std::function<void(int)>* script_end_callback_function = nullptr;
static std::mutex lua_initialization_and_destruction_lock;
static std::unique_ptr<LuaStateToScriptContextMap> state_to_script_context_map = nullptr;

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
          std::function<void(int)>* new_script_end_callback, int unique_script_identifier)
{
  const std::lock_guard<std::mutex> lock(lua_initialization_and_destruction_lock);
  lua_State* new_main_lua_state = luaL_newstate();
  luaL_openlibs(new_main_lua_state);

  if (!is_lua_core_initialized)
  {
    state_to_script_context_map =
        std::make_unique<LuaStateToScriptContextMap>(LuaStateToScriptContextMap());
    x = 0;
    print_callback_function = new_print_callback;
    script_end_callback_function = new_script_end_callback;
    is_lua_core_initialized = true;
  }

  lua_pushlightuserdata(new_main_lua_state, state_to_script_context_map.get());
  lua_setglobal(new_main_lua_state, "StateToScriptContextMap");
  
  lua_newtable(new_main_lua_state);
  lua_pushcfunction(new_main_lua_state, CustomPrintFunction);
  lua_setglobal(new_main_lua_state, "print");
  std::shared_ptr<LuaScriptContext> new_lua_script_context = Lua::CreateNewLuaScriptContext(new_main_lua_state, unique_script_identifier,  script_location);
  state_to_script_context_map.get()
      ->lua_state_to_script_context_pointer_map[new_lua_script_context->main_lua_thread] =
      new_lua_script_context.get();

  new_lua_script_context->current_call_location = LuaScriptCallLocations::FromScriptStartup;
  list_of_lua_script_contexts.push_back(new_lua_script_context);

  LuaImportModule::InitLuaImportModule(new_lua_script_context->main_lua_thread, global_lua_api_version);
  LuaOnFrameStartCallback::InitLuaOnFrameStartCallbackFunctions(
      new_lua_script_context->main_lua_thread, global_lua_api_version, &list_of_lua_script_contexts,
      script_end_callback_function);
  new_lua_script_context->frame_callback_lua_thread = lua_newthread(new_lua_script_context->main_lua_thread);
   state_to_script_context_map.get()
      ->lua_state_to_script_context_pointer_map[new_lua_script_context->frame_callback_lua_thread] =
      new_lua_script_context.get();


  if (luaL_loadfile(new_lua_script_context->main_lua_thread, new_lua_script_context->script_filename.c_str()) != LUA_OK)
  {
    const char* temp_string = lua_tostring(new_lua_script_context->main_lua_thread, -1);
    (*print_callback_function)(temp_string);
    (*script_end_callback_function)(unique_script_identifier);
  }
  new_lua_script_context->lua_script_specific_lock.lock();
  int retVal = lua_resume(new_lua_script_context->main_lua_thread, nullptr, 0, &Lua::x);
  if (retVal == LUA_YIELD)
    new_lua_script_context->called_yielding_function_in_last_global_script_resume = true;
  else
  {
    new_lua_script_context->called_yielding_function_in_last_global_script_resume = false;
    if (retVal == LUA_OK)
      new_lua_script_context->finished_with_global_code = true;
    else
    {
      if (retVal == 2)
      {
        const char* error_msg = lua_tostring(new_lua_script_context->main_lua_thread, -1);
        (*Lua::print_callback_function)(error_msg);
      }
      (*script_end_callback_function)(unique_script_identifier);
      new_lua_script_context->is_lua_script_active = false;
    }
    }
  new_lua_script_context->lua_script_specific_lock.unlock();

  

  return;
}

void StopScript(int unique_identifier)
{
  const std::lock_guard<std::mutex> lock(lua_initialization_and_destruction_lock);
  for (int i = 0; i < list_of_lua_script_contexts.size(); ++i)
  {
    if (list_of_lua_script_contexts[i].get()->unique_script_identifier == unique_identifier)
    {
      LuaScriptContext* script_context_to_delete = list_of_lua_script_contexts[i].get();
      script_context_to_delete->lua_script_specific_lock.lock();
      script_context_to_delete->is_lua_script_active = false;
      script_context_to_delete->lua_script_specific_lock.unlock();
    }
  }
}

}  // namespace Lua
