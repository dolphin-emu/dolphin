#include "Core/Scripting/InternalAPIModules/InstructionStepAPI.h"

#include <fmt/format.h>
#include "Common/GekkoDisassembler.h"
#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/Movie.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnInstructionHitCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"
#include "Core/System.h"

namespace Scripting::InstructionStepAPI
{
const char* class_name = "InstructionStepAPI";

static std::array all_instruction_step_functions_metadata_list = {
    FunctionMetadata("singleStep", "1.0", "singleStep()", SingleStep,
                     ScriptingEnums::ArgTypeEnum::VoidType, {}),
    FunctionMetadata("stepOver", "1.0", "stepOver()", StepOver,
                     ScriptingEnums::ArgTypeEnum::VoidType, {}),
    FunctionMetadata("stepOut", "1.0", "stepOut()", StepOut, ScriptingEnums::ArgTypeEnum::VoidType,
                     {}),
    FunctionMetadata("skip", "1.0", "skip()", Skip, ScriptingEnums::ArgTypeEnum::VoidType, {}),
    FunctionMetadata("setPC", "1.0", "setPC(0X80000045)", SetPC,
                     ScriptingEnums::ArgTypeEnum::VoidType, {ScriptingEnums::ArgTypeEnum::U32}),
    FunctionMetadata("getInstructionFromAddress", "1.0", "getInstructionFromAddress(0X80000032)",
                     GetInstructionFromAddress, ScriptingEnums::ArgTypeEnum::String,
                     {ScriptingEnums::ArgTypeEnum::U32})};

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

ArgHolder* CreateNotInBreakpointError(const std::string& function_name)
{
  return CreateErrorStringArgHolder(
      std::string("Error: CPU was not in a valid callback when ") + function_name +
      " method was called. Make sure that you invoked function from inside of an OnInstructionHit "
      "callback, an OnMemoryAddressReadFrom callback, or an OnMemoryAddressWrittenTo callback in "
      "order to prevent this error.");
}

ArgHolder* SingleStep(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
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

ArgHolder* StepOver(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  if (!IsCurrentlyInBreakpoint())
    return CreateNotInBreakpointError("StepOver()");
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::PowerPCManager& power_pc = Core::System::GetInstance().GetPowerPC();
  power_pc.GetBreakPoints().ClearAllTemporary();

  int starting_frame_number = Movie::GetCurrentFrame();
  u32 pc_value_to_end_on = power_pc.GetPPCState().pc + 4;
  PowerPC::CoreMode old_mode = power_pc.GetMode();
  power_pc.SetMode(PowerPC::CoreMode::Interpreter);

  // Step until we hit the instruction after the one we started on, or until we hit the next frame
  // (whichever one happens first)
  while (power_pc.GetPPCState().pc != pc_value_to_end_on &&
         Movie::GetCurrentFrame() == starting_frame_number)
    power_pc.SingleStep();

  power_pc.SetMode(old_mode);
  return CreateVoidTypeArgHolder();
}

bool IsPotentiallyFunctionCallInstruction(UGeckoInstruction instruction)
{
  return instruction.LK;
}

bool IsFunctionReturnInstruction(UGeckoInstruction instruction)
{
  if (instruction.hex == 0x4C000064u)
    return true;
  const auto& ppc_state = Core::System::GetInstance().GetPPCState();
  bool counter = (instruction.BO_2 >> 2 & 1) != 0 ||
                 (CTR(ppc_state) != 0) != ((instruction.BO_2 >> 1 & 1) != 0);
  bool condition = instruction.BO_2 >> 4 != 0 ||
                   ppc_state.cr.GetBit(instruction.BI_2) == (instruction.BO_2 >> 3 & 1);
  bool isBclr = instruction.OPCD_7 == 0b010011 && (instruction.hex >> 1 & 0b10000) != 0;
  return isBclr && counter && condition && !instruction.LK_3;
}

ArgHolder* StepOut(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  if (!IsCurrentlyInBreakpoint())
    return CreateNotInBreakpointError("StepOut()");
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::PowerPCManager& power_pc = Core::System::GetInstance().GetPowerPC();
  power_pc.GetBreakPoints().ClearAllTemporary();

  int function_call_depth_from_start = 0;
  int starting_frame_number = Movie::GetCurrentFrame();
  PowerPC::CoreMode old_mode = power_pc.GetMode();
  power_pc.SetMode(PowerPC::CoreMode::Interpreter);

  while (function_call_depth_from_start >= 0 && Movie::GetCurrentFrame() == starting_frame_number)
  {
    UGeckoInstruction current_instruction =
        PowerPC::MMU::HostRead_Instruction(guard, power_pc.GetPPCState().pc);
    bool potentially_a_function_call = false;
    u32 pc_before_step = power_pc.GetPPCState().pc;
    potentially_a_function_call = IsPotentiallyFunctionCallInstruction(current_instruction);
    if (IsFunctionReturnInstruction(current_instruction))
      --function_call_depth_from_start;

    power_pc.SingleStep();

    if (potentially_a_function_call && pc_before_step + 4 != power_pc.GetPPCState().pc)
      ++function_call_depth_from_start;
  }

  power_pc.SetMode(old_mode);
  return CreateVoidTypeArgHolder();
}

ArgHolder* Skip(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  if (!IsCurrentlyInBreakpoint())
    return CreateNotInBreakpointError("Skip()");

  Core::CPUThreadGuard guard(Core::System::GetInstance());
  Core::System::GetInstance().GetPowerPC().GetPPCState().pc =
      Core::System::GetInstance().GetPowerPC().GetPPCState().pc + 4;

  return CreateVoidTypeArgHolder();
}

ArgHolder* SetPC(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  u32 new_pc = (*args_list)[0]->u32_val;
  if (!IsCurrentlyInBreakpoint())
    return CreateNotInBreakpointError("SetPC");

  Core::CPUThreadGuard guard(Core::System::GetInstance());
  Core::System::GetInstance().GetPowerPC().GetPPCState().pc = new_pc;

  return CreateVoidTypeArgHolder();
}

ArgHolder* GetInstructionFromAddress(ScriptContext* current_script,
                                     std::vector<ArgHolder*>* args_list)
{
  u32 instruction_addr = (*args_list)[0]->u32_val;
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  const u32 op = PowerPC::MMU::HostRead_Instruction(guard, instruction_addr);
  std::string disasm = Common::GekkoDisassembler::Disassemble(op, instruction_addr);
  return CreateStringArgHolder(disasm);
}
}  // namespace Scripting::InstructionStepAPI
