#include "Core/Scripting/EventCallbackRegistrationAPIs/OnGCControllerPolledCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnGCControllerPolledCallbackAPI
{
const char* class_name = "OnGCControllerPolled";

static std::array all_on_gc_controller_polled_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(value)", Register, ArgTypeEnum::RegistrationReturnType, {ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("unregister", "1.0", "unregister(value)", Unregister, ArgTypeEnum::UnregistrationReturnType, {ArgTypeEnum::UnregistrationInputType})};

ClassMetadata GetOnGCControllerPolledCallbackApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_on_gc_controller_polled_callback_functions_metadata_list, api_version, deprecated_functions_map)};
}

ArgHolder Register(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateRegistrationReturnTypeArgHolder(
      current_script->RegisterOnGCCControllerPolledCallbacks(args_list[0].void_pointer_val));
}

ArgHolder Unregister(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateUnregistrationReturnTypeArgHolder(
      current_script->UnregisterOnGCControllerPolledCallbacks(args_list[0].void_pointer_val));
}
}  // namespace Scripting::OnGCControllerPolledCallbackAPI
