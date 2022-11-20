#include "Lua.h"

#include <mutex>
#include <condition_variable>
#include <thread>

#include "LuaFunctions/LuaMemoryFunctions/LuaRamFunctions.h"
#include "LuaFunctions/LuaEmuFunctions.h"
#include "LuaFunctions/LuaBitFunctions.h"
#include "LuaFunctions/LuaConverter.h"
#include "Core/Movie.h"
#include <filesystem>

namespace Lua
{
std::thread luaThread;

void tempRunner()
{
  std::filesystem::path p = std::filesystem::current_path().filename();
  while (Movie::GetCurrentFrame() < 5)
    ;
  luaL_dofile(mainLuaState, "script.lua");
}

void Init()
{
  LuaEmu::luaScriptActive = true;
  mainLuaState = luaL_newstate();
  luaL_openlibs(mainLuaState);
  LuaRamFunctions::InitLuaRamFunctions(mainLuaState);
  LuaEmu::InitLuaEmuFunctions(mainLuaState);
  LuaBit::InitLuaBitFunctions(mainLuaState);
  LuaConverter::InitLuaConverterFunctions(mainLuaState);
  luaThread = std::thread(tempRunner);
}

void Shutdown()
{
  luaThread.join();
}
}
