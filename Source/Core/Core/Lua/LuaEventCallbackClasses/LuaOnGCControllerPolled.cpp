#include "Core/Lua/LuaEventCallbackClasses/LuaOnGCControllerPolled.h"

#include <array>
#include <memory>
#include <vector>

#include "Core/Scripting/InternalAPIModules/EmuAPI.h"
#include "Core/Scripting/InternalAPIModules/GameCubeControllerAPI.h"
#include "Core/Lua/LuaHelperClasses/LuaStateToScriptContextMap.h"
#include "Core/Lua/LuaVersionResolver.h"
#include "Core/Movie.h"

namespace Lua::LuaOnGCControllerPolled
{
std::mutex gc_controller_polled_lock;
static int x;

class LuaOnGCControllerPoll
{
public:
  inline LuaOnGCControllerPoll() {}
};

static std::unique_ptr<LuaOnGCControllerPoll> instance = nullptr;

LuaOnGCControllerPoll* GetInstance()
{
  if (instance == nullptr)
    instance = std::make_unique<LuaOnGCControllerPoll>(LuaOnGCControllerPoll());
  return instance.get();
}

static const char* class_name = "OnGCControllerPolled";
static std::vector<std::shared_ptr<LuaScriptState>>* pointer_to_lua_script_list;
static std::function<void(const std::string&)>* print_callback_function;
static std::function<void(int)>* script_end_callback_function = nullptr;

void InitLuaOnGCControllerPolledCallbackFunctions(
    lua_State* main_lua_thread, const std::string& lua_api_version,
    std::vector<std::shared_ptr<LuaScriptState>>* new_pointer_to_lua_script_list,
    std::function<void(const std::string&)>* new_print_callback_function,
    std::function<void(int)>* new_script_end_callback_function)
{
  x = 0;
  pointer_to_lua_script_list = new_pointer_to_lua_script_list;
  print_callback_function = new_print_callback_function;
  script_end_callback_function = new_script_end_callback_function;
  LuaOnGCControllerPoll** lua_on_gc_controller_polled_instance_ptr_ptr =
      (LuaOnGCControllerPoll**)lua_newuserdata(main_lua_thread, sizeof(LuaOnGCControllerPoll*));
  *lua_on_gc_controller_polled_instance_ptr_ptr = GetInstance();
  luaL_newmetatable(main_lua_thread, "LuaOnGCControllerPolledFunctionsMetaTable");
  lua_pushvalue(main_lua_thread, -1);
  lua_setfield(main_lua_thread, -2, "__index");
  std::array lua_on_controller_polled_functions_list_with_versions_attached = {

      luaL_Reg_With_Version({"register", "1.0", Register}),
      luaL_Reg_With_Version({"unregister", "1.0", Unregister})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_on_controller_polled_functions_list_with_versions_attached,
                               lua_api_version, deprecated_functions_map, main_lua_thread);
  lua_setglobal(main_lua_thread, class_name);
}

int Register(lua_State* lua_state)
{
  lua_getglobal(lua_state, "StateToScriptContextMap");
  std::string r = luaL_typename(lua_state, -1);
  if (!lua_islightuserdata(lua_state, -1))
    luaL_error(lua_state, (std::string("Error: type of value found in stack was ") + r).c_str());
  LuaStateToScriptContextMap* state_to_script_context_map =
      (LuaStateToScriptContextMap*)lua_touserdata(lua_state, -1);
  lua_pop(lua_state, 1);
  LuaScriptState* corresponding_script_context =
      state_to_script_context_map->lua_state_to_script_context_pointer_map[lua_state];
  if (corresponding_script_context == nullptr)
    luaL_error(lua_state,
               "Error: in OnGCControllerInputPolled:register(func) method, could not find the lua script "
               "context associated with the current lua_State*");

  std::string q = luaL_typename(lua_state, 2);
  lua_pushvalue(lua_state, 2);
  lua_xmove(lua_state, corresponding_script_context->gc_controller_input_polled_callback_lua_thread, 1);
  lua_pop(lua_state, 1);
  int new_function_reference =
      luaL_ref(corresponding_script_context->gc_controller_input_polled_callback_lua_thread, LUA_REGISTRYINDEX);
  corresponding_script_context->gc_controller_input_polled_callback_locations.push_back(new_function_reference);
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
    luaL_error(lua_state, "Error: in OnGCControllerInputPolled:unregister(func) method, could not find the lua "
                          "script context associated with the current lua_State*");
  if (std::find(corresponding_script_context->gc_controller_input_polled_callback_locations.begin(),
                corresponding_script_context->gc_controller_input_polled_callback_locations.end(),
                function_reference_to_delete) ==
      corresponding_script_context->gc_controller_input_polled_callback_locations.end())
    luaL_error(lua_state, "Error: Attempt to call OnGCControllerInputPolled:unregister(func) on a function "
                          "which wasn't registered as a GCControllerInput callback function!");
  corresponding_script_context->gc_controller_input_polled_callback_locations.erase((std::find(
      corresponding_script_context->gc_controller_input_polled_callback_locations.begin(),
      corresponding_script_context->gc_controller_input_polled_callback_locations.end(), function_reference_to_delete)));
  luaL_unref(corresponding_script_context->gc_controller_input_polled_callback_lua_thread, LUA_REGISTRYINDEX,
             function_reference_to_delete);
  return 0;
}

int RunCallbacks()
{
  std::lock_guard<std::mutex> lock(gc_controller_polled_lock);

  size_t number_of_scripts = pointer_to_lua_script_list->size();
  for (size_t i = 0; i < number_of_scripts; ++i)
  {
    LuaScriptState* current_script = (*pointer_to_lua_script_list)[i].get();
    if (!current_script->is_lua_script_active)
      break;
    current_script->lua_script_specific_lock.lock();

    if (current_script->gc_controller_input_polled_callback_locations.size() > 0)
    {
      current_script->current_call_location = Scripting::ScriptCallLocations::FromGCControllerInputPolled;
      
      for (size_t function_index = 0; function_index < current_script->gc_controller_input_polled_callback_locations.size(); ++function_index)
      {
          lua_rawgeti(current_script->gc_controller_input_polled_callback_lua_thread, LUA_REGISTRYINDEX,
                      current_script->gc_controller_input_polled_callback_locations[function_index]);
          int ret_val = lua_resume(current_script->gc_controller_input_polled_callback_lua_thread, nullptr, 0, &x);
          if (ret_val != LUA_YIELD)
          {
           if (ret_val != LUA_OK)
              {
                const char* error_msg = lua_tostring(current_script->gc_controller_input_polled_callback_lua_thread, -1);
                (*print_callback_function)(error_msg);
                (*script_end_callback_function)(current_script->unique_script_identifier);
                current_script->is_lua_script_active = false;
              }
          }
      }
      
      if (current_script->finished_with_global_code &&
          current_script->frame_callback_locations.size() == 0 && current_script->gc_controller_input_polled_callback_locations.size() == 0 &&
          !current_script->called_yielding_function_in_last_frame_callback_script_resume
        && current_script->gc_controller_input_polled_callback_locations.size() == 0)
        (*script_end_callback_function)(current_script->unique_script_identifier);
    }
    current_script->lua_script_specific_lock.unlock();
  }

  return 0;
}
}  // namespace Lua::LuaOnGCControllerInputPolled
