#pragma once
#include <string>
#include <vector>
#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"

namespace Scripting::BitApi
{
extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* BitwiseAnd(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* BitwiseOr(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* BitwiseNot(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* BitwiseXor(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* LogicalAnd(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* LogicalOr(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* LogicalXor(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* LogicalNot(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* BitShiftLeft(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* BitShiftRight(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::BitApi
