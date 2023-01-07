#include "LuaStatistics.h"

namespace Lua
{
namespace LuaStatistics
{

class Lua_Statistics
{
public:
  inline Lua_Statistics(){};
};

Lua_Statistics* lua_statistics_pointer;

Lua_Statistics* getStatisticsInstance()
{
  if (lua_statistics_pointer == NULL)
    lua_statistics_pointer = new Lua_Statistics();
  return lua_statistics_pointer;
}

void InitLuaStatisticsFunctions(lua_State* luaState)
{
  Lua_Statistics** luaStatisticsPtrPtr = (Lua_Statistics**)lua_newuserdata(luaState, sizeof(Lua_Statistics*));
  *luaStatisticsPtrPtr = getStatisticsInstance();
  luaL_newmetatable(luaState, "LuaStatisticsTable");
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");

  luaL_Reg luaStatisticsFunctions[] = {
    {"isRecordingInput", isRecordingInput},
    {"isRecordingInputFromSaveState", isRecordingInputFromSaveState},
    {"isPlayingInput", isPlayingInput},
    {"isMovieActive", isMovieActive},
    {"getCurrentFrame", getCurrentFrame},
    {"getMovieLegnth", getMovieLength},
    {"getRerecordCount", getRerecordCount},
    {"getCurrentInputCount", getCurrentInputCount},
    {"getTotalInputCount", getTotalInputCount},
    {"getCurrentLagCount",  getCurrentLagCount},
    {"getTotalLagCount", getTotalLagCount},
    {"isGcControllerInPort", isGcControllerInPort},
    {"isUsingPort", isUsingPort},
    {nullptr, nullptr}};

  luaL_setfuncs(luaState, luaStatisticsFunctions, 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, "statistics");
}

int isRecordingInput(lua_State* luaState)
{
  lua_pushboolean(luaState, Movie::IsRecordingInput());
  return 1;
}

int isRecordingInputFromSaveState(lua_State* luaState)
{
  lua_pushboolean(luaState, Movie::IsRecordingInputFromSaveState());
  return 1;
}

int isPlayingInput(lua_State* luaState)
{
  lua_pushboolean(luaState, Movie::IsPlayingInput());
  return 1;
}

int isMovieActive(lua_State* luaState)
{
  lua_pushboolean(luaState, Movie::IsMovieActive());
  return 1;
}

int getCurrentFrame(lua_State* luaState)
{
  lua_pushinteger(luaState, Movie::GetCurrentFrame());
  return 1;
}

int getMovieLength(lua_State* luaState)
{
  lua_pushinteger(luaState, Movie::GetTotalFrames());
  return 1;
}

int getRerecordCount(lua_State* luaState)
{
  lua_pushinteger(luaState, Movie::GetRerecordCount());
  return 1;
}

int getCurrentInputCount(lua_State* luaState)
{
  lua_pushinteger(luaState, Movie::GetCurrentInputCount());
  return 1;
}

int getTotalInputCount(lua_State* luaState)
{
  lua_pushinteger(luaState, Movie::GetTotalInputCount());
  return 1;
}

int getCurrentLagCount(lua_State* luaState)
{
  lua_pushinteger(luaState, Movie::GetCurrentLagCount());
  return 1;
}

int getTotalLagCount(lua_State* luaState)
{
  lua_pushinteger(luaState, Movie::GetTotalLagCount());
  return 1;
}

int isGcControllerInPort(lua_State* luaState)
{
  s64 portNumber = luaL_checkinteger(luaState, 2);
  if (portNumber < 1 || portNumber > 4)
    luaL_error(luaState, "Error: in isGcControllerInPort() function, portNumber was not between 1 and 4");
  lua_pushboolean(luaState, Movie::IsUsingGCController(portNumber - 1));
  return 1;
}

int isUsingPort(lua_State* luaState)
{
  s64 portNumber = luaL_checkinteger(luaState, 2);
  if (portNumber < 1 || portNumber > 4)
    luaL_error(luaState, "Error: in isUsingPort() function, portNumber was not between 1 and 4");
  lua_pushboolean(luaState, Movie::IsUsingPad(portNumber - 1));
  return 1;
}

}
}
