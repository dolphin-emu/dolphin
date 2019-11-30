// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"

#ifdef _WX_EXCLUSIVITY
struct UnprotectedRegion
{
  u8* start;
  size_t size;
};
#endif

namespace Common
{
// Everything that needs to generate code should inherit from this.
// You get memory management for free, plus, you can use all emitter functions without
// having to prefix them with gen-> or something similar.
// Example implementation:
// class JIT : public CodeBlock<ARMXEmitter> {}
template <class T>
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
    region = static_cast<u8*>(Common::AllocateExecutableMemory(total_region_size));
    T::SetCodePtr(region);
  }

  // Always clear code space with breakpoints, so that if someone accidentally executes
  // uninitialized, it just breaks into the debugger.
  void ClearCodeSpace()
  {
#ifdef _WX_EXCLUSIVITY
    Common::UnWriteProtectMemory(region, region_size);
#endif

    PoisonMemory();

#ifdef _WX_EXCLUSIVITY
    Common::WriteProtectMemory(region, region_size, true);
#endif

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

  // These functions should be used when writing to the code space to ensure that the destination
  // page(s) is/are writable on platforms that enforce W^X.

  // WriteCode will allow writing to all pages starting at the page containing the current code
  // pointer and ending at the end of the region.
  template <typename Callable>
  void WriteCode(Callable write_code)
  {
#ifdef _WX_EXCLUSIVITY
    const size_t page_size = Common::PageSize();

    // There is a chance that the caller can write to the child blocks, so every
    // child block should also be marked as writable just in case.
    for (CodeBlock* child : m_children)
    {
      Common::UnWriteProtectMemory(child->region, child->region_size);
    }

    // Round the code pointer down to the nearest page boundary.
    u8* write_ptr = reinterpret_cast<u8*>(reinterpret_cast<uint64_t>(T::GetWritableCodePtr()) &
                                          ~(page_size - 1));

    // All memory starting at the current page until the end of the region is unprotected.
    size_t size = (region + region_size) - write_ptr;

    // Round up to the next page size if necessary.
    if (size % page_size != 0)
    {
      size = size + page_size - (size % page_size);
    }

    Common::UnWriteProtectMemory(write_ptr, size);
#endif

    write_code();

#ifdef _WX_EXCLUSIVITY
    // Reprotect all memory that was unprotected.
    Common::WriteProtectMemory(write_ptr, size, true);

    for (CodeBlock* child : m_children)
    {
      Common::WriteProtectMemory(child->region, child->region_size, true);
    }
#endif
  }

  // WriteCodeAtRegion should be used if writing to a specific area within the code space
  // is needed. However, it is much slower than WriteCode as it prevents regions from being
  // reprotected when they may still be written to later. Use WriteCode whenever possible.
  template <typename Callable>
  void WriteCodeAtRegion(Callable write_code, u8* start_ptr, size_t size = 0)
  {
#ifdef _WX_EXCLUSIVITY
    const size_t page_size = Common::PageSize();

    // Round the start pointer down to the nearest page boundary.
    u8* write_ptr = reinterpret_cast<u8*>(reinterpret_cast<uint64_t>(start_ptr) & ~(page_size - 1));
    size_t new_size = size + start_ptr - write_ptr;

    // Round up to the next page size if necessary.
    if (new_size % page_size != 0)
    {
      new_size = new_size + page_size - (new_size % page_size);
    }

    // Check the permissions of all pages within the bounds. This is to prevent any pages from
    // being write-protected again even though the JIT still needs to write to them elsewhere.
    std::vector<UnprotectedRegion> unprotected_regions;
    for (size_t unprotected_size = 0; unprotected_size <= new_size; unprotected_size += page_size)
    {
      u8* ptr = write_ptr + unprotected_size;

      if (Common::IsMemoryPageExecutable(ptr))
      {
        if (unprotected_regions.size() != 0)
        {
          UnprotectedRegion& unprotected_region = unprotected_regions.back();
          if (unprotected_region.start + unprotected_region.size == ptr)
          {
            // Expand the previous region.
            unprotected_region.size += page_size;
            continue;
          }
        }

        // Create a new region that should be unprotected.
        UnprotectedRegion new_region;
        new_region.start = ptr;
        new_region.size = page_size;

        unprotected_regions.push_back(new_region);
      }
    }

    for (UnprotectedRegion& unprotected_region : unprotected_regions)
    {
      Common::UnWriteProtectMemory(unprotected_region.start, unprotected_region.size);
    }
#endif

    write_code();

#ifdef _WX_EXCLUSIVITY
    for (UnprotectedRegion& unprotected_region : unprotected_regions)
    {
      Common::WriteProtectMemory(unprotected_region.start, unprotected_region.size, true);
    }
#endif
  }

  bool IsInSpace(const u8* ptr) const { return ptr >= region && ptr < (region + region_size); }
  // Cannot currently be undone. Will write protect the entire code region.
  // Start over if you need to change the code (call FreeCodeSpace(), AllocCodeSpace()).
  void WriteProtect() { Common::WriteProtectMemory(region, region_size, true); }
  void ResetCodePtr() { T::SetCodePtr(region); }
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
    ASSERT_MSG(DYNA_REC, child_size < GetSpaceLeft(), "Insufficient space for child allocation.");
    u8* child_region = region + region_size - child_size;
    region_size -= child_size;
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
