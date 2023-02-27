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
#include "Core/Scripting/HelperClasses/ScriptCallLocations.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"

namespace Scripting::GameCubeControllerApi
{
extern const char* class_name;

extern std::array<bool, 4> overwrite_controller_at_specified_port;
extern std::array<Movie::ControllerState, 4> new_controller_inputs;
extern std::array<Movie::ControllerState, 4> controller_inputs_on_last_frame;
extern int current_controller_number_polled;

ClassMetadata GetGameCubeControllerApiClassData(const std::string& api_version);
ArgHolder GetCurrentPortNumberOfPoll(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder SetInputsForPoll(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForPoll(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForPreviousFrame(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);

}  // namespace Lua::LuaGameCubeController
