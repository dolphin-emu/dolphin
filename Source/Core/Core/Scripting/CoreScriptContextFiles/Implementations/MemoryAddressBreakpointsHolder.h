#ifndef MEMORY_ADDRESS_BREAKPOINTS_HOLDER_IMPL
#define MEMORY_ADDRESS_BREAKPOINTS_HOLDER_IMPL

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct MemoryAddressBreakpointsHolder
{
    std::vector<unsigned int> read_breakpoint_addresses;
    std::vector<unsigned int> write_breakpoint_addresses;
} MemoryAddressBreakpointsHolder;

  // In each of the below functions, the void* parameter  _this represents a MemoryAddressBreakpointsHolder*
void MemoryAddressBreakpointsHolder_AddReadBreakpoint_impl(void* _this, unsigned int addr);
unsigned int MemoryAddressBreakpointsHolder_GetNumReadCopiesOfBreakpoint_impl(void* _this, unsigned int addr);
unsigned int MemoryAddressBreakpointsHolder_ContainsReadBreakpoint_impl(void* _this, unsigned int addr);
void MemoryAddressBreakpointsHolder_RemoveReadBreakpoint_impl(void* _this, unsigned int addr);
void MemoryAddressBreakpointsHolder_AddWriteBreakpoint_impl(void* _this, unsigned int addr);
unsigned int MemoryAddressBreakpointsHolder_GetNumWriteCopiesOfBreakpoint_impl(void* _this, unsigned int addr);
unsigned int MemoryAddressBreakpointsHolder_ContainsWriteBreakpoint_impl(void* _this, unsigned int addr);
void MemoryAddressBreakpointsHolder_RemoveWriteBreakpoint_impl(void* _this, unsigned int addr);
unsigned int MemoryAddressBreakpointsHolder_RemoveReadBreakpoints_OneByOne_impl(void* _this);
unsigned int MemoryAddressBreakpointsHolder_RemoveWriteBreakpoints_OneByOne_impl(void* _this);

#ifdef __cplusplus
}
#endif

#endif
