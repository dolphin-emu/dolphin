#pragma once

#include <lua.hpp>

#include <string>
#include <functional>
#include <vector>
#include <array>

namespace Lua
{
extern std::string GLOBAL_LUA_API_VERSION_VARIABLE;
extern lua_State* mainLuaState;
extern lua_State* mainLuaThreadState;
extern int* x;
extern bool luaScriptActive;
extern bool luaInitialized;
extern std::function<void(const std::string&)>* printCallbackFunction;
extern std::function<void()>* scriptEndCallbackFunction;

int setLuaCoreVersion(lua_State* luaState);
int getLuaCoreVersion(lua_State* luaState);

void Init(const std::string& scriptLocation,
          std::function<void(const std::string&)>* newPrintCallback,
          std::function<void()>* newScriptEndCallback);

void StopScript();

}
