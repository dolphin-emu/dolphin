// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <iosfwd>
#include <type_traits>

#include "Common/CodeBlock.h"
#include "Common/CommonTypes.h"

namespace PowerPC
{
struct PowerPCState;
}

class CachedInterpreterEmitter
{
protected:
  // The return value of most callbacks is the distance in memory to the next callback.
  // If a callback returns 0, the block will be exited. The return value is signed to
  // support block-linking. 32-bit return values seem to perform better than 64-bit ones.
  template <class Operands>
  using Callback = s32 (*)(PowerPC::PowerPCState& ppc_state, const Operands& operands);
  using AnyCallback = s32 (*)(PowerPC::PowerPCState& ppc_state, const void* operands);

  template <class Operands>
  static consteval Callback<Operands> CallbackCast(Callback<Operands> callback)
  {
    return callback;
  }
  template <class Operands>
  static AnyCallback AnyCallbackCast(Callback<Operands> callback)
  {
    return reinterpret_cast<AnyCallback>(callback);
  }
  static consteval AnyCallback AnyCallbackCast(AnyCallback callback) { return callback; }

  // Disassemble callbacks will always return the distance to the next callback.
  template <class Operands>
  using Disassemble = s32 (*)(std::ostream& stream, const Operands& operands);
  using AnyDisassemble = s32 (*)(std::ostream& stream, const void* operands);

  template <class Operands>
  static AnyDisassemble AnyDisassembleCast(Disassemble<Operands> disassemble)
  {
    return reinterpret_cast<AnyDisassemble>(disassemble);
  }
  static consteval AnyDisassemble AnyDisassembleCast(AnyDisassemble disassemble)
  {
    return disassemble;
  }

public:
  CachedInterpreterEmitter() = default;
  explicit CachedInterpreterEmitter(u8* begin, u8* end) : m_code(begin), m_code_end(end) {}

  template <class Operands>
  void Write(Callback<Operands> callback, const Operands& operands)
  {
    // I would use std::is_trivial_v, but almost every operands struct uses
    // references instead of pointers to make the callback functions nicer.
    static_assert(
        std::is_trivially_copyable_v<Operands> && std::is_trivially_destructible_v<Operands> &&
        alignof(Operands) <= alignof(AnyCallback) && sizeof(Operands) % alignof(AnyCallback) == 0);
    Write(AnyCallbackCast(callback), &operands, sizeof(Operands));
  }
  void Write(AnyCallback callback) { Write(callback, nullptr, 0); }

  const u8* GetCodePtr() const { return m_code; }
  u8* GetWritableCodePtr() { return m_code; }
  const u8* GetCodeEnd() const { return m_code_end; }
  u8* GetWritableCodeEnd() { return m_code_end; }
  // Should be checked after a block of code has been generated to see if the code has been
  // successfully written to memory. Do not call the generated code when this returns true!
  bool HasWriteFailed() const { return m_write_failed; }

  void SetCodePtr(u8* begin, u8* end)
  {
    m_code = begin;
    m_code_end = end;
    m_write_failed = false;
  }

  static s32 PoisonCallback(PowerPC::PowerPCState& ppc_state, const void* operands);
  static s32 PoisonCallback(std::ostream& stream, const void* operands);

private:
  void Write(AnyCallback callback, const void* operands, std::size_t size);

  // Pointer to memory where code will be emitted to.
  u8* m_code = nullptr;
  // Pointer past the end of the memory region we're allowed to emit to.
  // Writes that would reach this memory are refused and will set the m_write_failed flag instead.
  u8* m_code_end = nullptr;
  // Set to true when a write request happens that would write past m_code_end.
  // Must be cleared with SetCodePtr() afterwards.
  bool m_write_failed = false;
};

class CachedInterpreterCodeBlock : public Common::CodeBlock<CachedInterpreterEmitter, false>
{
private:
  void PoisonMemory() override;
};
