#include "Core/Lua/LuaEventCallbackClasses/LuaOnFrameStartCallbackClass.h"

#include <array>
#include <memory>

#include "Core/Lua/LuaFunctions/LuaEmuFunctions.h"
#include "Core/Lua/LuaFunctions/LuaGameCubeController.h"
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Lua/LuaVersionResolver.h"
#include "Core/Movie.h"

namespace Lua::LuaOnFrameStartCallback
{

lua_State* on_frame_start_thread = nullptr;
static lua_State** main_lua_state = nullptr;
int on_frame_start_lua_function_reference = -1;
bool frame_start_callback_is_registered = false;

static bool finished_global_code = false;
static int temp_int = 0;
static std::mutex* general_lua_lock;
std::function<void()>* shutdown_func;

class LuaOnFrameStart
{
public:
  inline LuaOnFrameStart() {}
};

static std::unique_ptr<LuaOnFrameStart> instance = nullptr;
static LuaScriptCallLocations* script_call_location = nullptr;

LuaOnFrameStart* GetInstance()
{
  if (instance == nullptr)
    instance = std::make_unique<LuaOnFrameStart>(LuaOnFrameStart());
  return instance.get();
}

static const char* class_name = "OnFrameStart";

void InitLuaOnFrameStartCallbackFunctions(lua_State** lua_state, const std::string& lua_api_version,
                                          std::mutex* new_lua_general_lock,
                                          std::function<void()>* new_shutdown_func,
                                          LuaScriptCallLocations* new_call_locations_pointer)
{
  finished_global_code = false;
  main_lua_state = lua_state;
  general_lua_lock = new_lua_general_lock;
  shutdown_func = new_shutdown_func;
  script_call_location = new_call_locations_pointer;
  LuaOnFrameStart** lua_on_frame_start_instance_ptr_ptr =
      (LuaOnFrameStart**)lua_newuserdata(*lua_state, sizeof(LuaOnFrameStart*));
  *lua_on_frame_start_instance_ptr_ptr = GetInstance();
  luaL_newmetatable(*lua_state, "LuaOnFrameStartFunctionsMetaTable");
  lua_pushvalue(*lua_state, -1);
  lua_setfield(*lua_state, -2, "__index");
  std::array lua_on_frame_start_functions_list_with_versions_attached = {

      luaL_Reg_With_Version({"register", "1.0", Register}),
      luaL_Reg_With_Version({"unregister", "1.0", Unregister})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_on_frame_start_functions_list_with_versions_attached,
                               lua_api_version, deprecated_functions_map, *lua_state);
  lua_setglobal(*lua_state, class_name);
  on_frame_start_thread = lua_newthread(*lua_state);
}

int Register(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "register", "(functionName)");
  if (frame_start_callback_is_registered)
    luaL_unref(lua_state, LUA_REGISTRYINDEX, on_frame_start_lua_function_reference);
  lua_pushvalue(lua_state, 2);
  on_frame_start_lua_function_reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
  frame_start_callback_is_registered = true;
  return 0;
}
int Unregister(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "unregister", "()");
  if (frame_start_callback_is_registered)
  {
    luaL_unref(lua_state, LUA_REGISTRYINDEX, on_frame_start_lua_function_reference);
    frame_start_callback_is_registered = false;
    on_frame_start_lua_function_reference = -1;
  }
  return 0;
}

int RunCallback()
{
  general_lua_lock->lock();
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

  if (!finished_global_code)
  {
    *script_call_location = LuaScriptCallLocations::FromFrameStartGlobalScope;
    int ret_val = lua_resume(*main_lua_state, nullptr, 0, &temp_int);
    if (ret_val == LUA_YIELD)
      LuaEmu::called_yielding_function_on_last_frame = true;
    else
    {
      finished_global_code = true;
      LuaEmu::called_yielding_function_on_last_frame = false;
    }

    if (finished_global_code && !frame_start_callback_is_registered)
      (*shutdown_func)();
  }

  if (finished_global_code && frame_start_callback_is_registered)
  {
    *script_call_location = LuaScriptCallLocations::FromFrameStartCallback;
    if (LuaEmu::called_yielding_function_on_last_frame)
    {
      int ret_val = lua_resume(on_frame_start_thread, nullptr, 0, &temp_int);
      if (ret_val == LUA_YIELD)
        LuaEmu::called_yielding_function_on_last_frame = true;
      else
        LuaEmu::called_yielding_function_on_last_frame = false;
    }
    else
    {
      lua_rawgeti(on_frame_start_thread, LUA_REGISTRYINDEX, on_frame_start_lua_function_reference);
      int ret_val = lua_resume(on_frame_start_thread, nullptr, 0, &temp_int);
      if (ret_val == LUA_YIELD)
        LuaEmu::called_yielding_function_on_last_frame = true;
      else
        LuaEmu::called_yielding_function_on_last_frame = false;
    }
    if (finished_global_code && !frame_start_callback_is_registered &&
        !LuaEmu::called_yielding_function_on_last_frame)
      (*shutdown_func)();
  }

  general_lua_lock->unlock();
  return 0;
}
}  // namespace Lua::LuaOnFrameStartCallback
