#pragma once
#include <string>
#include <vector>
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"

namespace Scripting::WiiAPI
{
extern const char* class_name;
/* Commenting this out until the format of Wii movies is altered.
extern std::array<bool, 5> should_overwrite_slot_array;
extern std::array<WiiControllerEnums::WiiSlot, 5> new_slot_inputs;
extern std::array<WiiControllerEnums::WiiSlot, 5> slot_inputs_on_last_frame;
extern int current_controller_number_polled;

ClassMetadata GetWiiApiClassData(const std::string& api_version);

ArgHolder GetCurrentPortNumberOfPoll(ScriptContext* current_script, std::vector<ArgHolder>&
args_list);

ArgHolder GetInputsForWiimotePoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder SetInputsForWiimotePoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder GetInputsForNunchuckPoll(ScriptContext* current_script,
                                   std::vector<ArgHolder>& args_list);
ArgHolder SetInputsForNunchuckPoll(ScriptContext* current_script,
                                   std::vector<ArgHolder>& args_list);

ArgHolder GetInputsForClassicControllerPoll(ScriptContext* current_script,
                                            std::vector<ArgHolder>& args_list);
ArgHolder SetInputsForClassicControllerPoll(ScriptContext* current_script,
                                            std::vector<ArgHolder>& args_list);

ArgHolder GetInputsForWiimoteOnPreviousFrame(ScriptContext* current_script,
                                    std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForNunchuckOnPreviousFrame(ScriptContext* current_script,
                                             std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForClassicControllerOnPreviousFrame(ScriptContext* current_script,
                                             std::vector<ArgHolder>& args_list);


ArgHolder IsWiimoteInSlot(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder IsNunchuckAttachedToWiimoteInSlot(ScriptContext* current_script,
                                   std::vector<ArgHolder>& args_list);
ArgHolder IsClassicControllerAttachedToWiimoteInSlot(ScriptContext* current_script,
                                            std::vector<ArgHolder>& args_list);
ArgHolder SlotHasIR(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder WiimoteInSlotHasAccel(ScriptContext* current_script,
                                     std::vector<ArgHolder>& args_list);
ArgHolder SlotHasExtension(ScriptContext* current_script, std::vector<ArgHolder>& args_list);*/
}  // namespace Scripting::WiiAPI
