#include "Core/Scripting/InternalAPIModules/StatisticsAPI.h"

#include <fmt/format.h>
#include <memory>
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::StatisticsApi
{
const char* class_name = "StatisticsAPI";

static std::array all_statistics_functions_metadata_list = {
  FunctionMetadata("isRecordingInput", "1.0", "isRecordingInput()", IsRecordingInput, ArgTypeEnum::Boolean, {}),
  FunctionMetadata("isRecordingInputFromSaveState", "1.0", "isRecordingInputFromSaveState()", IsRecordingInputFromSaveState, ArgTypeEnum::Boolean, {}),
  FunctionMetadata("isPlayingInput", "1.0", "isPlayingInput()", IsPlayingInput, ArgTypeEnum::Boolean, {}),
  FunctionMetadata("isMovieActive", "1.0", "isMovieActive()", IsMovieActive, ArgTypeEnum::Boolean, {}),
  FunctionMetadata("getCurrentFrame", "1.0", "getCurrentFrame()", GetCurrentFrame, ArgTypeEnum::LongLong, {}),
  FunctionMetadata("getMovieLength", "1.0", "getMovieLength()", GetMovieLength, ArgTypeEnum::LongLong, {}),
  FunctionMetadata("getRerecordCount", "1.0", "getRerecordCount()", GetRerecordCount, ArgTypeEnum::LongLong, {}),
  FunctionMetadata("getCurrentInputCount", "1.0", "getCurrentInputCount()", GetCurrentInputCount, ArgTypeEnum::LongLong, {}),
  FunctionMetadata("getTotalInputCount", "1.0", "getTotalInputCount()", GetTotalInputCount, ArgTypeEnum::LongLong, {}),
  FunctionMetadata("getCurrentLagCount", "1.0", "getCurrentlagCount()", GetCurrentLagCount, ArgTypeEnum::LongLong, {}),
  FunctionMetadata("getTotalLagCount", "1.0", "getTotalLagCount()", GetTotalLagCount, ArgTypeEnum::LongLong, {}),
  FunctionMetadata("isGcControllerInPort", "1.0", "isGcControllerInPort(1)", IsGcControllerInPort, ArgTypeEnum::Boolean, {ArgTypeEnum::LongLong}),
  FunctionMetadata("isUsingPort", "1.0", "isUsingPort(1)", IsUsingPort, ArgTypeEnum::Boolean, {ArgTypeEnum::LongLong})
};

 ClassMetadata GetStatisticsApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_statistics_functions_metadata_list, api_version, deprecated_functions_map)};
}

ArgHolder IsRecordingInput(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateBoolArgHolder(Movie::IsRecordingInput());
}

ArgHolder IsRecordingInputFromSaveState(ScriptContext* current_script,
                                        std::vector<ArgHolder>& args_list)
{
  return CreateBoolArgHolder(Movie::IsRecordingInputFromSaveState());
}

ArgHolder IsPlayingInput(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateBoolArgHolder(Movie::IsPlayingInput());
}

ArgHolder IsMovieActive(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateBoolArgHolder(Movie::IsMovieActive());
}

ArgHolder GetCurrentFrame(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetCurrentFrame());
}

ArgHolder GetMovieLength(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetTotalFrames());
}

ArgHolder GetRerecordCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetRerecordCount());
}

ArgHolder GetCurrentInputCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetCurrentInputCount());
}

ArgHolder GetTotalInputCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetTotalInputCount());
}

ArgHolder GetCurrentLagCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetCurrentLagCount());
}

ArgHolder GetTotalLagCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetTotalLagCount());
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

}  // namespace Scripting::StatisticsApi
