// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"

namespace Common
{
// Everything that needs to generate code should inherit from this.
// You get memory management for free, plus, you can use all emitter functions without
// having to prefix them with gen-> or something similar.
// Example implementation:
// class JIT : public CodeBlock<ARMXEmitter> {}
template <class T, bool executable = true>
class CodeBlock : public T
{
private:
  // A privately used function to set the executable RAM space to something invalid.
  // For debugging usefulness it should be used to set the RAM to a host specific breakpoint
  // instruction
  virtual void PoisonMemory() = 0;

protected:
  u8* region = nullptr;
  // Size of region we can use.
  size_t region_size = 0;
  // Original size of the region we allocated.
  size_t total_region_size = 0;

  bool m_is_child = false;
  std::vector<CodeBlock*> m_children;

public:
  CodeBlock() = default;
  virtual ~CodeBlock()
  {
    if (region)
      FreeCodeSpace();
  }
  CodeBlock(const CodeBlock&) = delete;
  CodeBlock& operator=(const CodeBlock&) = delete;
  CodeBlock(CodeBlock&&) = delete;
  CodeBlock& operator=(CodeBlock&&) = delete;

  // Call this before you generate any code.
  void AllocCodeSpace(size_t size)
  {
    region_size = size;
    total_region_size = size;
    if constexpr (executable)
      region = static_cast<u8*>(Common::AllocateExecutableMemory(total_region_size));
    else
      region = static_cast<u8*>(Common::AllocateMemoryPages(total_region_size));
    T::SetCodePtr(region, region + size);
  }

  // Always clear code space with breakpoints, so that if someone accidentally executes
  // uninitialized, it just breaks into the debugger.
  void ClearCodeSpace()
  {
    PoisonMemory();
    ResetCodePtr();
  }

  // Call this when shutting down. Don't rely on the destructor, even though it'll do the job.
  void FreeCodeSpace()
  {
    ASSERT(!m_is_child);
    Common::FreeMemoryPages(region, total_region_size);
    region = nullptr;
    region_size = 0;
    total_region_size = 0;
    for (CodeBlock* child : m_children)
    {
      child->region = nullptr;
      child->region_size = 0;
      child->total_region_size = 0;
    }
  }

  bool IsInSpace(const u8* ptr) const { return ptr >= region && ptr < (region + region_size); }
  bool IsInSpaceOrChildSpace(const u8* ptr) const
  {
    return ptr >= region && ptr < (region + total_region_size);
  }
  void WriteProtect(bool allow_execute)
  {
    Common::WriteProtectMemory(region, region_size, allow_execute);
  }
  void UnWriteProtect(bool allow_execute)
  {
    Common::UnWriteProtectMemory(region, region_size, allow_execute);
  }
  void ResetCodePtr() { T::SetCodePtr(region, region + region_size); }
  size_t GetSpaceLeft() const
  {
    ASSERT(static_cast<size_t>(T::GetCodePtr() - region) < region_size);
    return region_size - (T::GetCodePtr() - region);
  }

  bool IsAlmostFull() const
  {
    // This should be bigger than the biggest block ever.
    return GetSpaceLeft() < 0x10000;
  }

  bool HasChildren() const { return region_size != total_region_size; }
  u8* AllocChildCodeSpace(size_t child_size)
  {
    ASSERT_MSG(DYNA_REC, child_size <= GetSpaceLeft(), "Insufficient space for child allocation.");
    u8* child_region = region + region_size - child_size;
    region_size -= child_size;
    ResetCodePtr();
    return child_region;
  }
  void AddChildCodeSpace(CodeBlock* child, size_t child_size)
  {
    u8* child_region = AllocChildCodeSpace(child_size);
    child->m_is_child = true;
    child->region = child_region;
    child->region_size = child_size;
    child->total_region_size = child_size;
    child->ResetCodePtr();
    m_children.emplace_back(child);
  }
};
}  // namespace Common
