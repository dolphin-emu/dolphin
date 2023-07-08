#pragma once
#include <string>

#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::OnMemoryAddressReadFromCallbackAPI
{
extern const char* class_name;
extern u32 memory_address_read_from_for_current_callback;
extern u32 read_size;
extern bool in_memory_address_read_from_breakpoint;

// The following 2 variables are only used by ScriptUtilities.cpp to check which callbacks fall
// between these 2.
extern u32 start_address_for_current_breakpoint;
extern u32 end_address_for_current_breakpoint;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* Register(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* RegisterWithAutoDeregistration(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list);
ArgHolder* Unregister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* IsInMemoryAddressReadFromCallback(ScriptContext* current_script,
                                             std::vector<ArgHolder*>* args_list);
ArgHolder* GetMemoryAddressReadFromForCurrentCallback(ScriptContext* current_script,
                                                      std::vector<ArgHolder*>* args_list);
ArgHolder* GetReadSize(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::OnMemoryAddressReadFromCallbackAPI
