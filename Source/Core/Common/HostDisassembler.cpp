// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/HostDisassembler.h"

#include <span>
#include <sstream>

#include <fmt/format.h>
#include <fmt/ostream.h>

#if defined(HAVE_LLVM)
#include <llvm-c/Disassembler.h>
#include <llvm-c/Target.h>
#elif defined(_M_X86_64)
#include <disasm.h>  // Bochs
#endif

#if defined(HAVE_LLVM)
class HostDisassemblerLLVM final : public HostDisassembler
{
public:
  explicit HostDisassemblerLLVM(const char* host_disasm, const char* cpu = "",
                                std::size_t inst_size = 0);
  ~HostDisassemblerLLVM();

private:
  LLVMDisasmContextRef m_llvm_context;
  std::size_t m_instruction_size;

  std::size_t Disassemble(const u8* begin, const u8* end, std::ostream& stream) override;
};

HostDisassemblerLLVM::HostDisassemblerLLVM(const char* host_disasm, const char* cpu,
                                           std::size_t inst_size)
    : m_instruction_size(inst_size)
{
  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllDisassemblers();

  m_llvm_context = LLVMCreateDisasmCPU(host_disasm, cpu, nullptr, 0, nullptr, nullptr);

  // Couldn't create llvm context
  if (!m_llvm_context)
    return;

  LLVMSetDisasmOptions(m_llvm_context, LLVMDisassembler_Option_AsmPrinterVariant |
                                           LLVMDisassembler_Option_PrintLatency);
}

HostDisassemblerLLVM::~HostDisassemblerLLVM()
{
  if (m_llvm_context)
    LLVMDisasmDispose(m_llvm_context);
}

std::size_t HostDisassemblerLLVM::Disassemble(const u8* begin, const u8* end, std::ostream& stream)
{
  std::size_t instruction_count = 0;
  if (!m_llvm_context)
    return instruction_count;

  while (begin < end)
  {
    char inst_disasm[256];

    const auto inst_size =
        LLVMDisasmInstruction(m_llvm_context, const_cast<u8*>(begin), static_cast<u64>(end - begin),
                              reinterpret_cast<u64>(begin), inst_disasm, sizeof(inst_disasm));
    if (inst_size == 0)
    {
      if (m_instruction_size != 0)
      {
        // If we are on an architecture that has a fixed instruction
        // size, we can continue onward past this bad instruction.
        fmt::println(stream, "{}\tInvalid inst: {:02x}", fmt::ptr(begin),
                     fmt::join(std::span{begin, m_instruction_size}, ""));
        begin += m_instruction_size;
      }
      else
      {
        // We can't continue if we are on an architecture that has flexible
        // instruction sizes. Dump the rest of the block instead.
        fmt::println(stream, "{}\tInvalid inst: {:02x}", fmt::ptr(begin),
                     fmt::join(std::span{begin, end}, ""));
        break;
      }
    }
    else
    {
      fmt::println(stream, "{}{}", fmt::ptr(begin), inst_disasm);
      begin += inst_size;
    }
    ++instruction_count;
  }
  return instruction_count;
}
#elif defined(_M_X86_64)
class HostDisassemblerBochs final : public HostDisassembler
{
public:
  explicit HostDisassemblerBochs();
  ~HostDisassemblerBochs() = default;

private:
  disassembler m_disasm;

  std::size_t Disassemble(const u8* begin, const u8* end, std::ostream& stream) override;
};

HostDisassemblerBochs::HostDisassemblerBochs()
{
  m_disasm.set_syntax_intel();
}

std::size_t HostDisassemblerBochs::Disassemble(const u8* begin, const u8* end, std::ostream& stream)
{
  std::size_t instruction_count = 0;
  while (begin < end)
  {
    char inst_disasm[256];

    const auto inst_size =
        m_disasm.disasm64(reinterpret_cast<bx_address>(begin), reinterpret_cast<bx_address>(begin),
                          begin, inst_disasm);
    fmt::println(stream, "{}\t{}", fmt::ptr(begin), inst_disasm);
    begin += inst_size;
    ++instruction_count;
  }
  return instruction_count;
}
#endif

std::unique_ptr<HostDisassembler> HostDisassembler::Factory(Platform arch)
{
  switch (arch)
  {
#if defined(HAVE_LLVM)
  case Platform::x86_64:
    return std::make_unique<HostDisassemblerLLVM>("x86_64-none-unknown");
  case Platform::aarch64:
    return std::make_unique<HostDisassemblerLLVM>("aarch64-none-unknown", "cortex-a57", 4);
#elif defined(_M_X86_64)
  case Platform::x86_64:
    return std::make_unique<HostDisassemblerBochs>();
#else
  case Platform{}:  // warning C4065: "switch statement contains 'default' but no 'case' labels"
#endif
  default:
    return std::make_unique<HostDisassembler>();
  }
}

std::size_t HostDisassembler::Disassemble(const u8* begin, const u8* end, std::ostream& stream)
{
  fmt::println(stream, "{}\t{:02x}", fmt::ptr(begin), fmt::join(std::span{begin, end}, ""));
  return 0;
}
