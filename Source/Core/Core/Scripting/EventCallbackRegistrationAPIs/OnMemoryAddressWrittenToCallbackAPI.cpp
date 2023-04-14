#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
{
const char* class_name = "OnMemoryAddressWrittenTo";

u32 memory_address_written_to_for_current_callback = 0;
s64 value_written_to_memory_address_for_current_callback = -1;
bool in_memory_address_written_to_breakpoint = false;

static std::array all_on_memory_address_written_to_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(memoryAddress, value)", Register,
                     ArgTypeEnum::RegistrationReturnType, {ArgTypeEnum::U32, ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("registerWithAutoDeregistration", "1.0",
                     "registerWithAutoDeregistration(memoryAddress, value)",
                     RegisterWithAutoDeregistration,
                     ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType, {ArgTypeEnum::U32, ArgTypeEnum::RegistrationWithAutoDeregistrationInputType}),
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
                     GetValueWrittenToMemoryAddressForCurrentCallback, ArgTypeEnum::LongLong, {})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name,
          GetLatestFunctionsForVersion(all_on_memory_address_written_to_callback_functions_metadata_list,
                                       api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_on_memory_address_written_to_callback_functions_metadata_list)};
}

 FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_on_memory_address_written_to_callback_functions_metadata_list, api_version, function_name,
                               deprecated_functions_map);
}

ArgHolder Register(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateRegistrationReturnTypeArgHolder(
      current_script->RegisterOnMemoryAddressWrittenToCallbacks(args_list[0].u32_val, args_list[1].void_pointer_val));
}

ArgHolder RegisterWithAutoDeregistration(ScriptContext* current_script,
                                         std::vector<ArgHolder>& args_list)
{
  current_script->RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallbacks(
      args_list[0].u32_val, args_list[1].void_pointer_val);
  return CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder();
}

ArgHolder Unregister(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  if (args_list[0].u32_val < 0)
    return CreateErrorStringArgHolder("Address was less than 0!");
  bool return_value = current_script->UnregisterOnMemoryAddressWrittenToCallbacks(args_list[0].u32_val, args_list[1].void_pointer_val);
  if (!return_value)
    return CreateErrorStringArgHolder(
        "2nd argument passed into OnMemoryAddressWrittenTo:unregister() was not a reference to a "
        "function currently registered as an OnMemoryAddressWrittenTo callback!");
  else
    return CreateUnregistrationReturnTypeArgHolder(nullptr);
}

ArgHolder IsInMemoryAddressWrittenToCallback(ScriptContext* current_script,
                                             std::vector<ArgHolder>& arg_list)
{
  return CreateBoolArgHolder(current_script->current_script_call_location ==
                             ScriptCallLocations::FromMemoryAddressWrittenToCallback);
}

ArgHolder GetMemoryAddressWrittenToForCurrentCallback(ScriptContext* current_script,
                                                      std::vector<ArgHolder>& args_list)
{
  if (current_script->current_script_call_location != ScriptCallLocations::FromMemoryAddressWrittenToCallback)
    return CreateErrorStringArgHolder(
        "User attempted to call "
        "OnMemoryAddressWrittenTo:getMemoryAddressWrittenToForCurrentCallback() outside of an "
        "OnMemoryAddressWrittenTo callback function!");
  return CreateU32ArgHolder(memory_address_written_to_for_current_callback);
}

ArgHolder GetValueWrittenToMemoryAddressForCurrentCallback(ScriptContext* current_script,
                                                           std::vector<ArgHolder>& args_list)
{
  if (current_script->current_script_call_location != ScriptCallLocations::FromMemoryAddressWrittenToCallback)
    return CreateErrorStringArgHolder(
        "User attempted to call "
        "OnMemoryAddressWrittenTo:getValueWrittenToMemoryAddressForCurrentCallback() outside of an "
        "OnMemoryAddressWrittenTo callback function!");
  return CreateLongLongArgHolder(value_written_to_memory_address_for_current_callback);
}
}  // namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
