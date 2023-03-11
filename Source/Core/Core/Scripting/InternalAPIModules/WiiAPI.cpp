#include "Core/Scripting/InternalAPIModules/WiiAPI.h"

#include "Core/Movie.h"

namespace Scripting::WiiAPI
{
const char* class_name = "WiiAPI";

extern std::array<WiimoteCommon::DataReportBuilder*, 5> current_wii_slot_inputs;
int current_controller_number_polled = -1;

ClassMetadata GetWiiApiClassData(const std::string& api_version)
{
  return {};
}

ArgHolder GetCurrentPortNumberOfPoll(ScriptContext* current_script,
                                     std::vector<ArgHolder>& args_list)
{
  return CreateLongLongArgHolder(current_controller_number_polled + 1);
}

ArgHolder SetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return {};
}
ArgHolder GetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return {};
}
ArgHolder GetInputsForPreviousFrame(ScriptContext* current_script,
                                    std::vector<ArgHolder>& args_list)
{
  return {};
}

ArgHolder IsWiimoteInSlot(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  long long port_number = args_list[0].long_long_val;
  if (port_number < 1 || port_number > 4)
    return CreateErrorStringArgHolder("Port number was not in the valid range of 1-4");
  bool wiimote_in_slot = Movie::IsUsingWiimote(port_number - 1);
  return CreateBoolArgHolder(wiimote_in_slot);
}

ArgHolder IsNunchuckAttachedToCurrentSlot(ScriptContext* current_script,
                                          std::vector<ArgHolder>& args_list)
{
  return {};
}

ArgHolder IsClassicControllerAttachedToCurrentSlot(ScriptContext* current_script,
                                                   std::vector<ArgHolder>& args_list)
{
  return {};

}

ArgHolder CurrentSlotHasAcceleration(ScriptContext* current_script,
                                     std::vector<ArgHolder>& args_list)
{
  return {};
}

ArgHolder CurrentSlotHasIR(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return {};
}

ArgHolder CurrentSlotHasExtension(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  return {};
}

}  // namespace Scripting::WiiAPI
