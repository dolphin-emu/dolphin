#include "Core/Scripting/InternalAPIModules/WiiAPI.h"

namespace Scripting::WiiAPI
{
const char* class_name = "WiiAPI";

std::array<bool, 5> overwrite_controller_at_specified_port{};
std::array<u8*, 5> new_controller_inputs{};
std::array<u8*, 5> controller_inputs_on_last_frame{};
int current_controller_number_polled = -1;

ClassMetadata GetWiiApiClassData(const std::string& api_version);

ArgHolder GetCurrentPortNumberOfPoll(ScriptContext* current_script,
                                     std::vector<ArgHolder>& args_list);
ArgHolder SetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetInputsForPreviousFrame(ScriptContext* current_script,
                                    std::vector<ArgHolder>& args_list);
