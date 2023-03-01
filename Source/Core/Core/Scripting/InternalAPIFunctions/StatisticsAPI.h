#pragma once
#pragma once
#include <string>

#include "Core/Movie.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ScriptCallLocations.h"
#include <vector>

namespace Scripting::StatisticsApi
{
extern const char* class_name;

ClassMetadata GetStatisticsApiClassData(const std::string& api_version);

ArgHolder IsRecordingInput(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder IsRecordingInputFromSaveState(ScriptCallLocations call_location,
                                        std::vector<ArgHolder>& args_list);
ArgHolder IsPlayingInput(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder IsMovieActive(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);

ArgHolder GetCurrentFrame(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetMovieLength(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetRerecordCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetCurrentInputCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetTotalInputCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetCurrentLagCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetTotalLagCount(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);

ArgHolder IsGcControllerInPort(ScriptCallLocations call_location,
                               std::vector<ArgHolder>& args_list);
ArgHolder IsUsingPort(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
}  // namespace Scripting::StatisticsApi
