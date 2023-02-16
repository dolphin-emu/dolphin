#include "Core/Lua/LuaFunctions/LuaStatistics.h"

#include <fmt/format.h>
#include <memory>
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Lua/LuaVersionResolver.h"

namespace Lua::LuaStatistics
{
const char* class_name = "statistics";

class Lua_Statistics
{
public:
  inline Lua_Statistics(){};
};

static LuaScriptCallLocations* script_call_location_pointer = nullptr;
static std::unique_ptr<Lua_Statistics> lua_statistics_pointer = nullptr;

Lua_Statistics* getStatisticsInstance()
{
  if (lua_statistics_pointer == nullptr)
    lua_statistics_pointer = std::make_unique<Lua_Statistics>(Lua_Statistics());
  return lua_statistics_pointer.get();
}

void InitLuaStatisticsFunctions(lua_State* lua_state, const std::string& lua_api_version,
                                LuaScriptCallLocations* new_script_call_location_pointer)
{
  script_call_location_pointer = new_script_call_location_pointer;
  Lua_Statistics** lua_statistics_ptr_ptr =
      (Lua_Statistics**)lua_newuserdata(lua_state, sizeof(Lua_Statistics*));
  *lua_statistics_ptr_ptr = getStatisticsInstance();
  luaL_newmetatable(lua_state, "LuaStatisticsTable");
  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "__index");

  std::array lua_statistics_functions_with_versions_attached = {
      luaL_Reg_With_Version({"isRecordingInput", "1.0", IsRecordingInput}),
      luaL_Reg_With_Version(
          {"isRecordingInputFromSaveState", "1.0", IsRecordingInputFromSaveState}),
      luaL_Reg_With_Version({"isPlayingInput", "1.0", IsPlayingInput}),
      luaL_Reg_With_Version({"isMovieActive", "1.0", IsMovieActive}),
      luaL_Reg_With_Version({"getCurrentFrame", "1.0", GetCurrentFrame}),
      luaL_Reg_With_Version({"getMovieLength", "1.0", GetMovieLength}),
      luaL_Reg_With_Version({"getRerecordCount", "1.0", GetRerecordCount}),
      luaL_Reg_With_Version({"getCurrentInputCount", "1.0", GetCurrentInputCount}),
      luaL_Reg_With_Version({"getTotalInputCount", "1.0", GetTotalInputCount}),
      luaL_Reg_With_Version({"getCurrentLagCount", "1.0", GetCurrentLagCount}),
      luaL_Reg_With_Version({"getTotalLagCount", "1.0", GetTotalLagCount}),
      luaL_Reg_With_Version({"isGcControllerInPort", "1.0", IsGcControllerInPort}),
      luaL_Reg_With_Version({"isUsingPort", "1.0", IsUsingPort})};

  std::unordered_map<std::string, std::string> deprecated_functions_map;
  AddLatestFunctionsForVersion(lua_statistics_functions_with_versions_attached, lua_api_version,
                               deprecated_functions_map, lua_state);
  lua_setglobal(lua_state, class_name);
}

int IsRecordingInput(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "isRecordingInput", "()");
  lua_pushboolean(lua_state, Movie::IsRecordingInput());
  return 1;
}

int IsRecordingInputFromSaveState(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "isRecordingInputFromSaveState", "()");
  lua_pushboolean(lua_state, Movie::IsRecordingInputFromSaveState());
  return 1;
}

int IsPlayingInput(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "isPlayingInput", "()");
  lua_pushboolean(lua_state, Movie::IsPlayingInput());
  return 1;
}

int IsMovieActive(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "isMovieActive", "()");
  lua_pushboolean(lua_state, Movie::IsMovieActive());
  return 1;
}

int GetCurrentFrame(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "getCurrentFrame", "()");
  lua_pushinteger(lua_state, Movie::GetCurrentFrame());
  return 1;
}

int GetMovieLength(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "getMovieLength", "()");
  lua_pushinteger(lua_state, Movie::GetTotalFrames());
  return 1;
}

int GetRerecordCount(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "getRerecordCount", "()");
  lua_pushinteger(lua_state, Movie::GetRerecordCount());
  return 1;
}

int GetCurrentInputCount(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "getCurrentInputCount", "()");
  lua_pushinteger(lua_state, Movie::GetCurrentInputCount());
  return 1;
}

int GetTotalInputCount(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "getTotalInputCount", "()");
  lua_pushinteger(lua_state, Movie::GetTotalInputCount());
  return 1;
}

int GetCurrentLagCount(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "getCurrentLagCount", "()");
  lua_pushinteger(lua_state, Movie::GetCurrentLagCount());
  return 1;
}

int GetTotalLagCount(lua_State* lua_state)
{
  LuaColonOperatorTypeCheck(lua_state, class_name, "getTotalLagCount", "()");
  lua_pushinteger(lua_state, Movie::GetTotalLagCount());
  return 1;
}

int IsGcControllerInPort(lua_State* lua_state)
{
  const char* func_name = "isGcControllerInPort";
  LuaColonOperatorTypeCheck(lua_state, class_name, func_name, "(1)");
  s64 port_number = luaL_checkinteger(lua_state, 2);
  if (port_number < 1 || port_number > 4)
    luaL_error(lua_state,
               fmt::format("Error: in {}:{}() function, portNumber was not between 1 and 4",
                           class_name, func_name)
                   .c_str());
  lua_pushboolean(lua_state, Movie::IsUsingGCController(port_number - 1));
  return 1;
}

int IsUsingPort(lua_State* lua_state)
{
  const char* func_name = "isUsingPort";
  LuaColonOperatorTypeCheck(lua_state, class_name, func_name, "(1)");
  s64 port_number = luaL_checkinteger(lua_state, 2);
  if (port_number < 1 || port_number > 4)
    luaL_error(lua_state,
               fmt::format("Error: in {}:{}() function, portNumber was not between 1 and 4",
                           class_name, func_name)
                   .c_str());
  lua_pushboolean(lua_state, Movie::IsUsingPad(port_number - 1));
  return 1;
}

}  // namespace Lua::LuaStatistics
