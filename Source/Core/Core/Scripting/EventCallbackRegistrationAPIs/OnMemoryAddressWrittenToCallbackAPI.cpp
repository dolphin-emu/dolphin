#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"

#include "Core/Scripting/HelperClasses/MemoryAddressBreakpointsHolder.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
{
const char* class_name = "OnMemoryAddressWrittenTo";

u32 memory_address_written_to_for_current_callback = 0;
u64 value_written_to_memory_address_for_current_callback = 0;
u32 write_size = 0;
bool in_memory_address_written_to_breakpoint = false;

static std::array all_on_memory_address_written_to_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(memoryStartAddress, memoryEndAddress, value)",
                     Register, ScriptingEnums::ArgTypeEnum::RegistrationReturnType,
                     {ScriptingEnums::ArgTypeEnum::U32, ScriptingEnums::ArgTypeEnum::U32,
                      ScriptingEnums::ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("registerWithAutoDeregistration", "1.0",
                     "registerWithAutoDeregistration(memoryStartAddress, memoryEndAddress, value)",
                     RegisterWithAutoDeregistration,
                     ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType,
                     {ScriptingEnums::ArgTypeEnum::U32, ScriptingEnums::ArgTypeEnum::U32,
                      ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationInputType}),
    FunctionMetadata(
        "unregister", "1.0", "unregister(memoryStartAddress, value)", Unregister,
        ScriptingEnums::ArgTypeEnum::UnregistrationReturnType,
        {ScriptingEnums::ArgTypeEnum::U32, ScriptingEnums::ArgTypeEnum::UnregistrationInputType}),
    FunctionMetadata("isInMemoryAddressWrittenToCallback", "1.0",
                     "isInMemoryAddressWrittenToCallback()", IsInMemoryAddressWrittenToCallback,
                     ScriptingEnums::ArgTypeEnum::Boolean, {}),
    FunctionMetadata("getMemoryAddressWrittenToForCurrentCallback", "1.0",
                     "getMemoryAddressWrittenToForCurrentCallback()",
                     GetMemoryAddressWrittenToForCurrentCallback, ScriptingEnums::ArgTypeEnum::U32,
                     {}),
    FunctionMetadata("getValueWrittenToMemoryAddressForCurrentCallback", "1.0",
                     "getValueWrittenToMemoryAddressForCurrentCallback()",
                     GetValueWrittenToMemoryAddressForCurrentCallback,
                     ScriptingEnums::ArgTypeEnum::U64, {}),
    FunctionMetadata("getWriteSize", "1.0", "getWriteSize()", GetWriteSize,
                     ScriptingEnums::ArgTypeEnum::U32, {})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(
                          all_on_memory_address_written_to_callback_functions_metadata_list,
                          api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name,
          GetAllFunctions(all_on_memory_address_written_to_callback_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_on_memory_address_written_to_callback_functions_metadata_list,
                               api_version, function_name, deprecated_functions_map);
}

ArgHolder* Register(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  u32 memory_breakpoint_start_address = (*args_list)[0]->u32_val;
  u32 memory_breakpoint_end_address = (*args_list)[1]->u32_val;
  void* callback = (*args_list)[2]->void_pointer_val;

  if (memory_breakpoint_start_address == 0)
    return CreateErrorStringArgHolder("Error: Memory address write breakpoint cannot be 0!");
  else if (memory_breakpoint_start_address > memory_breakpoint_end_address)
    return CreateErrorStringArgHolder(
        "Error: Memory address write breakpoint has an ending address less "
        "than its starting address!");

  current_script->memoryAddressBreakpointsHolder.AddWriteBreakpoint(memory_breakpoint_start_address,
                                                                    memory_breakpoint_end_address);

  return CreateRegistrationReturnTypeArgHolder(
      current_script->dll_specific_api_definitions.RegisterOnMemoryAddressWrittenToCallback(
          current_script, memory_breakpoint_start_address, memory_breakpoint_end_address,
          callback));
}

ArgHolder* RegisterWithAutoDeregistration(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list)
{
  u32 memory_breakpoint_start_address = (*args_list)[0]->u32_val;
  u32 memory_breakpoint_end_address = (*args_list)[1]->u32_val;
  void* callback = (*args_list)[2]->void_pointer_val;

  if (memory_breakpoint_start_address == 0)
    return CreateErrorStringArgHolder("Error: Memory address write breakpoint cannot be 0!");
  else if (memory_breakpoint_start_address > memory_breakpoint_end_address)
    return CreateErrorStringArgHolder("Error: Memory address write breakpoint has an ending "
                                      "address less than its starting address!");

  current_script->memoryAddressBreakpointsHolder.AddWriteBreakpoint(memory_breakpoint_start_address,
                                                                    memory_breakpoint_end_address);

  current_script->dll_specific_api_definitions
      .RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback(
          current_script, memory_breakpoint_start_address, memory_breakpoint_end_address, callback);
  return CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder();
}

ArgHolder* Unregister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  u32 memory_breakpoint_start_address = (*args_list)[0]->u32_val;
  void* callback = (*args_list)[1]->void_pointer_val;

  if (!current_script->memoryAddressBreakpointsHolder.ContainsWriteBreakpoint(
          memory_breakpoint_start_address))
  {
    return CreateErrorStringArgHolder(
        "Error: Address passed into OnMemoryAddressWrittenTo:Unregister() did not represent a "
        "write breakpoint that was currently enabled!");
  }

  current_script->memoryAddressBreakpointsHolder.RemoveWriteBreakpoint(
      memory_breakpoint_start_address);

  bool return_value =
      current_script->dll_specific_api_definitions.UnregisterOnMemoryAddressWrittenToCallback(
          current_script, callback);
  if (!return_value)
    return CreateErrorStringArgHolder(
        "2nd argument passed into OnMemoryAddressWrittenTo:unregister() was not a reference to a "
        "function currently registered as an OnMemoryAddressWrittenTo callback!");
  else
    return CreateUnregistrationReturnTypeArgHolder(nullptr);
}

ArgHolder* IsInMemoryAddressWrittenToCallback(ScriptContext* current_script,
                                              std::vector<ArgHolder*>* arg_list)
{
  return CreateBoolArgHolder(
      current_script->current_script_call_location ==
      ScriptingEnums::ScriptCallLocations::FromMemoryAddressWrittenToCallback);
}

ArgHolder* GetMemoryAddressWrittenToForCurrentCallback(ScriptContext* current_script,
                                                       std::vector<ArgHolder*>* args_list)
{
  if (current_script->current_script_call_location !=
      ScriptingEnums::ScriptCallLocations::FromMemoryAddressWrittenToCallback)
    return CreateErrorStringArgHolder(
        "User attempted to call "
        "OnMemoryAddressWrittenTo:getMemoryAddressWrittenToForCurrentCallback() outside of an "
        "OnMemoryAddressWrittenTo callback function!");
  return CreateU32ArgHolder(memory_address_written_to_for_current_callback);
}

ArgHolder* GetValueWrittenToMemoryAddressForCurrentCallback(ScriptContext* current_script,
                                                            std::vector<ArgHolder*>* args_list)
{
  if (current_script->current_script_call_location !=
      ScriptingEnums::ScriptCallLocations::FromMemoryAddressWrittenToCallback)
    return CreateErrorStringArgHolder(
        "User attempted to call "
        "OnMemoryAddressWrittenTo:getValueWrittenToMemoryAddressForCurrentCallback() outside of an "
        "OnMemoryAddressWrittenTo callback function!");
  return CreateU64ArgHolder(value_written_to_memory_address_for_current_callback);
}

ArgHolder* GetWriteSize(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  if (current_script->current_script_call_location !=
      ScriptingEnums::ScriptCallLocations::FromMemoryAddressWrittenToCallback)
    return CreateErrorStringArgHolder("User attempted to call "
                                      "OnMemoryAddressWrittenTo:getWriteSize() outside of an "
                                      "OnMemoryAddressWrittenTo callback function!");
  return CreateU32ArgHolder(write_size);
}

}  // namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
