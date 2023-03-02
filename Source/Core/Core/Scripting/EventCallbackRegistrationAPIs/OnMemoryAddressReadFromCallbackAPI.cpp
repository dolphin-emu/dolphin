#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnMemoryAddressReadFromCallbackAPI
{
const char* class_name = "OnMemoryAddressReadFrom";

static std::array all_on_memory_address_read_from_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(memoryAddress, value)", Register,
                     ArgTypeEnum::RegistrationReturnType, {ArgTypeEnum::LongLong, ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("unregister", "1.0", "unregister(memoryAddress, value)", Unregister,
                     ArgTypeEnum::UnregistrationReturnType,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::UnregistrationInputType})};

ClassMetadata GetOnMemoryAddressReadFromCallbackApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name,
          GetLatestFunctionsForVersion(all_on_memory_address_read_from_callback_functions_metadata_list,
                                       api_version, deprecated_functions_map)};
}

ArgHolder Register(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateRegistrationReturnTypeArgHolder(
      current_script->RegisterOnMemoryAddressReadFromCallbacks(args_list[0].long_long_val, args_list[1].void_pointer_val));
}

ArgHolder Unregister(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateUnregistrationReturnTypeArgHolder(
      current_script->UnregisterOnMemoryAddressReadFromCallbacks(args_list[0].long_long_val, args_list[1].void_pointer_val));
}
}  // namespace Scripting::OnMemoryAddressReadFromCallbackAPI
