#pragma once

#include <vector>
#include "Core/PowerPC/PowerPC.h"

// This is a helper class which stores all memory address read/write breakpoints for a particular
// ScriptContext*
class MemoryAddressBreakpointsHolder
{
public:
  MemoryAddressBreakpointsHolder();
  ~MemoryAddressBreakpointsHolder();
  void AddReadBreakpoint(unsigned int start_address, unsigned int end_address);
  void AddWriteBreakpoint(unsigned int start_address, unsigned int end_address);
  bool ContainsReadBreakpoint(unsigned int start_address) const;
  bool ContainsWriteBreakpoint(unsigned int start_address) const;
  void RemoveReadBreakpoint(unsigned int start_address);
  void RemoveWriteBreakpoint(unsigned int start_address);
  void RemoveAllBreakpoints();

private:
  struct MemCheckAndCounts
  {
    uint64_t start_address{};
    uint64_t end_address{};
    uint64_t read_breakpoint_count{};
    uint64_t write_breakpoint_count{};

    MemCheckAndCounts() = default;
  };

  int GetIndexOfStartAddress(unsigned int) const;

  // Sets the breakpoint to have its values based on the corresponding entry in the
  // memory_breakpoints list at the specified inex.
  void PopulateBreakpointFromValueAtIndex(TMemCheck& check, int index);

  //  We keep track of how many times we've added each read+write breakpoint
  // for a given start address. Note that when a breakpoint is added with
  // the same start address as another breakpoint, the breakpoint is updated
  // to have the larger end address between the original breakpoint and
  // the new one.
  std::vector<MemCheckAndCounts> memory_breakpoints;
};
