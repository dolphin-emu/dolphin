#include "Core/Scripting/InternalAPIModules/WiiAPI.h"

#include "Core/Movie.h"

namespace Scripting::WiiAPI
{
const char* class_name = "WiiAPI";
/*
std::array<bool, 5> should_overwrite_slot_array{};
std::array<WiiControllerEnums::WiiSlot, 5> current_wii_slot_inputs{};
std::array<WiiControllerEnums::WiiSlot, 5> slot_inputs_on_last_frame{};
int current_controller_number_polled = -1;


static std::array all_wii_controller_functions_metadata_list = {
    FunctionMetadata("getCurrentPortNumberOfPoll", "1.0", "getCurrentPortNumberOfPoll()",
                     GetCurrentPortNumberOfPoll, ArgTypeEnum::LongLong, {}),
    FunctionMetadata("getInputsForWiimotePoll", "1.0", "getInputsForWiimotePoll()",
                     GetInputsForWiimotePoll, ArgTypeEnum::WiimoteStateObject, {}),
    FunctionMetadata("setInputsForWiimotePoll", "1.0", "setInputsForWiimotePoll(wiimoteObj)",
                     SetInputsForWiimotePoll, ArgTypeEnum::VoidType,
{ArgTypeEnum::WiimoteStateObject}), FunctionMetadata("getInputsForNunchuckPoll", )};

 ClassMetadata GetWiiApiClassData(const std::string& api_version)
{
  return {};
}

ArgHolder GetCurrentPortNumberOfPoll(ScriptContext* current_script,
                                     std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(current_controller_number_polled + 1);
}

ArgHolder GetCurrentPortNumberOfPoll(ScriptContext* current_script,
                                     std::vector<ArgHolder>& args_list);

ArgHolder GetInputsForWiimotePoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder SetInputsForWiimotePoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder GetInputsForNunchuckPoll(ScriptContext* current_script, std::vector<ArgHolder>&
args_list); ArgHolder SetInputsForNunchuckPoll(ScriptContext* current_script,
std::vector<ArgHolder>& args_list);

ArgHolder GetInputsForClassicControllerPoll(ScriptContext* current_script, std::vector<ArgHolder>&
args_list); ArgHolder SetInputsForClassicControllerPoll(ScriptContext* current_script,
std::vector<ArgHolder>& args_list);


ArgHolder GetInputsForWiimoteOnPreviousFrame(ScriptContext* current_script,
                                             std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForNunchuckOnPreviousFrame(ScriptContext* current_script,
                                              std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForClassicControllerOnPreviousFrame(ScriptContext* current_script,
                                                       std::vector<ArgHolder>& args_list);

ArgHolder IsWiimoteInSlot(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  long long port_number = args_list[0].long_long_val;
  if (port_number < 1 || port_number > 4)
    return CreateErrorStringArgHolder("Port number was not in the valid range of 1-4");
  bool wiimote_in_slot = Movie::IsUsingWiimote(port_number - 1);
  return CreateBoolArgHolder(wiimote_in_slot);
}

ArgHolder IsNunchuckAttachedToWiimoteInSlot(ScriptContext* current_script,
                                            std::vector<ArgHolder>& args_list);
ArgHolder IsClassicControllerAttachedToWiimoteInSlot(ScriptContext* current_script,
                                                     std::vector<ArgHolder>& args_list);
ArgHolder SlotHasIR(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder WiimoteInSlotHasAccel(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder SlotHasExtension(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
*/
}  // namespace Scripting::WiiAPI
