#pragma once
#include <thread>
#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"

namespace Lua
{

extern lua_State* mainLuaState;
extern lua_State* mainLuaThreadState;
extern std::thread luaThread;
extern int* x;
extern bool luaScriptActive;
extern bool luaInitialized;

void tempRunner();

void Init();

void Shutdown();

}
