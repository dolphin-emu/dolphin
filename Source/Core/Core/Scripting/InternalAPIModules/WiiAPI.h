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

namespace Scripting::WiiAPI
{
extern const char* class_name;

extern std::array<bool, 5> overwrite_controller_at_specified_port;
extern std::array<u8*, 5> new_controller_inputs;
extern std::array<u8*, 5> controller_inputs_on_last_frame;
extern int current_controller_number_polled;

ClassMetadata GetWiiApiClassData(const std::string& api_version);

ArgHolder GetCurrentPortNumberOfPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder SetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForPreviousFrame(ScriptContext* current_script,
                                    std::vector<ArgHolder>& args_list);
}  // namespace Scripting::WiiAPI
