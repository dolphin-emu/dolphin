#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace Scripting::OnMemoryAddressReadFromCallbackAPI
{
const char* class_name = "OnMemoryAddressReadFrom";
u32 memory_address_read_from_for_current_callback = 0;
bool in_memory_address_read_from_breakpoint = false;

static std::array all_on_memory_address_read_from_callback_functions_metadata_list = {
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
    FunctionMetadata("isInMemoryAddressReadFromCallback", "1.0",
                     "isInMemoryAddressReadFromCallback()", IsInMemoryAddressReadFromCallback,
                     ArgTypeEnum::Boolean, {}),
    FunctionMetadata("getMemoryAddressReadFromForCurrentCallback", "1.0",
                     "getMemoryAddressReadFromForCurrentCallback()",
                     GetMemoryAddressReadFromForCurrentCallback, ArgTypeEnum::U32, {})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(
                          all_on_memory_address_read_from_callback_functions_metadata_list,
                          api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name,
          GetAllFunctions(all_on_memory_address_read_from_callback_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_on_memory_address_read_from_callback_functions_metadata_list,
                               api_version, function_name, deprecated_functions_map);
}

ArgHolder Register(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  u32 memory_breakpoint_address = args_list[0].u32_val;
  void* callback = args_list[1].void_pointer_val;

  if (memory_breakpoint_address == 0)
    return CreateErrorStringArgHolder("Error: Memory address breakpoint cannot be 0!");

  int write_breakpoint_exists = current_script->memoryAddressBreakpointsHolder.ContainsWriteBreakpoint(&(current_script->memoryAddressBreakpointsHolder), memory_breakpoint_address);

  TMemCheck check;

  check.start_address = memory_breakpoint_address;
  check.end_address = memory_breakpoint_address;
  check.is_break_on_read = true;
  check.is_break_on_write = write_breakpoint_exists;
  check.condition = std::nullopt;
  check.break_on_hit = true;

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));
  current_script->memoryAddressBreakpointsHolder.AddReadBreakpoint(&(current_script->memoryAddressBreakpointsHolder), memory_breakpoint_address);
  return CreateRegistrationReturnTypeArgHolder(
      current_script->scriptContextBaseFunctionsTable.RegisterOnMemoryAddressReadFromCallbacks(current_script, memory_breakpoint_address, callback));
}

ArgHolder RegisterWithAutoDeregistration(ScriptContext* current_script,
                                         std::vector<ArgHolder>& args_list)
{
  u32 memory_breakpoint_address = args_list[0].u32_val;
  void* callback = args_list[1].void_pointer_val;

  if (memory_breakpoint_address == 0)
    return CreateErrorStringArgHolder("Error: Memory address breakpoint cannot be 0!");

  int write_breakpoint_exists =
      current_script->memoryAddressBreakpointsHolder.ContainsWriteBreakpoint(
          &(current_script->memoryAddressBreakpointsHolder), memory_breakpoint_address);

  TMemCheck check;

  check.start_address = memory_breakpoint_address;
  check.end_address = memory_breakpoint_address;
  check.is_break_on_read = true;
  check.is_break_on_write = write_breakpoint_exists;
  check.condition = std::nullopt;
  check.break_on_hit = true;

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));
  current_script->memoryAddressBreakpointsHolder.AddReadBreakpoint(
      &(current_script->memoryAddressBreakpointsHolder), memory_breakpoint_address);

  current_script->scriptContextBaseFunctionsTable.RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallbacks(current_script, memory_breakpoint_address, callback);
  return CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder();
}

ArgHolder Unregister(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  u32 memory_breakpoint_address = args_list[0].u32_val;
  void* callback = args_list[1].void_pointer_val;
  current_script->memoryAddressBreakpointsHolder.RemoveReadBreakpoint(
      &(current_script->memoryAddressBreakpointsHolder), memory_breakpoint_address);

  if (!current_script->memoryAddressBreakpointsHolder.ContainsReadBreakpoint(
          &(current_script->memoryAddressBreakpointsHolder), memory_breakpoint_address) &&
      !current_script->memoryAddressBreakpointsHolder.ContainsWriteBreakpoint(
          &(current_script->memoryAddressBreakpointsHolder), memory_breakpoint_address))
  {
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Remove(memory_breakpoint_address);
  }

  bool return_value = current_script->scriptContextBaseFunctionsTable.UnregisterOnMemoryAddressReadFromCallbacks(current_script, memory_breakpoint_address, callback);

  if (!return_value)
    return CreateErrorStringArgHolder(
        "2nd Argument passed into OnMemoryAddressReadFrom:unregister() was not a reference to a "
        "function currently registered as an OnMemoryAddressReadFrom callback!");

  else
    return CreateUnregistrationReturnTypeArgHolder(nullptr);
}

ArgHolder IsInMemoryAddressReadFromCallback(ScriptContext* current_script,
                                            std::vector<ArgHolder>& args_list)
{
  return CreateBoolArgHolder(current_script->current_script_call_location ==
                             ScriptCallLocations::FromMemoryAddressReadFromCallback);
}
ArgHolder GetMemoryAddressReadFromForCurrentCallback(ScriptContext* current_script,
                                                     std::vector<ArgHolder>& args_list)
{
  if (current_script->current_script_call_location !=
      ScriptCallLocations::FromMemoryAddressReadFromCallback)
    return CreateErrorStringArgHolder(
        "User attempted to call "
        "OnMemoryAddressReadFrom:getMemoryAddressReadFromForCurrentCallback() outside of an "
        "OnMemoryAddressReadFrom callback function!");
  return CreateU32ArgHolder(memory_address_read_from_for_current_callback);
}
}  // namespace Scripting::OnMemoryAddressReadFromCallbackAPI
