#include "LuaStatistics.h"
#include "Core/Lua/LuaVersionResolver.h"

namespace Lua::LuaStatistics
{

class Lua_Statistics
{
public:
  inline Lua_Statistics(){};
};

Lua_Statistics* lua_statistics_pointer = nullptr;

Lua_Statistics* getStatisticsInstance()
{
  if (lua_statistics_pointer == nullptr)
    lua_statistics_pointer = new Lua_Statistics();
  return lua_statistics_pointer;
}

void InitLuaStatisticsFunctions(lua_State* luaState, const std::string& luaApiVersion)
{
  Lua_Statistics** luaStatisticsPtrPtr = (Lua_Statistics**)lua_newuserdata(luaState, sizeof(Lua_Statistics*));
  *luaStatisticsPtrPtr = getStatisticsInstance();
  luaL_newmetatable(luaState, "LuaStatisticsTable");
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");

  std::array luaStatisticsFunctionsWithVersionsAttached = {
    luaL_Reg_With_Version({"isRecordingInput", "1.0", isRecordingInput}),
   luaL_Reg_With_Version({"isRecordingInputFromSaveState", "1.0", isRecordingInputFromSaveState}),
    luaL_Reg_With_Version({"isPlayingInput", "1.0", isPlayingInput}),
    luaL_Reg_With_Version({"isMovieActive", "1.0", isMovieActive}),
    luaL_Reg_With_Version({"getCurrentFrame", "1.0", getCurrentFrame}),
    luaL_Reg_With_Version({"getMovieLength", "1.0", getMovieLength}),
    luaL_Reg_With_Version({"getRerecordCount", "1.0", getRerecordCount}),
    luaL_Reg_With_Version({"getCurrentInputCount", "1.0", getCurrentInputCount}),
    luaL_Reg_With_Version({"getTotalInputCount", "1.0", getTotalInputCount}),
    luaL_Reg_With_Version({"getCurrentLagCount", "1.0", getCurrentLagCount}),
    luaL_Reg_With_Version({"getTotalLagCount", "1.0", getTotalLagCount}),
    luaL_Reg_With_Version({"isGcControllerInPort", "1.0", isGcControllerInPort}),
    luaL_Reg_With_Version({"isUsingPort", "1.0", isUsingPort})
  };

  std::unordered_map<std::string, std::string> deprecatedFunctionsMap;
  AddLatestFunctionsForVersion(luaStatisticsFunctionsWithVersionsAttached, luaApiVersion, deprecatedFunctionsMap, luaState);
  lua_setglobal(luaState, "statistics");
}

int isRecordingInput(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "isRecordingInput", "statistics:isRecordingInput()");
  lua_pushboolean(luaState, Movie::IsRecordingInput());
  return 1;
}

int isRecordingInputFromSaveState(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "isRecordingInputFromSaveState", "statistics:isRecordingInputFromSaveState()");
  lua_pushboolean(luaState, Movie::IsRecordingInputFromSaveState());
  return 1;
}

int isPlayingInput(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "isPlayingInput", "statistics:isPlayingInput()");
  lua_pushboolean(luaState, Movie::IsPlayingInput());
  return 1;
}

int isMovieActive(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "isMovieActive", "statistics:isMovieActive()");
  lua_pushboolean(luaState, Movie::IsMovieActive());
  return 1;
}

int getCurrentFrame(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "getCurrentFrame", "statistics:getCurrentFrame()");
  lua_pushinteger(luaState, Movie::GetCurrentFrame());
  return 1;
}

int getMovieLength(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "getMovieLength", "statistics:getMovieLength()");
  lua_pushinteger(luaState, Movie::GetTotalFrames());
  return 1;
}

int getRerecordCount(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "getRerecordCount", "statistics:getRerecordCount()");
  lua_pushinteger(luaState, Movie::GetRerecordCount());
  return 1;
}

int getCurrentInputCount(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "getCurrentInputCount", "statistics:getCurrentInputCount()");
  lua_pushinteger(luaState, Movie::GetCurrentInputCount());
  return 1;
}

int getTotalInputCount(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "getTotalInputCount", "statistics:getTotalInputCount()");
  lua_pushinteger(luaState, Movie::GetTotalInputCount());
  return 1;
}

int getCurrentLagCount(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "getCurrentLagCount", "statistics:getCurrentLagCount()");
  lua_pushinteger(luaState, Movie::GetCurrentLagCount());
  return 1;
}

int getTotalLagCount(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "getTotalLagCount", "statistics:getTotalLagCount()");
  lua_pushinteger(luaState, Movie::GetTotalLagCount());
  return 1;
}

int isGcControllerInPort(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "isGcControllerInPort", "statistics:isGcControllerInPort(1)");
  s64 portNumber = luaL_checkinteger(luaState, 2);
  if (portNumber < 1 || portNumber > 4)
    luaL_error(luaState, "Error: in isGcControllerInPort() function, portNumber was not between 1 and 4");
  lua_pushboolean(luaState, Movie::IsUsingGCController(portNumber - 1));
  return 1;
}

int isUsingPort(lua_State* luaState)
{
  LuaColonOperatorTypeCheck(luaState, "isUsingPort", "statistics:isUsingPort(1)");
  s64 portNumber = luaL_checkinteger(luaState, 2);
  if (portNumber < 1 || portNumber > 4)
    luaL_error(luaState, "Error: in isUsingPort() function, portNumber was not between 1 and 4");
  lua_pushboolean(luaState, Movie::IsUsingPad(portNumber - 1));
  return 1;
}

}
