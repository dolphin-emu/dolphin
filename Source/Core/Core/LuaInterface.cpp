// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/MsgHandler.h"

#include "Core/LuaInterface.h"

#include "lua.hpp"



namespace Lua
{
template<class T>
T square(T x)
{
	return x * x;
}

void Init()
{
	int status, result;
	lua_State *L;

	// All Lua contexts are held in this structure. We work with it almost all the time.
	L = luaL_newstate();

	if (!L)
		return;

	luaL_openlibs(L); // Load Lua libraries

	// Load the file containing the script we are going to run 
	status = luaL_loadfile(L, "script.lua");
	if (status) {
		// If something went wrong, error message is at the top of the stack
		PanicAlert("Couldn't load file: %s\n", lua_tostring(L, -1));
		return;
	}

	/* Ask Lua to run our little script */
	result = lua_pcall(L, 0, LUA_MULTRET, 0);
	if (result) {
		PanicAlert("Failed to run script: %s\n", lua_tostring(L, -1));
		return;
	}

	static const char* boolDecode[] = { "false", "true" };

	// Get the returned value at the top of the stack (index -1) */
	if (lua_isstring(L, -1))
		SuccessAlert("Script returned string: %s\n", lua_tostring(L, -1));
	else if(lua_isboolean(L, -1))
		SuccessAlert("Script returned %s\n", boolDecode[lua_toboolean(L, -1)]);
	else
		SuccessAlert("Script returned: %.0f\n", lua_tonumber(L, -1));

	lua_pop(L, 1);  // Take the returned value out of the stack
	lua_close(L);   // Cya, Lua
}
} // Lua

API_EXPORT int RandomFunction(int x) {
	return Lua::square(x);
}
