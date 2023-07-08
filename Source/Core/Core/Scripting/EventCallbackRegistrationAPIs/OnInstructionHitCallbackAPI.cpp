#include "Core/Scripting/EventCallbackRegistrationAPIs/OnInstructionHitCallbackAPI.h"

#include "Core/Scripting/HelperClasses/InstructionBreakpointsHolder.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::OnInstructionHitCallbackAPI
{
const char* class_name = "OnInstructionHit";
u32 instruction_address_for_current_callback = 0;
bool in_instruction_hit_breakpoint = false;

static std::array all_on_instruction_hit_callback_functions_metadata_list = {
    FunctionMetadata(
        "register", "1.0", "register(instructionAddress, value)", Register,
        ScriptingEnums::ArgTypeEnum::RegistrationReturnType,
        {ScriptingEnums::ArgTypeEnum::U32, ScriptingEnums::ArgTypeEnum::RegistrationInputType}),
    FunctionMetadata("registerWithAutoDeregistration", "1.0",
                     "registerWithAutoDeregistration(instructionAddress, value)",
                     RegisterWithAutoDeregistration,
                     ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationReturnType,
                     {ScriptingEnums::ArgTypeEnum::U32,
                      ScriptingEnums::ArgTypeEnum::RegistrationWithAutoDeregistrationInputType}),
    FunctionMetadata(
        "unregister", "1.0", "unregister(instructionAddress, value)", Unregister,
        ScriptingEnums::ArgTypeEnum::UnregistrationReturnType,
        {ScriptingEnums::ArgTypeEnum::U32, ScriptingEnums::ArgTypeEnum::UnregistrationInputType}),
    FunctionMetadata("isInInstructionHitCallback", "1.0", "isInInstructionHitCallback()",
                     IsInInstructionHitCallback, ScriptingEnums::ArgTypeEnum::Boolean, {}),
    FunctionMetadata("getAddressOfInstructionForCurrentCallback", "1.0",
                     "getAddressOfInstructionForCurrentCallback()",
                     GetAddressOfInstructionForCurrentCallback, ScriptingEnums::ArgTypeEnum::U32,
                     {})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name,
          GetLatestFunctionsForVersion(all_on_instruction_hit_callback_functions_metadata_list,
                                       api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_on_instruction_hit_callback_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_on_instruction_hit_callback_functions_metadata_list, api_version,
                               function_name, deprecated_functions_map);
}

ArgHolder* Register(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  u32 address_of_breakpoint = (*args_list)[0]->u32_val;
  void* callback = (*args_list)[1]->void_pointer_val;

  current_script->instructionBreakpointsHolder.AddBreakpoint(address_of_breakpoint);
  return CreateRegistrationReturnTypeArgHolder(
      current_script->dll_specific_api_definitions.RegisterOnInstructionReachedCallback(
          current_script, address_of_breakpoint, callback));
}

ArgHolder* RegisterWithAutoDeregistration(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list)
{
  u32 address_of_breakpoint = (*args_list)[0]->u32_val;
  void* callback = (*args_list)[1]->void_pointer_val;

  current_script->instructionBreakpointsHolder.AddBreakpoint(address_of_breakpoint);
  current_script->dll_specific_api_definitions
      .RegisterOnInstructionReachedWithAutoDeregistrationCallback(current_script,
                                                                  address_of_breakpoint, callback);
  return CreateRegistrationWithAutoDeregistrationReturnTypeArgHolder();
}

ArgHolder* Unregister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  u32 address_of_breakpoint = (*args_list)[0]->u32_val;
  void* callback = (*args_list)[1]->void_pointer_val;

  if (!current_script->instructionBreakpointsHolder.ContainsBreakpoint(address_of_breakpoint))
    return CreateErrorStringArgHolder(
        "Error: Address passed into OnInstructionHit:Unregister() did not correspond to any "
        "breakpoint that was currently enabled!");

  current_script->instructionBreakpointsHolder.RemoveBreakpoint(address_of_breakpoint);

  bool return_value =
      current_script->dll_specific_api_definitions.UnregisterOnInstructionReachedCallback(
          current_script, callback);
  if (!return_value)
    return CreateErrorStringArgHolder(
        "Argument passed into OnInstructionHit:unregister() was not a reference to a function "
        "currently registered as an OnInstructionHit callback!");
  else
    return CreateUnregistrationReturnTypeArgHolder(nullptr);
}

ArgHolder* IsInInstructionHitCallback(ScriptContext* current_script,
                                      std::vector<ArgHolder*>* args_list)
{
  return CreateBoolArgHolder(current_script->current_script_call_location ==
                             ScriptingEnums::ScriptCallLocations::FromInstructionHitCallback);
}

ArgHolder* GetAddressOfInstructionForCurrentCallback(ScriptContext* current_script,
                                                     std::vector<ArgHolder*>* args_list)
{
  if (current_script->current_script_call_location !=
      ScriptingEnums::ScriptCallLocations::FromInstructionHitCallback)
    return CreateErrorStringArgHolder(
        "User attempted to call OnInstructionHit:getAddressOfInstructionForCurrentCallback() "
        "outside of an OnInstructionHit callback function!");
  return CreateU32ArgHolder(instruction_address_for_current_callback);
}
}  // namespace Scripting::OnInstructionHitCallbackAPI
