#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Core/HW/GCPad.h"
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Lua/LuaHelperClasses/LuaGameCubeButtonProbabilityClasses.h"
#include "Core/Lua/LuaHelperClasses/LuaScriptCallLocations.h"
#include "Core/Movie.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

namespace Lua::LuaGameCubeController
{
extern const char* class_name;

extern std::array<bool, 4> overwrite_controller_at_specified_port;
extern std::array<bool, 4> add_to_controller_at_specified_port;
extern std::array<bool, 4> do_random_input_events_at_specified_port;

extern std::array<Movie::ControllerState, 4> new_overwrite_controller_inputs;
extern std::array<Movie::ControllerState, 4> add_to_controller_inputs;
extern std::array<std::vector<GcButtonName>, 4> button_lists_for_add_to_controller_inputs;
extern std::array<std::vector<std::unique_ptr<LuaGameCubeButtonProbabilityEvent>>, 4>
    random_button_events;

void InitLuaGameCubeControllerFunctions(lua_State* lua_state, const std::string& lua_api_version,
                                        LuaScriptCallLocations* new_script_call_location_pointer);

int SetInputs(lua_State* lua_state);
int AddInputs(lua_State* lua_state);
int AddButtonFlipChance(lua_State* lua_state);
int AddButtonPressChance(lua_State* lua_state);
int AddButtonReleaseChance(lua_State* lua_state);
int AddOrSubtractFromCurrentAnalogValueChance(lua_State* lua_state);
int AddOrSubtractFromSpecificAnalogValueChance(lua_State* lua_state);
int AddButtonComboChance(lua_State* lua_state);
int AddControllerClearChance(lua_State* lua_state);
int GetControllerInputs(lua_State* lua_state);

}  // namespace Lua::LuaGameCubeController
