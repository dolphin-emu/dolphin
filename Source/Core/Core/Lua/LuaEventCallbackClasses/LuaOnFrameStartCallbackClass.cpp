#include "Core/Lua/LuaEventCallbackClasses/LuaOnFrameStartCallbackClass.h"

#include <array>
#include <memory>
#include <vector>

#include "Core/Lua/LuaFunctions/LuaEmuFunctions.h"
#include "Core/Lua/LuaFunctions/LuaGameCubeController.h"
#include "Core/Lua/LuaHelperClasses/LuaStateToScriptContextMap.h"
#include "Core/Lua/LuaVersionResolver.h"
#include "Core/Movie.h"

namespace Lua::LuaOnFrameStartCallback
{
std::mutex frame_start_lock;
static int x;

class LuaOnFrameStart
{
public:
  inline LuaOnFrameStart() {}
};

static std::unique_ptr<LuaOnFrameStart> instance = nullptr;

LuaOnFrameStart* GetInstance()
{
  if (instance == nullptr)
    instance = std::make_unique<LuaOnFrameStart>(LuaOnFrameStart());
  return instance.get();
}

static const char* class_name = "OnFrameStart";
static std::vector<std::shared_ptr<LuaScriptState>>* pointer_to_lua_script_list;
static std::function<void(int)>* script_end_callback_function = nullptr;
static std::function<void(const std::string&)>* print_callback_function;

void InitLuaOnFrameStartCallbackFunctions(lua_State* main_lua_thread, const std::string& lua_api_version,
    std::vector<std::shared_ptr<LuaScriptState>>* new_pointer_to_lua_script_list,
    std::function<void(const std::string&)>* new_print_callback_function,
    std::function<void(int)>* new_script_end_callback_function)
{
  x = 0;
  pointer_to_lua_script_list = new_pointer_to_lua_script_list;
  print_callback_function = new_print_callback_function;
  script_end_callback_function = new_script_end_callback_function;
  LuaOnFrameStart** lua_on_frame_start_instance_ptr_ptr =
      (LuaOnFrameStart**)lua_newuserdata(main_lua_thread, sizeof(LuaOnFrameStart*));
  *lua_on_frame_start_instance_ptr_ptr = GetInstance();
  luaL_newmetatable(main_lua_thread, "LuaOnFrameStartFunctionsMetaTable");
  lua_pushvalue(main_lua_thread, -1);
  lua_setfield(main_lua_thread, -2, "__index");
  std::array lua_on_frame_start_functions_list_with_versions_attached = {

      luaL_Reg_With_Version({"register", "1.0", Register}),
      luaL_Reg_With_Version({"unregister", "1.0", Unregister})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_on_frame_start_functions_list_with_versions_attached,
                               lua_api_version, deprecated_functions_map, main_lua_thread);
  lua_setglobal(main_lua_thread, class_name);
}

int Register(lua_State* lua_state)
{
  lua_getglobal(lua_state, "StateToScriptContextMap");
  std::string r = luaL_typename(lua_state, -1);
  if (!lua_islightuserdata(lua_state, -1))
    luaL_error(lua_state, (std::string("Error: type of value found in stack was ") + r).c_str());
  LuaStateToScriptContextMap* state_to_script_context_map = (LuaStateToScriptContextMap*) lua_touserdata(lua_state, -1);
  lua_pop(lua_state, 1);
  LuaScriptState* corresponding_script_context =
      state_to_script_context_map->lua_state_to_script_context_pointer_map[lua_state];
  if (corresponding_script_context == nullptr)
    luaL_error(lua_state, "Error: in OnFrameStart:register(func) method, could not find the lua script "
                          "context associated with the current lua_State*");

  std::string q = luaL_typename(lua_state, 2);
  lua_pushvalue(lua_state, 2);
  lua_xmove(lua_state, corresponding_script_context->frame_callback_lua_thread, 1);
  lua_pop(lua_state, 1);
  int new_function_reference = luaL_ref(corresponding_script_context->frame_callback_lua_thread, LUA_REGISTRYINDEX);
  corresponding_script_context->frame_callback_locations.push_back(new_function_reference);
  lua_pushinteger(lua_state, new_function_reference);
  return 1;
}
int Unregister(lua_State* lua_state)
{
  int function_reference_to_delete = lua_tointeger(lua_state, 2);
  lua_getglobal(lua_state, "StateToScriptContextMap");
  LuaStateToScriptContextMap* state_to_script_context_map =
      (LuaStateToScriptContextMap*)lua_touserdata(lua_state, -1);
  lua_pop(lua_state, 1);
  LuaScriptState* corresponding_script_context =
      state_to_script_context_map->lua_state_to_script_context_pointer_map[lua_state];
  if (corresponding_script_context == nullptr)
    luaL_error(lua_state, "Error: in OnFrameStart:unregister(func) method, could not find the lua "
                          "script context associated with the current lua_State*");
  if (std::find(corresponding_script_context->frame_callback_locations.begin(),
                corresponding_script_context->frame_callback_locations.end(),
                function_reference_to_delete) ==
      corresponding_script_context->frame_callback_locations.end())
    luaL_error(lua_state, "Error: Attempt to call OnFrameStart:unregister(func) on a function "
                          "which wasn't registered as a frame callback function!");
  corresponding_script_context->frame_callback_locations.erase((std::find(corresponding_script_context->frame_callback_locations.begin(),
             corresponding_script_context->frame_callback_locations.end(),
             function_reference_to_delete)));
  luaL_unref(corresponding_script_context->frame_callback_lua_thread, LUA_REGISTRYINDEX, function_reference_to_delete);
  return 0;
}

int RunCallbacks()
{
  std::lock_guard<std::mutex> lock(frame_start_lock);

  size_t number_of_scripts = pointer_to_lua_script_list->size();
  for (size_t i = 0; i < number_of_scripts; ++i)
  {
    LuaScriptState* current_script = (*pointer_to_lua_script_list)[i].get();
    if (!current_script->is_lua_script_active)
      break;
    current_script->lua_script_specific_lock.lock();
    if (current_script->frame_callback_locations.size() > 0)
    {
      current_script->current_call_location = Scripting::ScriptCallLocations::FromFrameStartCallback;
      if (current_script->index_of_next_frame_callback_to_execute >=
          current_script->frame_callback_locations.size())
      {
        current_script->index_of_next_frame_callback_to_execute = 0;
        current_script->called_yielding_function_in_last_frame_callback_script_resume = false;
      }
      for (current_script->index_of_next_frame_callback_to_execute;
           current_script->index_of_next_frame_callback_to_execute <
           current_script->frame_callback_locations.size();
           ++(current_script->index_of_next_frame_callback_to_execute))
      {
        if (current_script->called_yielding_function_in_last_frame_callback_script_resume)
        {
          int ret_val = lua_resume(current_script->frame_callback_lua_thread, nullptr, 0, &x);
          if (ret_val == LUA_YIELD)
            current_script->called_yielding_function_in_last_frame_callback_script_resume = true;
          else
          {
            current_script->called_yielding_function_in_last_frame_callback_script_resume = false;
            if (ret_val != LUA_OK)
            {
              const char* error_msg = lua_tostring(current_script->frame_callback_lua_thread, -1);
              (*print_callback_function)(error_msg);
              (*script_end_callback_function)(current_script->unique_script_identifier);
              current_script->is_lua_script_active = false;
            }
          }
        }
        else
        {
          lua_rawgeti(current_script->frame_callback_lua_thread, LUA_REGISTRYINDEX,
                      current_script->frame_callback_locations
                          [current_script->index_of_next_frame_callback_to_execute]);
          int ret_val = lua_resume(current_script->frame_callback_lua_thread, nullptr, 0, &x);
          if (ret_val == LUA_YIELD)
            current_script->called_yielding_function_in_last_frame_callback_script_resume = true;
          else
          {
            current_script->called_yielding_function_in_last_frame_callback_script_resume = false;
            if (ret_val != LUA_OK)
            {
              const char* error_msg = lua_tostring(current_script->frame_callback_lua_thread, -1);
              (*print_callback_function)(error_msg);
              (*script_end_callback_function)(current_script->unique_script_identifier);
              current_script->is_lua_script_active = false;
            }
            break;
          }
        }
      }
      if (current_script->finished_with_global_code &&
          current_script->frame_callback_locations.size() == 0 &&
          current_script->gc_controller_input_polled_callback_locations.size() == 0 &&
          !current_script->called_yielding_function_in_last_frame_callback_script_resume)
        (*script_end_callback_function)(current_script->unique_script_identifier);
    }

    if (!current_script->finished_with_global_code)
    {
      current_script->current_call_location = Scripting::ScriptCallLocations::FromFrameStartGlobalScope;
      int ret_val = lua_resume(current_script->main_lua_thread, nullptr, 0, &x);
      if (ret_val == LUA_YIELD)
        current_script->called_yielding_function_in_last_global_script_resume = true;
      else
      {
        current_script->finished_with_global_code = true;
        current_script->called_yielding_function_in_last_global_script_resume = false;
        if (ret_val != LUA_OK)
        {
            const char* error_msg = lua_tostring(current_script->main_lua_thread, -1);
            (*print_callback_function)(error_msg);
            (*script_end_callback_function)(current_script->unique_script_identifier);
            current_script->is_lua_script_active = false;
        }
        
        }

      if (current_script->finished_with_global_code && current_script->frame_callback_locations.size() == 0 && current_script->gc_controller_input_polled_callback_locations.size() == 0)
        (*script_end_callback_function)(current_script->unique_script_identifier);
    }
    current_script->lua_script_specific_lock.unlock();
  }

  return 0;
}
}  // namespace Lua::LuaOnFrameStartCallback
