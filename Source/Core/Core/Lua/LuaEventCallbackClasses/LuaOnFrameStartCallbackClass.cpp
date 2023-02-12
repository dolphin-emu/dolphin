#include "Core/Lua/LuaEventCallbackClasses/LuaOnFrameStartCallbackClass.h"

#include <array>
#include <memory>

#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Lua/LuaVersionResolver.h"

namespace Lua::LuaOnFrameStartCallback
{

lua_State* on_frame_start_thread = nullptr;
int on_frame_start_lua_function_reference = -1;
bool frame_start_callback_is_registered = false;

static bool in_middle_of_callback = false;
static int temp_int = 0;

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

void InitLuaOnFrameStartCallbackFunctions(lua_State* lua_state, const std::string& lua_api_version)
{
  LuaOnFrameStart** lua_on_frame_start_instance_ptr_ptr =
      (LuaOnFrameStart**)lua_newuserdata(lua_state, sizeof(LuaOnFrameStart*));
  *lua_on_frame_start_instance_ptr_ptr = GetInstance();
  luaL_newmetatable(lua_state, "LuaOnFrameStartFunctionsMetaTable");
  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "__index");
  std::array lua_on_frame_start_functions_list_with_versions_attached = {

      luaL_Reg_With_Version({"register", "1.0", Register}),
      luaL_Reg_With_Version({"unregister", "1.0", Unregister})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_on_frame_start_functions_list_with_versions_attached,
                               lua_api_version, deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, "OnFrameStart");
  on_frame_start_thread = lua_newthread(lua_state);
}

int Register(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "OnFrameStart:register", "OnFrameStart:register(functionName)");
  if (frame_start_callback_is_registered)
    luaL_unref(lua_state, LUA_REGISTRYINDEX, on_frame_start_lua_function_reference);
  lua_pushvalue(lua_state, 2);
  on_frame_start_lua_function_reference = luaL_ref(lua_state, LUA_REGISTRYINDEX);
  frame_start_callback_is_registered = true;
  in_middle_of_callback = false;
  return 0;
}
int Unregister(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, "OnFrameStart:unregister", "OnFrameStart:unregister()");
  if (frame_start_callback_is_registered)
  {
    luaL_unref(lua_state, LUA_REGISTRYINDEX, on_frame_start_lua_function_reference);
    frame_start_callback_is_registered = false;
    on_frame_start_lua_function_reference = -1;
    in_middle_of_callback = false;
  }
  return 0;
}

int RunCallback()
{
  if (frame_start_callback_is_registered)
  {
    if (in_middle_of_callback)
    {
      lua_resume(on_frame_start_thread, nullptr, 0, &temp_int);
      in_middle_of_callback = false;
    }
    else
    {
      in_middle_of_callback = true;
      lua_rawgeti(on_frame_start_thread, LUA_REGISTRYINDEX, on_frame_start_lua_function_reference);
      lua_pcall(on_frame_start_thread, 0, 0, 0);
      in_middle_of_callback = false;
    }
  }
  return 0;
}
}  // namespace Lua::LuaOnFrameStartCallback
