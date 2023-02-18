#include "Core/Lua/LuaEventCallbackClasses/LuaOnFrameStartCallbackClass.h"

#include <array>
#include <memory>

#include "Core/Lua/LuaFunctions/LuaEmuFunctions.h"
#include "Core/Lua/LuaFunctions/LuaGameCubeController.h"
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Lua/LuaHelperClasses/LuaScriptLocationModifier.h"
#include "Core/Lua/LuaVersionResolver.h"
#include "Core/Movie.h"

namespace Lua::LuaOnFrameStartCallback
{
static std::mutex frame_start_lock;
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
static std::vector<std::shared_ptr<LuaScriptContext>>* pointer_to_lua_script_list;
static std::function<void(int)>* script_end_callback_function = nullptr;

void InitLuaOnFrameStartCallbackFunctions(lua_State* main_lua_thread, const std::string& lua_api_version,
    std::vector<std::shared_ptr<LuaScriptContext>>* new_pointer_to_lua_script_list,
    std::function<void(int)>* new_script_end_callback_function)
{
  x = 0;
  pointer_to_lua_script_list = new_pointer_to_lua_script_list;
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
  LuaColonOperatorTypeCheck(lua_state, class_name, "register", "(functionName)");
  /* lua_pushvalue(lua_state, 2);
   pointer_to_lua_script_list  = luaL_ref(lua_state, LUA_REGISTRYINDEX); TODO: Fix this*/
  return 0;
}
int Unregister(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "unregister", "()"); /*
  if (frame_start_callback_is_registered)
  {
    luaL_unref(lua_state, LUA_REGISTRYINDEX, on_frame_start_lua_function_reference);
    frame_start_callback_is_registered = false;
    on_frame_start_lua_function_reference = -1;
  }
  TODO: Fix this*/
  return 0;
}

int RunCallbacks()
{
  std::lock_guard<std::mutex> lock(frame_start_lock);
  for (int i = 0; i < 4; ++i)
  {
    Lua::LuaGameCubeController::overwrite_controller_at_specified_port[i] = false;
    Lua::LuaGameCubeController::add_to_controller_at_specified_port[i] = false;
    Lua::LuaGameCubeController::do_random_input_events_at_specified_port[i] = false;
    Lua::LuaGameCubeController::random_button_events[i].clear();
    Lua::LuaGameCubeController::button_lists_for_add_to_controller_inputs[i].clear();
    memset(&Lua::LuaGameCubeController::new_overwrite_controller_inputs[i], 0,
           sizeof(Movie::ControllerState));
    memset(&Lua::LuaGameCubeController::add_to_controller_inputs[i], 0,
           sizeof(Movie::ControllerState));
  }

  size_t number_of_scripts = pointer_to_lua_script_list->size();
  for (size_t i = 0; i < number_of_scripts; ++i)
  {
    LuaScriptContext* current_script = (*pointer_to_lua_script_list)[i].get();
    if (!current_script->is_lua_script_active)
      break;
    current_script->lua_script_specific_lock.lock();
    if (!current_script->finished_with_global_code)
    {
      Lua::SetScriptCallLocation(current_script->main_lua_thread,
                                 LuaScriptCallLocations::FromFrameStartGlobalScope);
      int ret_val = lua_resume(current_script->main_lua_thread, nullptr, 0, &x);
      if (ret_val == LUA_YIELD)
        current_script->called_yielding_function_in_last_global_script_resume = true;
      else
      {
        current_script->finished_with_global_code = true;
        current_script->called_yielding_function_in_last_global_script_resume = false;
      }

      if (current_script->finished_with_global_code && current_script->frame_callback_locations.size() == 0)
        (*script_end_callback_function)(current_script->unique_script_identifier);
    }

    if (current_script->finished_with_global_code && current_script->frame_callback_locations.size() > 0)
    {
      Lua::SetScriptCallLocation(current_script->frame_callback_lua_thread,
                                 LuaScriptCallLocations::FromFrameStartCallback);
      if (current_script->index_of_next_frame_callback_to_execute >=
          current_script->frame_callback_locations.size())
      {
        current_script->index_of_next_frame_callback_to_execute = 0;
        current_script->called_yielding_function_in_last_frame_callback_script_resume = false;
      }
      for (current_script->index_of_next_frame_callback_to_execute;
           current_script->index_of_next_frame_callback_to_execute < current_script->frame_callback_locations.size(); ++(current_script->index_of_next_frame_callback_to_execute))
      {
        if (current_script->called_yielding_function_in_last_frame_callback_script_resume)
        {
          int ret_val = lua_resume(current_script->frame_callback_lua_thread, nullptr, 0, &x);
          if (ret_val != LUA_YIELD)
            current_script->called_yielding_function_in_last_frame_callback_script_resume = false;
          else
          {
            current_script->called_yielding_function_in_last_frame_callback_script_resume = true;
            break;
          }
      }
        else {
          lua_rawgeti(current_script->frame_callback_lua_thread, LUA_REGISTRYINDEX,
                      current_script->frame_callback_locations[current_script->index_of_next_frame_callback_to_execute]);
          int ret_val = lua_resume(current_script->frame_callback_lua_thread, nullptr, 0, &x);
          if (ret_val != LUA_YIELD)
            current_script->called_yielding_function_in_last_frame_callback_script_resume = false;
          else
          {
            current_script->called_yielding_function_in_last_frame_callback_script_resume = true;
            break;
          }
            }
    }
      if (current_script->finished_with_global_code && current_script->frame_callback_locations.size() == 0 &&
          !current_script->called_yielding_function_in_last_frame_callback_script_resume)
            (*script_end_callback_function)(current_script->unique_script_identifier);
    }
    current_script->lua_script_specific_lock.unlock();
  }

  return 0;
}
}  // namespace Lua::LuaOnFrameStartCallback
