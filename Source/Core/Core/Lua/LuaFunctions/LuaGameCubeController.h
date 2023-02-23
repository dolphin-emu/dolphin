#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Core/HW/GCPad.h"
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Lua/LuaHelperClasses/LuaGameCubeButtonProbabilityClasses.h"
#include "Core/Movie.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

namespace Lua::LuaGameCubeController
{
extern const char* class_name;

extern std::array<bool, 4> overwrite_controller_at_specified_port;
extern std::array<Movie::ControllerState, 4> new_controller_inputs;
extern int current_controller_number_polled;

extern std::array<Movie::ControllerState, 4> controller_inputs_on_last_frame;

void InitLuaGameCubeControllerFunctions(lua_State* lua_state, const std::string& lua_api_version);

int GetCurrentPortNumberOfPoll(lua_State* lua_state);
int SetInputsForPoll(lua_State* lua_state);
int GetInputsForPoll(lua_State* lua_state);
int GetInputsForPreviousFrame(lua_State* lua_state);

}  // namespace Lua::LuaGameCubeController
