#pragma once
#include <string>
#include <vector>
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::InstructionStepAPI
{
extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* SingleStep(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* StepOver(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* StepOut(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* Skip(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* SetPC(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetInstructionFromAddress(ScriptContext* current_script,
                                     std::vector<ArgHolder*>* args_list);
}  // namespace Scripting::InstructionStepAPI
