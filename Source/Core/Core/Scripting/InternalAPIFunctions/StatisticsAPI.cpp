#include "Core/Scripting/InternalAPIFunctions/StatisticsAPI.h"

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

ArgHolder IsRecordingInput(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateBoolArgHolder(Movie::IsRecordingInput());
}

ArgHolder IsRecordingInputFromSaveState(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateBoolArgHolder(Movie::IsRecordingInputFromSaveState());
}

ArgHolder IsPlayingInput(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateBoolArgHolder(Movie::IsPlayingInput());
}

ArgHolder IsMovieActive(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateBoolArgHolder(Movie::IsMovieActive());
}

ArgHolder GetCurrentFrame(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetCurrentFrame());
}

ArgHolder GetMovieLength(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetTotalFrames());
}

ArgHolder GetRerecordCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetRerecordCount());
}

ArgHolder GetCurrentInputCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetCurrentInputCount());
}

ArgHolder GetTotalInputCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetTotalInputCount());
}

ArgHolder GetCurrentLagCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetCurrentLagCount());
}

ArgHolder GetTotalLagCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(Movie::GetTotalLagCount());
}

ArgHolder IsGcControllerInPort(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  long long controller_port_number = args_list[0].long_long_val;
  
  if (controller_port_number < 1 || controller_port_number > 4)
    return CreateErrorStringArgHolder("controller port number was outside the valid range of 1-4");

  return CreateBoolArgHolder(Movie::IsUsingGCController(controller_port_number - 1));
}

ArgHolder IsUsingPort(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list)
{
  long long controller_port_number = args_list[0].long_long_val;
  if (controller_port_number < 1 || controller_port_number > 4)
    return CreateErrorStringArgHolder("controller port number was outside the valid range of 1-4");

  return CreateBoolArgHolder(Movie::IsUsingPad(controller_port_number - 1));
}

}  // namespace Scripting::StatisticsApi
