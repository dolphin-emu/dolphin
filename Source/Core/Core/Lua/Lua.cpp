#include "Lua.h"

#include <mutex>
#include <condition_variable>
#include <thread>

#include "LuaFunctions/LuaRamFunctions.h"
#include "LuaFunctions/LuaEmuFunctions.h"
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
  Lua_emu::luaScriptActive = true;
  mainLuaState = luaL_newstate();
  luaL_openlibs(mainLuaState);
  Lua_RAM::InitLuaRAMFunctions(mainLuaState);
  Lua_emu::InitLuaEmuFunctions(mainLuaState);
  luaThread = std::thread(tempRunner);
}

void Shutdown()
{
  luaThread.join();
}
}
