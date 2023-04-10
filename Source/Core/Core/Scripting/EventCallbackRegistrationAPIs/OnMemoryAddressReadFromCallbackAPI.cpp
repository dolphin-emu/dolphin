#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnMemoryAddressReadFromCallbackAPI
{
const char* class_name = "OnMemoryAddressReadFrom";
u32 memory_address_read_from_for_current_callback = 0;

static std::array all_on_memory_address_read_from_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(memoryAddress, value)", Register,
                     ArgTypeEnum::RegistrationReturnType, {ArgTypeEnum::LongLong, ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("registerWithAutoDeregistration", "1.0",
                     "registerWithAutoDeregistration(memoryAddress, value)",
                     RegisterWithAutoDeregistration,
                     ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType, {ArgTypeEnum::LongLong, ArgTypeEnum::RegistrationWithAutoDeregistrationInputType}),
    FunctionMetadata("unregister", "1.0", "unregister(memoryAddress, value)", Unregister,
                     ArgTypeEnum::UnregistrationReturnType,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::UnregistrationInputType}),
    FunctionMetadata("isInMemoryAddressReadFromCallback", "1.0",
                     "isInMemoryAddressReadFromCallback()", IsInMemoryAddressReadFromCallback,
                     ArgTypeEnum::Boolean, {}),
    FunctionMetadata("getMemoryAddressReadFromForCurrentCallback", "1.0",
                     "getMemoryAddressReadFromForCurrentCallback()",
                     GetMemoryAddressReadFromForCurrentCallback, ArgTypeEnum::U32, {})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name,
          GetLatestFunctionsForVersion(all_on_memory_address_read_from_callback_functions_metadata_list,
                                       api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_on_memory_address_read_from_callback_functions_metadata_list)};
}

 FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_on_memory_address_read_from_callback_functions_metadata_list, api_version, function_name,
                               deprecated_functions_map);
}

ArgHolder Register(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateRegistrationReturnTypeArgHolder(
      current_script->RegisterOnMemoryAddressReadFromCallbacks(args_list[0].long_long_val, args_list[1].void_pointer_val));
}

ArgHolder RegisterWithAutoDeregistration(ScriptContext* current_script,
                                         std::vector<ArgHolder>& args_list)
{
  current_script->RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallbacks(
      args_list[0].long_long_val, args_list[1].void_pointer_val);
  return CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder();
}

ArgHolder Unregister(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  if (args_list[0].long_long_val < 0)
    return CreateErrorStringArgHolder("Address was less than 0!");
  bool return_value = current_script->UnregisterOnMemoryAddressReadFromCallbacks(args_list[0].long_long_val, args_list[1].void_pointer_val);
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
  if (current_script->current_script_call_location != ScriptCallLocations::FromMemoryAddressReadFromCallback)
    return CreateErrorStringArgHolder(
        "User attempted to call "
        "OnMemoryAddressReadFrom:getMemoryAddressReadFromForCurrentCallback() outside of an "
        "OnMemoryAddressReadFrom callback function!");
  return CreateU32ArgHolder(memory_address_read_from_for_current_callback);
}
}  // namespace Scripting::OnMemoryAddressReadFromCallbackAPI
