#include "Core/Scripting/InternalAPIModules/GameCubeControllerAPI.h"

#include <fmt/format.h>
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::GameCubeControllerApi
{
const char* class_name = "GameCubeControllerAPI";
static std::array all_game_cube_controller_functions_metadata_list = {
    FunctionMetadata("getCurrentPortNumberOfPoll", "1.0", "getCurrentPortNumberOfPoll()",
                     GetCurrentPortNumberOfPoll, ArgTypeEnum::LongLong, {}),
    FunctionMetadata("setInputsForPoll", "1.0", "setInputsForPoll(controllerValuesTable)", SetInputsForPoll,
                     ArgTypeEnum::VoidType, {ArgTypeEnum::ControllerStateObject}),
    FunctionMetadata("getInputsForPoll", "1.0", "getInputsForPoll()", GetInputsForPoll,
                     ArgTypeEnum::ControllerStateObject, {}),
    FunctionMetadata("getInputsForPreviousFrame", "1.0", "getInputsForPreviousFrame(1)",
                     GetInputsForPreviousFrame, ArgTypeEnum::ControllerStateObject, {ArgTypeEnum::LongLong}),
    FunctionMetadata("isGcControllerInPort", "1.0", "isGcControllerInPort(1)", IsGcControllerInPort,
                     ArgTypeEnum::Boolean, {ArgTypeEnum::LongLong}),
    FunctionMetadata("isUsingPort", "1.0", "isUsingPort(1)", IsUsingPort, ArgTypeEnum::Boolean,
                     {ArgTypeEnum::LongLong})};

std::array<bool, 4> overwrite_controller_at_specified_port{};
std::array<Movie::ControllerState, 4> new_controller_inputs{};
std::array<Movie::ControllerState, 4> controller_inputs_on_last_frame{};
int current_controller_number_polled = -1;

ClassMetadata GetGameCubeControllerApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_game_cube_controller_functions_metadata_list, api_version, deprecated_functions_map)};
}

ArgHolder
GetCurrentPortNumberOfPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(current_controller_number_polled + 1);
}

// NOTE: In SI.cpp, UpdateDevices() is called to update each device, which moves exactly 8 bytes
// forward for each controller. Also, it moves in order from controllers 1 to 4.
ArgHolder SetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  overwrite_controller_at_specified_port[current_controller_number_polled] = true;
  new_controller_inputs[current_controller_number_polled] = args_list[0].controller_state_val;
  return CreateVoidTypeArgHolder();
}

ArgHolder GetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateControllerStateArgHolder(new_controller_inputs[current_controller_number_polled]);
}

ArgHolder GetInputsForPreviousFrame(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  s64 controller_port_number = args_list[0].long_long_val;
  if (controller_port_number < 1 || controller_port_number > 4)
  {
    return CreateErrorStringArgHolder("controller port number was not between 1 and 4 !");
  }

  return CreateControllerStateArgHolder(controller_inputs_on_last_frame[controller_port_number - 1]);
}

ArgHolder IsGcControllerInPort(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  long long controller_port_number = args_list[0].long_long_val;

  if (controller_port_number < 1 || controller_port_number > 4)
    return CreateErrorStringArgHolder("controller port number was outside the valid range of 1-4");

  return CreateBoolArgHolder(Movie::IsUsingGCController(controller_port_number - 1));
}

ArgHolder IsUsingPort(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  long long controller_port_number = args_list[0].long_long_val;
  if (controller_port_number < 1 || controller_port_number > 4)
    return CreateErrorStringArgHolder("controller port number was outside the valid range of 1-4");

  return CreateBoolArgHolder(Movie::IsUsingPad(controller_port_number - 1));
}

}  // namespace Scripting::GameCubeControllerAPI
