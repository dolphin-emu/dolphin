#pragma once
#pragma once
#include <string>

#include "Core/Movie.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/ScriptContext.h"
#include <vector>

namespace Scripting::StatisticsApi
{
extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();

ArgHolder IsRecordingInput(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder IsRecordingInputFromSaveState(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder IsPlayingInput(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder IsMovieActive(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder GetCurrentFrame(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetMovieLength(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetRerecordCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetCurrentInputCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetTotalInputCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetCurrentLagCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetTotalLagCount(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
}  // namespace Scripting::StatisticsApi
