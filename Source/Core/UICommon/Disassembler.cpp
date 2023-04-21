// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/Disassembler.h"

#include <sstream>

#if defined(HAVE_LLVM)
#include <fmt/format.h>
#include <llvm-c/Disassembler.h>
#include <llvm-c/Target.h>
#elif defined(_M_X86)
#include <disasm.h>  // Bochs
#endif

#include "Common/Assert.h"
#include "Common/VariantUtil.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/System.h"

#if defined(HAVE_LLVM)
class HostDisassemblerLLVM : public HostDisassembler
{
public:
  HostDisassemblerLLVM(const std::string& host_disasm, int inst_size = -1,
                       const std::string& cpu = "");
  ~HostDisassemblerLLVM()
  {
    if (m_can_disasm)
      LLVMDisasmDispose(m_llvm_context);
  }

private:
  bool m_can_disasm;
  LLVMDisasmContextRef m_llvm_context;
  int m_instruction_size;

  std::string DisassembleHostBlock(const u8* code_start, const u32 code_size,
                                   u32* host_instructions_count, u64 starting_pc) override;
};

HostDisassemblerLLVM::HostDisassemblerLLVM(const std::string& host_disasm, int inst_size,
                                           const std::string& cpu)
    : m_can_disasm(false), m_instruction_size(inst_size)
{
  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllDisassemblers();

  m_llvm_context =
      LLVMCreateDisasmCPU(host_disasm.c_str(), cpu.c_str(), nullptr, 0, nullptr, nullptr);

  // Couldn't create llvm context
  if (!m_llvm_context)
    return;

  LLVMSetDisasmOptions(m_llvm_context, LLVMDisassembler_Option_AsmPrinterVariant |
                                           LLVMDisassembler_Option_PrintLatency);

  m_can_disasm = true;
}

std::string HostDisassemblerLLVM::DisassembleHostBlock(const u8* code_start, const u32 code_size,
                                                       u32* host_instructions_count,
                                                       u64 starting_pc)
{
  if (!m_can_disasm)
    return "(No LLVM context)";

  u8* disasmPtr = (u8*)code_start;
  const u8* end = code_start + code_size;

  std::ostringstream x86_disasm;
  while ((u8*)disasmPtr < end)
  {
    char inst_disasm[256];
    size_t inst_size = LLVMDisasmInstruction(m_llvm_context, disasmPtr, (u64)(end - disasmPtr),
                                             starting_pc, inst_disasm, 256);

    x86_disasm << "0x" << std::hex << starting_pc << "\t";
    if (!inst_size)
    {
      x86_disasm << "Invalid inst:";

      if (m_instruction_size != -1)
      {
        // If we are on an architecture that has a fixed instruction size
        // We can continue onward past this bad instruction.
        std::string inst_str;
        for (int i = 0; i < m_instruction_size; ++i)
          inst_str += fmt::format("{:02x}", disasmPtr[i]);

        x86_disasm << inst_str << std::endl;
        disasmPtr += m_instruction_size;
      }
      else
      {
        // We can't continue if we are on an architecture that has flexible instruction sizes
        // Dump the rest of the block instead
        std::string code_block;
        for (int i = 0; (disasmPtr + i) < end; ++i)
          code_block += fmt::format("{:02x}", disasmPtr[i]);

        x86_disasm << code_block << std::endl;
        break;
      }
    }
    else
    {
      x86_disasm << inst_disasm << std::endl;
      disasmPtr += inst_size;
      starting_pc += inst_size;
    }

    (*host_instructions_count)++;
  }

  return x86_disasm.str();
}
#elif defined(_M_X86)
class HostDisassemblerX86 : public HostDisassembler
{
public:
  HostDisassemblerX86();

private:
  disassembler m_disasm;

  std::string DisassembleHostBlock(const u8* code_start, const u32 code_size,
                                   u32* host_instructions_count, u64 starting_pc) override;
};

HostDisassemblerX86::HostDisassemblerX86()
{
  m_disasm.set_syntax_intel();
}

std::string HostDisassemblerX86::DisassembleHostBlock(const u8* code_start, const u32 code_size,
                                                      u32* host_instructions_count, u64 starting_pc)
{
  u64 disasmPtr = (u64)code_start;
  const u8* end = code_start + code_size;

  std::ostringstream x86_disasm;
  while ((u8*)disasmPtr < end)
  {
    char inst_disasm[256];
    disasmPtr += m_disasm.disasm64(disasmPtr, disasmPtr, (u8*)disasmPtr, inst_disasm);
    x86_disasm << inst_disasm << std::endl;
    (*host_instructions_count)++;
  }

  return x86_disasm.str();
}
#endif

std::unique_ptr<HostDisassembler> GetNewDisassembler(const std::string& arch)
{
#if defined(HAVE_LLVM)
  if (arch == "x86")
    return std::make_unique<HostDisassemblerLLVM>("x86_64-none-unknown");
  if (arch == "aarch64")
    return std::make_unique<HostDisassemblerLLVM>("aarch64-none-unknown", 4, "cortex-a57");
  if (arch == "armv7")
    return std::make_unique<HostDisassemblerLLVM>("armv7-none-unknown", 4, "cortex-a15");
#elif defined(_M_X86)
  if (arch == "x86")
    return std::make_unique<HostDisassemblerX86>();
#endif
  return std::make_unique<HostDisassembler>();
}

DisassembleResult DisassembleBlock(HostDisassembler* disasm, u32 address)
{
  auto res = Core::System::GetInstance().GetJitInterface().GetHostCode(address);

  return std::visit(overloaded{[&](JitInterface::GetHostCodeError error) {
                                 DisassembleResult result;
                                 switch (error)
                                 {
                                 case JitInterface::GetHostCodeError::NoJitActive:
                                   result.text = "(No JIT active)";
                                   break;
                                 case JitInterface::GetHostCodeError::NoTranslation:
                                   result.text = "(No translation)";
                                   break;
                                 default:
                                   ASSERT(false);
                                   break;
                                 }
                                 result.entry_address = address;
                                 result.instruction_count = 0;
                                 result.code_size = 0;
                                 return result;
                               },
                               [&](JitInterface::GetHostCodeResult host_result) {
                                 DisassembleResult new_result;
                                 u32 instruction_count = 0;
                                 new_result.text = disasm->DisassembleHostBlock(
                                     host_result.code, host_result.code_size, &instruction_count,
                                     (u64)host_result.code);
                                 new_result.entry_address = host_result.entry_address;
                                 new_result.code_size = host_result.code_size;
                                 new_result.instruction_count = instruction_count;
                                 return new_result;
                               }},
                    res);
}
