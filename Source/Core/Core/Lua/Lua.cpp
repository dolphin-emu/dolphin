#include "Lua.h"

#include <mutex>
#include <condition_variable>
#include <thread>

#include "LuaFunctions/LuaMemoryApi.h"
#include "LuaFunctions/LuaEmuFunctions.h"
#include "LuaFunctions/LuaBitFunctions.h"
#include "LuaFunctions/LuaGameCubeController.h"
#include "LuaFunctions/LuaRegisters.h"
#include "LuaFunctions/LuaStatistics.h"
#include "Core/Movie.h"
#include <filesystem>

namespace Lua
{
std::thread luaThread;
lua_State* mainLuaState = NULL;
lua_State* mainLuaThreadState = NULL;
bool luaScriptActive = false;
int* x = NULL;
bool luaInitialized = false;

void Init()
{
  std::srand(time(NULL));
  x = new int;
  *x = 0;
  luaScriptActive = true;
  luaInitialized = true;
  mainLuaState = luaL_newstate();
  luaL_openlibs(mainLuaState);
  LuaMemoryApi::InitLuaMemoryApi(mainLuaState);
  LuaEmu::InitLuaEmuFunctions(mainLuaState);
  LuaBit::InitLuaBitFunctions(mainLuaState);
  LuaGameCubeController::InitLuaGameCubeControllerFunctions(mainLuaState);
  LuaStatistics::InitLuaStatisticsFunctions(mainLuaState);
  LuaRegisters::InitLuaRegistersFunctions(mainLuaState);
  lua_gc(mainLuaState, LUA_GCSTOP);
  mainLuaThreadState = lua_newthread(mainLuaState);
  std::filesystem::path p = std::filesystem::current_path().filename();
  if (luaL_loadfile(mainLuaThreadState, "LuaExamplesAndTests/LuaRegistersTest.lua") != LUA_OK)
  {
    const char* tempString = lua_tostring(mainLuaThreadState, -1);
    fprintf(stderr, "%s\n", tempString);
  }
  return;
}

void Shutdown()
{
  luaThread.join();
}
}
