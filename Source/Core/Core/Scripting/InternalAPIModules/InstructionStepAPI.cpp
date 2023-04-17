#include "Core/Scripting/InternalAPIModules/InstructionStepAPI.h"

#include <fmt/format.h>
#include "common/GekkoDisassembler.h"
#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnInstructionHitCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"
#include "Core/System.h"

namespace Scripting::InstructionStepAPI
{
const char* class_name = "InstructionStepAPI";

static std::array all_instruction_step_functions_metadata_list = {
  FunctionMetadata("singleStep", "1.0", "singleStep()", SingleStep, ArgTypeEnum::VoidType, {}),
    FunctionMetadata("getInstructionFromAddress", "1.0", "getInstructionFromAddress(0X80000032)",
                     GetInstructionFromAddress, ArgTypeEnum::String, {ArgTypeEnum::U32})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_instruction_step_functions_metadata_list,
                                                   api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_instruction_step_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_instruction_step_functions_metadata_list, api_version,
                               function_name, deprecated_functions_map);
}

bool IsCurrentlyInBreakpoint()
{
  return OnInstructionHitCallbackAPI::in_instruction_hit_breakpoint ||
         OnMemoryAddressReadFromCallbackAPI::in_memory_address_read_from_breakpoint ||
         OnMemoryAddressWrittenToCallbackAPI::in_memory_address_written_to_breakpoint;
}

ArgHolder CreateNotInBreakpointError(const std::string& function_name)
{
  return CreateErrorStringArgHolder(
      std::string("Error: CPU was not in a valid callback when ") + function_name +
      " method was called. Make sure that you invoked function from inside of an OnInstructionHit "
      "callback, an OnMemoryAddressReadFrom callback, or an OnMemoryAddressWrittenTo callback in "
      "order to prevent this error.");
}

ArgHolder SingleStep(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  if (!IsCurrentlyInBreakpoint())
    return CreateNotInBreakpointError("SingleStep()");
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::PowerPCManager& power_pc = Core::System::GetInstance().GetPowerPC();
  power_pc.GetBreakPoints().ClearAllTemporary();

  PowerPC::CoreMode old_mode = power_pc.GetMode();
  power_pc.SetMode(PowerPC::CoreMode::Interpreter);
  power_pc.SingleStep();
  power_pc.SetMode(old_mode);
  return CreateVoidTypeArgHolder();
}

ArgHolder GetInstructionFromAddress(ScriptContext* current_script, std::vector<ArgHolder>& args_list)
{
  u32 instruction_addr = args_list[0].u32_val;
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  const u32 op = PowerPC::MMU::HostRead_Instruction(guard, instruction_addr);
  std::string disasm = Common::GekkoDisassembler::Disassemble(op, instruction_addr);
  return CreateStringArgHolder(disasm);
}
}  // namespace Scripting::InstructionStepAPI
