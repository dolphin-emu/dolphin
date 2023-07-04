#include "Core/Scripting/EventCallbackRegistrationAPIs/OnWiiInputPolledCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnWiiInputPolledCallbackAPI
{
const char* class_name = "OnWiiInputPolled";

static std::array all_on_wii_input_polled_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(value)", Register,
                     ScriptingEnums::ArgTypeEnum::RegistrationReturnType,
                     {ScriptingEnums::ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("registerWithAutoDeregistration", "1.0",
                     "registerWithAutoDeregistration(value)", RegisterWithAutoDeregistration,
                     ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType,
                     {ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationInputType}),
    FunctionMetadata("unregister", "1.0", "unregister(value)", Unregister,
                     ScriptingEnums::ArgTypeEnum::UnregistrationReturnType,
                     {ScriptingEnums::ArgTypeEnum::UnregistrationInputType}),
    FunctionMetadata("isInWiiInputPolledCallback", "1.0", "isInWiiInputPolledCallback()",
                     IsInWiiInputPolledCallback, ScriptingEnums::ArgTypeEnum::Boolean, {})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name,
          GetLatestFunctionsForVersion(all_on_wii_input_polled_callback_functions_metadata_list,
                                       api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_on_wii_input_polled_callback_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_on_wii_input_polled_callback_functions_metadata_list,
                               api_version, function_name, deprecated_functions_map);
}

ArgHolder* Register(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateRegistrationReturnTypeArgHolder(
      current_script->dll_specific_api_definitions.RegisterOnWiiInputPolledCallback(
          current_script, (*args_list)[0]->void_pointer_val));
}

ArgHolder* RegisterWithAutoDeregistration(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list)
{
  current_script->dll_specific_api_definitions
      .RegisterOnWiiInputPolledWithAutoDeregistrationCallback(current_script,
                                                              (*args_list)[0]->void_pointer_val);
  return CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder();
}

ArgHolder* Unregister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  bool return_value =
      current_script->dll_specific_api_definitions.UnregisterOnWiiInputPolledCallback(
          current_script, (*args_list)[0]->void_pointer_val);
  if (!return_value)
    return CreateErrorStringArgHolder(
        "Argument passed into OnWiiInputPolled:unregister() was not a reference to a function "
        "currently registered as an OnWiiInputPolled callback!");
  else
    return CreateUnregistrationReturnTypeArgHolder(nullptr);
}

ArgHolder* IsInWiiInputPolledCallback(ScriptContext* current_script,
                                      std::vector<ArgHolder*>* args_list)
{
  return CreateBoolArgHolder(current_script->current_script_call_location ==
                             ScriptingEnums::ScriptCallLocations::FromWiiInputPolled);
}
}  // namespace Scripting::OnWiiInputPolledCallbackAPI
