#pragma once
#include <string>

#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::OnFrameStartCallbackAPI
{
extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* Register(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* RegisterWithAutoDeregistration(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list);
ArgHolder* Unregister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* IsInFrameStartCallback(ScriptContext* current_script,
                                  std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::OnFrameStartCallbackAPI
