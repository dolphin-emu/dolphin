#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
{
const char* class_name = "OnMemoryAddressWrittenTo";

static std::array all_on_memory_address_written_to_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(memoryAddress, value)", Register,
                     ArgTypeEnum::RegistrationReturnType, {ArgTypeEnum::LongLong, ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("unregister", "1.0", "unregister(memoryAddress, value)", Unregister,
                     ArgTypeEnum::UnregistrationReturnType,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::UnregistrationInputType})};

ClassMetadata GetOnMemoryAddressWrittenToCallbackApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name,
          GetLatestFunctionsForVersion(all_on_memory_address_written_to_callback_functions_metadata_list,
                                       api_version, deprecated_functions_map)};
}

ArgHolder Register(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateRegistrationReturnTypeArgHolder(
      current_script->RegisterOnMemoryAddressWrittenToCallbacks(args_list[0].long_long_val, args_list[1].void_pointer_val));
}

ArgHolder Unregister(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateUnregistrationReturnTypeArgHolder(
      current_script->UnregisterOnMemoryAddressWrittenToCallbacks(args_list[0].long_long_val, args_list[1].void_pointer_val));
}
}  // namespace Scripting::OnMemoryAddressWrittenToCallbackAPI
