#pragma once
#include <string>
#include <vector>

#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::RegistersAPI
{

extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* GetU8FromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetU16FromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetU32FromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetU64FromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetS8FromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetS16FromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetS32FromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetS64FromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetFloatFromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetDoubleFromRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetUnsignedBytesFromRegister(ScriptContext* current_script,
                                        std::vector<ArgHolder*>* args_list);
ArgHolder* GetSignedBytesFromRegister(ScriptContext* current_script,
                                      std::vector<ArgHolder*>* args_list);

ArgHolder* WriteU8ToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteU16ToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteU32ToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteU64ToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteS8ToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteS16ToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteS32ToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteS64ToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteFloatToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteDoubleToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteBytesToRegister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::RegistersAPI
