#pragma once
#include <thread>
#include "src/lua.hpp"
#include "src/lua.h"
#include "src/luaconf.h"
#include "src/lapi.h"

namespace Lua
{

static lua_State* mainLuaState;
extern std::thread luaThread;


void tempRunner();

void Init();

void Shutdown();

}
