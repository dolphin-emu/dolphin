#ifndef MEMORY_ADDRESS_BREAKPOINTS_HOLDER_IMPL
#define MEMORY_ADDRESS_BREAKPOINTS_HOLDER_IMPL

#include <utility>
#include <vector>

#include "Core/PowerPC/PowerPC.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    TMemCheck memory_breakpoint;
    unsigned long long read_breakpoint_count;
    unsigned long long write_breakpoint_count;

    //TODO: Fix this code.
    MemCheckAndCounts()
    {
      memory_breakpoint = TMemCheck();
      read_breakpoint_count = 0;
      write_breakpoint_count = 0;
    }
    MemCheckAndCounts(const MemCheckAndCounts& o)
    {
      //memory_breakpoint = std::move(o.memory_breakpoint);
      read_breakpoint_count = o.read_breakpoint_count;
      write_breakpoint_count = o.write_breakpoint_count;
    }
    MemCheckAndCounts& operator=(const MemCheckAndCounts&) { return *this;
    }
  };

  int GetIndexOfStartAddress(unsigned int) const;

  //  We keep track of how many times we've added each read+write breakpoint
  // for a given start address. Note that when a breakpoint is added with
  // the same start address as another breakpoint the breakpoint is updated
  // to have the larger end address between the original breakpoint and
  // the new one.
  std::vector<MemCheckAndCounts> memory_breakpoints;
};

#ifdef __cplusplus
}
#endif

#endif
