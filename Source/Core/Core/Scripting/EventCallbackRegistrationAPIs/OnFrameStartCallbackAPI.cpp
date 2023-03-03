#include "Core/Scripting/EventCallbackRegistrationAPIs/OnFrameStartCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnFrameStartCallbackAPI
{
const char* class_name = "OnFrameStart";
static std::array all_on_frame_start_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(value)", Register, ArgTypeEnum::RegistrationReturnType,
                     {ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("unregister", "1.0", "unregister(value)", Unregister, ArgTypeEnum::UnregistrationReturnType,
                     {ArgTypeEnum::UnregistrationInputType})};


ClassMetadata GetOnFrameStartCallbackApiClassData(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_on_frame_start_callback_functions_metadata_list, api_version, deprecated_functions_map)};
}

ArgHolder Register(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateRegistrationReturnTypeArgHolder(current_script->RegisterOnFrameStartCallbacks(args_list[0].void_pointer_val));
}

ArgHolder Unregister(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  bool return_value = current_script->UnregisterOnFrameStartCallbacks(args_list[0].void_pointer_val);
  if (!return_value)
    return CreateErrorStringArgHolder(
        "Argument passed into OnFrameStart:unregister() was not a reference to a function "
        "currently registered as an OnFrameStart callback!");
  else
    return CreateUnregistrationReturnTypeArgHolder(nullptr);
}
}
