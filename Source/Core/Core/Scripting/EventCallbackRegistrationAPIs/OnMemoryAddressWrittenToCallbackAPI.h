#pragma once
#include <string>

#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
{
extern const char* class_name;
extern u32 memory_address_written_to_for_current_callback;
extern u64 value_written_to_memory_address_for_current_callback;
extern u32 write_size;
extern bool in_memory_address_written_to_breakpoint;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* Register(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* RegisterWithAutoDeregistration(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list);
ArgHolder* Unregister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* IsInMemoryAddressWrittenToCallback(ScriptContext* current_script,
                                              std::vector<ArgHolder*>* arg_list);
ArgHolder* GetMemoryAddressWrittenToForCurrentCallback(ScriptContext* current_script,
                                                       std::vector<ArgHolder*>* args_list);
ArgHolder* GetValueWrittenToMemoryAddressForCurrentCallback(ScriptContext* current_script,
                                                            std::vector<ArgHolder*>* args_list);
ArgHolder* GetWriteSize(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
