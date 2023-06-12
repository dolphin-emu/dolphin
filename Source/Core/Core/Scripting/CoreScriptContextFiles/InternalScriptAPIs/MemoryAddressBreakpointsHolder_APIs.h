#ifndef MEMORY_ADDRESS_BREAKPOINTS_HOLDER
#define MEMORY_ADDRESS_BREAKPOINTS_HOLDER

#ifdef __cplusplus
extern "C" {
#endif


// The first void* parameter for each function represents a MemoryAddressBreakpointsHolder*
// The MemoryBreakpointsHolder struct is basically 2 lists of unsigned ints which represent the
// memory address read breakpoints that have been set and the memory address write breakpoints that have been set.
typedef struct MemoryAddressBreakpointsHolder_APIs
{
  void (*AddReadBreakpoint)(void*, unsigned int); // Adds a read breakpoint to the MemoryAddressBreakpointsHolder* passed into the function.
  int (*ContainsReadBreakpoint)(void*, unsigned int); // Returns 1 if the MemoryAddressBreakpointsHolder* contains a read breakpoint for the specified memory address, and returns 0 otherwise.
  void (*RemoveReadBreakpoint)(void*, unsigned int); // Removes 1 copy of the specified read-from-memory-address breakpoint from the MemoryAddressBreakpointsHolder*
  void (*AddWriteBreakpoint)(void*, unsigned int); // Adds a write breakpoint to the MemoryAddressBreakpointsHolder* passed into the function.
  int (*ContainsWriteBreakpoint)(void*, unsigned int); // Returns 1 if the MemoryAddressBreakpointsHolder* contains a write breakpoint for the specified memory address, and returns 0 otherwise.
  void (*RemoveWriteBreakpoint)(void*, unsigned int); // Removes 1 copy of the specified write-to-memory-address breakpoint from the MemoryAddressBreakpointsHolder*
  unsigned int (*RemoveReadBreakpoints_OneByOne)(void*); // Removes 1 breakpoint from the list of read breakpoints of the MemoryAddressBreakpointsHolder*, and returns the address of the read breakpoint that was removed. If all read breakpoints have been removed, this function returns 0.
  unsigned int (*RemoveWriteBreakpoints_OneByOne)(void*); // Removes 1 breakpoint from the list of write breakpoints of the MemoryAddressBreakpointsHolder*, and returns the address of the write breakpoint that was removed. If all write breakpoints have been removed, this function returns 0.
} MemoryAddressBreakpointsHolder_APIs;

#ifdef __cplusplus
}
#endif

#endif
