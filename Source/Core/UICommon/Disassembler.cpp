#include <disasm.h>  // Bochs

#if defined(HAVE_LLVM)
// PowerPC.h defines PC.
// This conflicts with a function that has an argument named PC
#undef PC
#include <llvm-c/Disassembler.h>
#include <llvm-c/Target.h>
#endif

#include "Common/StringUtil.h"

#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/JitInterface.h"

#include "UICommon/Disassembler.h"

class HostDisassemblerX86 : public HostDisassembler
{
public:
  HostDisassemblerX86();

private:
  disassembler m_disasm;

  std::string DisassembleHostBlock(const u8* code_start, const u32 code_size,
                                   u32* host_instructions_count, u64 starting_pc) override;
};

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

  m_llvm_context = LLVMCreateDisasmCPU(host_disasm.c_str(), cpu.c_str(), nullptr, 0, 0, nullptr);

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
        std::string inst_str = "";
        for (int i = 0; i < m_instruction_size; ++i)
          inst_str += StringFromFormat("%02x", disasmPtr[i]);

        x86_disasm << inst_str << std::endl;
        disasmPtr += m_instruction_size;
      }
      else
      {
        // We can't continue if we are on an architecture that has flexible instruction sizes
        // Dump the rest of the block instead
        std::string code_block = "";
        for (int i = 0; (disasmPtr + i) < end; ++i)
          code_block += StringFromFormat("%02x", disasmPtr[i]);

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
#endif

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

std::string DisassembleBlock(HostDisassembler* disasm, u32* address, u32* host_instructions_count,
                             u32* code_size)
{
  const u8* code;
  int res = JitInterface::GetHostCode(address, &code, code_size);

  if (res == 1)
  {
    *host_instructions_count = 0;
    return "(No JIT active)";
  }
  else if (res == 2)
  {
    host_instructions_count = 0;
    return "(No translation)";
  }
  return disasm->DisassembleHostBlock(code, *code_size, host_instructions_count, (u64)code);
}
