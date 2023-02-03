#pragma once

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

#include <functional>

namespace Lua
{

extern lua_State* mainLuaState;
extern lua_State* mainLuaThreadState;
extern int* x;
extern bool luaScriptActive;
extern bool luaInitialized;
extern std::function<void(const std::string&)>* printCallbackFunction;
extern std::function<void()>* scriptEndCallbackFunction;

void tempRunner();

void Init(const std::string& scriptLocation, std::function<void(const std::string&)>* newPrintCallback, std::function<void()>* newScriptEndCallback);

void StopScript();

}
