#pragma once
#include <string>

#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::ImportAPI
{
extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* ImportCommon(ScriptContext* current_script, std::string api_name,
                        std::string version_number);

ArgHolder* ImportModule(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

ArgHolder* ImportAlt(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ShutdownScript(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ExitDolphin(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::ImportAPI
