#include "LuaEmuFunctions.h"
#include "../LuaVersionResolver.h"
#include "Core/State.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "Core/PowerPC/PowerPC.h"
#include <optional>

namespace Lua
{
namespace LuaEmu
{
  bool waitingForSaveStateLoad = false;
  bool waitingForSaveStateSave = false;
  bool waitingToStartPlayingMovie = false;
  bool waitingToSaveMovie = false;

  std::string loadStateName;
  std::string saveStateName;
  std::string moviePathName;
  std::string playMovieName;
  std::optional<std::string> blankString;
  std::string saveMovieName;

  class emu
{
public:
  inline emu() {}
};

emu* emuPointer = NULL;

emu* GetEmuInstance()
{
  if (emuPointer == NULL)
    emuPointer = new emu();
  return emuPointer;
}

void InitLuaEmuFunctions(lua_State* luaState, const std::string& luaApiVersion)
{
  waitingForSaveStateLoad = false;
  waitingForSaveStateSave = false;
  waitingToStartPlayingMovie = false;
  waitingToSaveMovie = false;
  emu** emuPtrPtr = (emu**)lua_newuserdata(luaState, sizeof(emu*));
  *emuPtrPtr = GetEmuInstance();
  luaL_newmetatable(luaState, "LuaEmuMetaTable");
  lua_pushvalue(luaState, -1);
  lua_setfield(luaState, -2, "__index");

  luaL_Reg luaEmuFunctionsWithVersionsAttached[] = {
    {"frameAdvance-VERSION-1.0", emu_frameAdvance},
    {"loadState-VERSION-1.0", emu_loadState},
    {"saveState-VERSION-1.0", emu_saveState},
    {"playMovie-VERSION-1.0", emu_playMovie},
    {"saveMovie-VERSION-1.0", emu_saveMovie},
   };

  std::vector<luaL_Reg> vectorOfFunctionsForVersion = getLatestFunctionsForVersion(luaEmuFunctionsWithVersionsAttached, 5, luaApiVersion);
  luaL_setfuncs(luaState, &vectorOfFunctionsForVersion[0], 0);
  lua_setmetatable(luaState, -2);
  lua_setglobal(luaState, "emu");
}

void StatePauseFunction()
{
  Core::SetState(Core::State::Paused);
}

int emu_frameAdvance(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "frameAdvance", "emu:frameAdvance()");
  return lua_yield(luaState, 0);
}

void convertToUpperCase(std::string& inputString)
{
  for (int i = 0; i < inputString.length(); ++i)
  {
    inputString[i] = std::toupper(inputString[i]);
  }
}

std::string checkIfFileExistsAndGetFileName(lua_State* luaState, const char* funcName)
{
  const std::string fileName = luaL_checkstring(luaState, 2);
  if (FILE* file = fopen(fileName.c_str(), "r"))
    fclose(file);
  else
    luaL_error(luaState, (std::string("Error: Filename ") + fileName + " passed into emu:" + funcName +
                             "() did not represent a file which exists.").c_str());
  return fileName;
}

int emu_loadState(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "loadState", "emu:loadState(stateFileName)");
  loadStateName = checkIfFileExistsAndGetFileName(luaState, "loadState");
  waitingForSaveStateLoad = true;
  Core::QueueHostJob([=]() { State::LoadAs(loadStateName); });
  return lua_yield(luaState, 0);
}

int emu_saveState(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "saveState", "emu:saveState(stateFileName)");
  saveStateName = luaL_checkstring(luaState, 2);
  waitingForSaveStateSave = true;
  Core::QueueHostJob([=]() { State::SaveAs(saveStateName); });
  return lua_yield(luaState, 0);
}

int emu_playMovie(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "playMovie", "emu:playMovie(movieFileName)");
  playMovieName = checkIfFileExistsAndGetFileName(luaState, "playMovie");
  waitingToStartPlayingMovie = true;
  Core::QueueHostJob([=]() {
    Movie::EndPlayInput(false);
    Movie::PlayInput(playMovieName, &blankString);
  });
  return lua_yield(luaState, 0);
}

int emu_saveMovie(lua_State* luaState)
{
  luaColonOperatorTypeCheck(luaState, "saveMovie", "emu:saveMovie(movieFileName)");
  saveMovieName = luaL_checkstring(luaState, 2);
  waitingToSaveMovie = true;
  Core::QueueHostJob([=]() {
    Movie::SaveRecording(saveMovieName);
  });
  return lua_yield(luaState, 0);
}

}  // namespace Lua_emu
}  // namespace Lua
