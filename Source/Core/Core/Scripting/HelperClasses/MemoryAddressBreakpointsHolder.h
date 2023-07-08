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
    unsigned long long start_address;
    unsigned long long end_address;
    unsigned long long read_breakpoint_count;
    unsigned long long write_breakpoint_count;

    MemCheckAndCounts()
    {
      start_address = 0;
      end_address = 0;
      read_breakpoint_count = 0;
      write_breakpoint_count = 0;
    }
  };

  int GetIndexOfStartAddress(unsigned int) const;
  void PopulateBreakpointFromValueAtIndex(TMemCheck& check, int index);
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
