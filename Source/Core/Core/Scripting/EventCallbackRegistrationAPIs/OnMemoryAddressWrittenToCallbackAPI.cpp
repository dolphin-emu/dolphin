#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"

#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "Core/Scripting/HelperClasses/MemoryAddressBreakpointsHolder.h"

namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
{
const char* class_name = "OnMemoryAddressWrittenTo";

u32 memory_address_written_to_for_current_callback = 0;
s64 value_written_to_memory_address_for_current_callback = -1;
bool in_memory_address_written_to_breakpoint = false;

static std::array all_on_memory_address_written_to_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(memoryAddress, value)", Register,
                     ArgTypeEnum::RegistrationReturnType,
                     {ArgTypeEnum::U32, ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("registerWithAutoDeregistration", "1.0",
                     "registerWithAutoDeregistration(memoryAddress, value)",
                     RegisterWithAutoDeregistration,
                     ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType,
                     {ArgTypeEnum::U32, ArgTypeEnum::RegistrationWithAutoDeregistrationInputType}),
    FunctionMetadata("unregister", "1.0", "unregister(memoryAddress, value)", Unregister,
                     ArgTypeEnum::UnregistrationReturnType,
                     {ArgTypeEnum::U32, ArgTypeEnum::UnregistrationInputType}),
    FunctionMetadata("isInMemoryAddressWrittenToCallback", "1.0",
                     "isInMemoryAddressWrittenToCallback()", IsInMemoryAddressWrittenToCallback,
                     ArgTypeEnum::Boolean, {}),
    FunctionMetadata("getMemoryAddressWrittenToForCurrentCallback", "1.0",
                     "getMemoryAddressWrittenToForCurrentCallback()",
                     GetMemoryAddressWrittenToForCurrentCallback, ArgTypeEnum::U32, {}),
    FunctionMetadata("getValueWrittenToMemoryAddressForCurrentCallback", "1.0",
                     "getValueWrittenToMemoryAddressForCurrentCallback",
                     GetValueWrittenToMemoryAddressForCurrentCallback, ArgTypeEnum::S64, {})};

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
  u32 memory_breakpoint_address = (*args_list)[0]->u32_val;
  void* callback = (*args_list)[1]->void_pointer_val;

  if (memory_breakpoint_address == 0)
    return CreateErrorStringArgHolder("Error: Memory address breakpoint cannot be 0!");

  bool read_breakpoint_exists =
      current_script->memoryAddressBreakpointsHolder.ContainsReadBreakpoint(
          memory_breakpoint_address);

  TMemCheck check;

  check.start_address = memory_breakpoint_address;
  check.end_address = memory_breakpoint_address;
  check.is_break_on_read = read_breakpoint_exists;
  check.is_break_on_write = true;
  check.condition = {};
  check.break_on_hit = true;

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));

  current_script->memoryAddressBreakpointsHolder.AddWriteBreakpoint(memory_breakpoint_address);

  return CreateRegistrationReturnTypeArgHolder(
      current_script->dll_specific_api_definitions.RegisterOnMemoryAddressWrittenToCallback(
          current_script, memory_breakpoint_address, callback));
}

ArgHolder* RegisterWithAutoDeregistration(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list)
{
  u32 memory_breakpoint_address = (*args_list)[0]->u32_val;
  void* callback = (*args_list)[1]->void_pointer_val;

  if (memory_breakpoint_address == 0)
    return CreateErrorStringArgHolder("Error: Memory address breakpoint cannot be 0!");

  bool read_breakpoint_exists =
      current_script->memoryAddressBreakpointsHolder.ContainsReadBreakpoint(
          memory_breakpoint_address);

  TMemCheck check;

  check.start_address = memory_breakpoint_address;
  check.end_address = memory_breakpoint_address;
  check.is_break_on_read = read_breakpoint_exists;
  check.is_break_on_write = true;
  check.condition = {};
  check.break_on_hit = true;

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));

  current_script->memoryAddressBreakpointsHolder.AddWriteBreakpoint(memory_breakpoint_address);

  current_script->dll_specific_api_definitions
      .RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback(
          current_script, memory_breakpoint_address, callback);
  return CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder();
}

ArgHolder* Unregister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  u32 memory_breakpoint_address = (*args_list)[0]->u32_val;
  void* callback = (*args_list)[1]->void_pointer_val;

  if (!current_script->memoryAddressBreakpointsHolder.ContainsWriteBreakpoint(
          memory_breakpoint_address))
  {
    return CreateErrorStringArgHolder(
        "Error: Address passed into OnMemoryAddressWrittenTo:Unregister() did not represent a "
        "write breakpoint that was currently enabled!");
  }

  current_script->memoryAddressBreakpointsHolder.RemoveWriteBreakpoint(memory_breakpoint_address);

  if (current_script->memoryAddressBreakpointsHolder.ContainsReadBreakpoint(
          memory_breakpoint_address) ||
      current_script->memoryAddressBreakpointsHolder.ContainsWriteBreakpoint(
          memory_breakpoint_address))
  {
    TMemCheck check;

    check.start_address = memory_breakpoint_address;
    check.end_address = memory_breakpoint_address;
    check.is_break_on_read = current_script->memoryAddressBreakpointsHolder.ContainsReadBreakpoint(
        memory_breakpoint_address);
    check.is_break_on_write =
        current_script->memoryAddressBreakpointsHolder.ContainsWriteBreakpoint(
            memory_breakpoint_address);
    check.condition = std::nullopt;
    check.break_on_hit = true;
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));
  }
  else
  {
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Remove(memory_breakpoint_address);
  }

  bool return_value =
      current_script->dll_specific_api_definitions.UnregisterOnMemoryAddressWrittenToCallback(
          current_script, memory_breakpoint_address, callback);
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
  return CreateBoolArgHolder(current_script->current_script_call_location ==
                             ScriptCallLocations::FromMemoryAddressWrittenToCallback);
}

ArgHolder* GetMemoryAddressWrittenToForCurrentCallback(ScriptContext* current_script,
                                                       std::vector<ArgHolder*>* args_list)
{
  if (current_script->current_script_call_location !=
      ScriptCallLocations::FromMemoryAddressWrittenToCallback)
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
      ScriptCallLocations::FromMemoryAddressWrittenToCallback)
    return CreateErrorStringArgHolder(
        "User attempted to call "
        "OnMemoryAddressWrittenTo:getValueWrittenToMemoryAddressForCurrentCallback() outside of an "
        "OnMemoryAddressWrittenTo callback function!");
  return CreateS64ArgHolder(value_written_to_memory_address_for_current_callback);
}
}  // namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
