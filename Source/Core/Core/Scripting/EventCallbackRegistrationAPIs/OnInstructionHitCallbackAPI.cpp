#include "Core/Scripting/EventCallbackRegistrationAPIs/OnInstructionHitCallbackAPI.h"

#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnInstructionHitCallbackAPI
{
const char* class_name = "OnInstructionHit";

static std::array all_on_instruction_hit_callback_functions_metadata_list = {
    FunctionMetadata("register", "1.0", "register(instructionAddress, value)", Register,
                     ArgTypeEnum::RegistrationReturnType, {ArgTypeEnum::LongLong, ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("registerWithAutoDeregistration", "1.0",
                     "registerWithAutoDeregistration(instructionAddress, value)",
                     RegisterWithAutoDeregistration,
                     ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType, {ArgTypeEnum::LongLong, ArgTypeEnum::RegistrationWithAutoDeregistrationInputType}),
    FunctionMetadata("unregister", "1.0", "unregister(instructionAddress, value)", Unregister,
                     ArgTypeEnum::UnregistrationReturnType,
                     {ArgTypeEnum::LongLong, ArgTypeEnum::UnregistrationInputType})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_on_instruction_hit_callback_functions_metadata_list, api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_on_instruction_hit_callback_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_on_instruction_hit_callback_functions_metadata_list, api_version, function_name,
                               deprecated_functions_map);
}

ArgHolder Register(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return CreateRegistrationReturnTypeArgHolder(
      current_script->RegisterOnInstructionReachedCallbacks(args_list[0].long_long_val, args_list[1].void_pointer_val));
}

ArgHolder RegisterWithAutoDeregistration(ScriptContext* current_script,
                                         std::vector<ArgHolder>& args_list)
{
  current_script->RegisterOnInstructionReachedWithAutoDeregistrationCallbacks(
      args_list[0].long_long_val, args_list[1].void_pointer_val);
  return CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder();
}

ArgHolder Unregister(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  bool return_value = current_script->UnregisterOnInstructionReachedCallbacks(
      args_list[0].long_long_val, args_list[1].void_pointer_val);
  if (!return_value)
    return CreateErrorStringArgHolder(
        "Argument passed into OnInstructionHit:unregister() was not a reference to a function "
        "currently registered as an OnInstructionHit callback!");
  else
    return CreateUnregistrationReturnTypeArgHolder(nullptr);
}
}  // namespace Scripting::OnInstructionHitCallbackAPI
