#pragma once

#include <string>
#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::EmuApi
{
extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* EmuFrameAdvance(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* EmuLoadState(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* EmuSaveState(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* EmuPlayMovie(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* EmuSaveMovie(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::EmuApi
