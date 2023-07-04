#include "Core/Scripting/InternalAPIModules/GameCubeControllerAPI.h"

#include <fmt/format.h>
#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::GameCubeControllerApi
{
const char* class_name = "GameCubeControllerAPI";
std::array<Movie::ControllerState, 4> controller_inputs_on_last_frame{};

static std::array all_game_cube_controller_functions_metadata_list = {
    FunctionMetadata("getInputsForPreviousFrame", "1.0", "getInputsForPreviousFrame(1)",
                     GetInputsForPreviousFrame, ScriptingEnums::ArgTypeEnum::ControllerStateObject,
                     {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("isGcControllerInPort", "1.0", "isGcControllerInPort(1)", IsGcControllerInPort,
                     ScriptingEnums::ArgTypeEnum::Boolean, {ScriptingEnums::ArgTypeEnum::S64}),
    FunctionMetadata("isUsingPort", "1.0", "isUsingPort(1)", IsUsingPort,
                     ScriptingEnums::ArgTypeEnum::Boolean, {ScriptingEnums::ArgTypeEnum::S64})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_game_cube_controller_functions_metadata_list,
                                                   api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_game_cube_controller_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_game_cube_controller_functions_metadata_list, api_version,
                               function_name, deprecated_functions_map);
}

ArgHolder* GetInputsForPreviousFrame(ScriptContext* current_script,
                                     std::vector<ArgHolder*>* args_list)
{
  s64 controller_port_number = (*args_list)[0]->s64_val;
  if (controller_port_number < 1 || controller_port_number > 4)
  {
    return CreateErrorStringArgHolder("controller port number was not between 1 and 4 !");
  }

  return CreateControllerStateArgHolder(
      controller_inputs_on_last_frame[controller_port_number - 1]);
}

ArgHolder* IsGcControllerInPort(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long controller_port_number = (*args_list)[0]->s64_val;

  if (controller_port_number < 1 || controller_port_number > 4)
    return CreateErrorStringArgHolder("controller port number was outside the valid range of 1-4");

  return CreateBoolArgHolder(Movie::IsUsingGCController(controller_port_number - 1));
}

ArgHolder* IsUsingPort(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  long long controller_port_number = (*args_list)[0]->s64_val;
  if (controller_port_number < 1 || controller_port_number > 4)
    return CreateErrorStringArgHolder("controller port number was outside the valid range of 1-4");

  return CreateBoolArgHolder(Movie::IsUsingPad(controller_port_number - 1));
}

}  // namespace Scripting::GameCubeControllerApi
