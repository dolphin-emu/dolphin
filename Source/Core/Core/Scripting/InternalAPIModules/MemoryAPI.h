#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::MemoryApi
{
extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* ReadU8(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadU16(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadU32(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadU64(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadS8(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadS16(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadS32(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadS64(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadFloat(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadDouble(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadFixedLengthString(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadNullTerminatedString(ScriptContext* current_script,
                                    std::vector<ArgHolder*>* args_list);
ArgHolder* ReadUnsignedBytes(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* ReadSignedBytes(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

ArgHolder* WriteU8(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteU16(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteU32(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteU64(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteS8(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteS16(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteS32(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteS64(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteFloat(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteDouble(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteString(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteBytes(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* WriteAllMemoryAsUnsignedBytesToFile(ScriptContext* current_script,
                                               std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::MemoryApi
