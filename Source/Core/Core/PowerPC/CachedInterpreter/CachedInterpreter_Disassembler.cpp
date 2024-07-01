// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include <array>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "Core/HLE/HLE.h"

s32 CachedInterpreterEmitter::PoisonCallback(std::ostream& stream, const void* operands)
{
  stream << "PoisonCallback()\n";
  return sizeof(AnyCallback);
}

s32 CachedInterpreter::EndBlock(std::ostream& stream, const EndBlockOperands& operands)
{
  const auto& [downcount, num_load_stores, num_fp_inst] = operands;
  fmt::println(stream, "EndBlock(downcount={}, num_load_stores={}, num_fp_inst={})", downcount,
               num_load_stores, num_fp_inst);
  return sizeof(AnyCallback) + sizeof(operands);
}

template <bool check_exceptions, bool write_pc>
s32 CachedInterpreter::Interpret(std::ostream& stream,
                                 const InterpretOperands<check_exceptions>& operands)
{
  if constexpr (check_exceptions)
  {
    fmt::println(stream,
                 "Interpret<check_exceptions=true , write_pc={:5}>(current_pc=0x{:08x}, "
                 "inst=0x{:08x}, downcount={})",
                 write_pc, operands.current_pc, operands.inst.hex, operands.downcount);
  }
  else
  {
    fmt::println(
        stream,
        "Interpret<check_exceptions=false, write_pc={:5}>(current_pc=0x{:08x}, inst=0x{:08x})",
        write_pc, operands.current_pc, operands.inst.hex);
  }
  return sizeof(AnyCallback) + sizeof(operands);
}

s32 CachedInterpreter::HLEFunction(std::ostream& stream, const HLEFunctionOperands& operands)
{
  const auto& [system, current_pc, hook_index] = operands;
  fmt::println(stream, "HLEFunction(current_pc=0x{:08x}, hook_index={}) [\"{}\"]", current_pc,
               hook_index, HLE::GetHookNameByIndex(hook_index));
  return sizeof(AnyCallback) + sizeof(operands);
}

s32 CachedInterpreter::WriteBrokenBlockNPC(std::ostream& stream,
                                           const WriteBrokenBlockNPCOperands& operands)
{
  const auto& [current_pc] = operands;
  fmt::println(stream, "WriteBrokenBlockNPC(current_pc=0x{:08x})", current_pc);
  return sizeof(AnyCallback) + sizeof(operands);
}

s32 CachedInterpreter::CheckFPU(std::ostream& stream, const CheckFPUOperands& operands)
{
  const auto& [power_pc, current_pc, downcount] = operands;
  fmt::println(stream, "CheckFPU(current_pc=0x{:08x}, downcount={})", current_pc, downcount);
  return sizeof(AnyCallback) + sizeof(operands);
}

s32 CachedInterpreter::CheckBreakpoint(std::ostream& stream,
                                       const CheckBreakpointOperands& operands)
{
  const auto& [power_pc, cpu_state, current_pc, downcount] = operands;
  fmt::println(stream, "CheckBreakpoint(current_pc=0x{:08x}, downcount={})", current_pc, downcount);
  return sizeof(AnyCallback) + sizeof(operands);
}

s32 CachedInterpreter::CheckIdle(std::ostream& stream, const CheckIdleOperands& operands)
{
  const auto& [core_timing, idle_pc] = operands;
  fmt::println(stream, "CheckIdle(idle_pc=0x{:08x})", idle_pc);
  return sizeof(AnyCallback) + sizeof(operands);
}

void CachedInterpreter::Disassemble(const JitBlock& block, std::ostream& stream,
                                    std::size_t& instruction_count)
{
  using LookupKV = std::pair<AnyCallback, AnyDisassemble>;
  // clang-format off
#define LOOKUP_KV(name, ...) {AnyCallbackCast(name __VA_OPT__(, ) __VA_ARGS__), AnyDisassembleCast(name __VA_OPT__(, ) __VA_ARGS__)}
  // clang-format on
  static const auto sorted_lookup = []() {
    auto unsorted_lookup = std::to_array<LookupKV>({
        LOOKUP_KV(CachedInterpreter::PoisonCallback),
        LOOKUP_KV(CachedInterpreter::EndBlock),
        LOOKUP_KV(CachedInterpreter::Interpret<false, false>),
        LOOKUP_KV(CachedInterpreter::Interpret<true, false>),
        LOOKUP_KV(CachedInterpreter::Interpret<false, true>),
        LOOKUP_KV(CachedInterpreter::Interpret<true, true>),
        LOOKUP_KV(CachedInterpreter::HLEFunction),
        LOOKUP_KV(CachedInterpreter::WriteBrokenBlockNPC),
        LOOKUP_KV(CachedInterpreter::CheckFPU),
        LOOKUP_KV(CachedInterpreter::CheckBreakpoint),
        LOOKUP_KV(CachedInterpreter::CheckIdle),
    });
#undef LOOKUP_KV
    std::ranges::sort(unsorted_lookup, {}, &LookupKV::first);
    ASSERT_MSG(DYNA_REC,
               std::ranges::adjacent_find(unsorted_lookup, {}, &LookupKV::first) ==
                   unsorted_lookup.end(),
               "Sorted lookup should not contain duplicate keys.");
    return unsorted_lookup;
  }();

  instruction_count = 0;
  for (const u8* normal_entry = block.normalEntry; normal_entry != block.near_end;
       ++instruction_count)
  {
    const auto callback = *reinterpret_cast<const AnyCallback*>(normal_entry);
    const auto kv = std::ranges::lower_bound(sorted_lookup, callback, {}, &LookupKV::first);
    if (kv != sorted_lookup.end() && kv->first == callback)
    {
      normal_entry += kv->second(stream, normal_entry + sizeof(AnyCallback));
      continue;
    }
    stream << "UNKNOWN OR ILLEGAL CALLBACK\n";
    break;
  }
}
