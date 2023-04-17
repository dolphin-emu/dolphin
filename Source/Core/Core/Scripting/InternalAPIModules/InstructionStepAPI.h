#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/ScriptContext.h"

namespace Scripting::InstructionStepAPI
{
extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);
bool IsCurrentlyInBreakpoint();

ArgHolder SingleStep(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetInstructionFromAddress(ScriptContext* current_script,
                                    std::vector<ArgHolder>& args_list);
}  // namespace Scripting::InstructionStepAPI
