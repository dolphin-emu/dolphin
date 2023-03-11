#pragma once
#include <string>
#include <vector>
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/Scripting/HelperClasses/WiiControllerRepresentation.h"

namespace Scripting::WiiAPI
{
extern const char* class_name;

extern std::array<bool, 5> should_overwrite_slot_array;
extern std::array<WiiControllerEnums::WiiSlot, 5> new_slot_inputs;
extern std::array<WiiControllerEnums::WiiSlot, 5> slot_inputs_on_last_frame;
extern int current_controller_number_polled;

ClassMetadata GetWiiApiClassData(const std::string& api_version);

ArgHolder GetCurrentPortNumberOfPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder SetInputsForWiimotePoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForWiimotePoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder SetInputsForIRPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForIRPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder SetInputsForNunchuckPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForNunchuckPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);


ArgHolder SetInputsForClassicControllerPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForClassicControllerPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder GetInputsForWiimoteOnPreviousFrame(ScriptContext* current_script,
                                    std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForIROnPreviousFrame(ScriptContext* current_script,
                                             std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForNunchuckOnPreviousFrame(ScriptContext* current_script,
                                             std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForClassicControllerOnPreviousFrame(ScriptContext* current_script,
                                             std::vector<ArgHolder>& args_list);


ArgHolder IsWiimoteInSlot(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder IsNunchuckAttachedToCurrentSlot(ScriptContext* current_script,
                                   std::vector<ArgHolder>& args_list);
ArgHolder IsClassicControllerAttachedToCurrentSlot(ScriptContext* current_script,
                                            std::vector<ArgHolder>& args_list);
ArgHolder CurrentSlotHasIR(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder CurrentSlotWiimoteHasAccel(ScriptContext* current_script,
                                     std::vector<ArgHolder>& args_list);
ArgHolder CurrentSlotHasExtension(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
}  // namespace Scripting::WiiAPI
