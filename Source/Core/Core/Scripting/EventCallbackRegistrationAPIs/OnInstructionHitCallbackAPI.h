#pragma once
#include <string>

#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::OnInstructionHitCallbackAPI
{
extern const char* class_name;
extern u32 instruction_address_for_current_callback;
extern bool in_instruction_hit_breakpoint;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* Register(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* RegisterWithAutoDeregistration(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list);
ArgHolder* Unregister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* IsInInstructionHitCallback(ScriptContext* current_script,
                                      std::vector<ArgHolder*>* args_list);
ArgHolder* GetAddressOfInstructionForCurrentCallback(ScriptContext* current_script,
                                                     std::vector<ArgHolder*>* args_list);
}  // namespace Scripting::OnInstructionHitCallbackAPI
