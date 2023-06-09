#ifdef __cplusplus
extern "C" {
#endif

#ifndef MEMORY_ADDRESS_BREAKPOINTS_HOLDER
#define MEMORY_ADDRESS_BREAKPOINTS_HOLDER

#include "Core/Scripting/CoreScriptContextFiles/Vector.h"

typedef struct MemoryAddressBreakpointsHolder
{
  Vector read_breakpoint_addresses;
  Vector write_breakpoint_addresses;
  void (*AddReadBreakpoint)(struct MemoryAddressBreakpointsHolder*, unsigned int);
  int (*ContainsReadBreakpoint)(struct MemoryAddressBreakpointsHolder*, unsigned int);
  void (*RemoveReadBreakpoint)(struct MemoryAddressBreakpointsHolder*, unsigned int);
  void (*AddWriteBreakpoint)(struct MemoryAddressBreakpointsHolder*, unsigned int);
  int (*ContainsWriteBreakpoint)(struct MemoryAddressBreakpointsHolder*, unsigned int);
  void (*RemoveWriteBreakpoint)(struct MemoryAddressBreakpointsHolder*, unsigned int);
  unsigned int (*RemoveReadBreakpoints_OneByOne)(struct MemoryAddressBreakpointsHolder*);
  unsigned int (*RemoveWriteBreakpoints_OneByOne)(struct MemoryAddressBreakpointsHolder*);
} MemoryAddressBreakpointsHolder;


void MemoryAddressBreakpointsHolder_AddReadBreakpoint(MemoryAddressBreakpointsHolder* __this, unsigned int addr);
int MemoryAddressBreakpointsHolder_GetNumReadCopiesOfBreakpoint(MemoryAddressBreakpointsHolder* __this, unsigned int addr);
int MemoryAddressBreakpointsHolder_ContainsReadBreakpoint(MemoryAddressBreakpointsHolder* __this, unsigned int addr);
void MemoryAddressBreakpointsHolder_RemoveReadBreakpoint(MemoryAddressBreakpointsHolder* __this, unsigned int addr);
void MemoryAddressBreakpointsHolder_AddWriteBreakpoint(MemoryAddressBreakpointsHolder* __this, unsigned int addr);
int MemoryAddressBreakpointsHolder_GetNumWriteCopiesOfBreakpoint(MemoryAddressBreakpointsHolder* __this, unsigned int addr);
int MemoryAddressBreakpointsHolder_ContainsWriteBreakpoint(MemoryAddressBreakpointsHolder* __this, unsigned int addr);
void MemoryAddressBreakpointsHolder_RemoveWriteBreakpoint(MemoryAddressBreakpointsHolder* __this, unsigned int addr);
unsigned int MemoryAddressBreakpointsHolder_RemoveReadBreakpoints_OneByOne(MemoryAddressBreakpointsHolder* __this);
unsigned int MemoryAddressBreakpointsHolder_RemoveWriteBreakpoints_OneByOne(MemoryAddressBreakpointsHolder* __this);
MemoryAddressBreakpointsHolder MemoryAddressBreakpointsHolder_Initializer();
void MemoryAddressBreakpointsHolder_Destructor(MemoryAddressBreakpointsHolder* __this);
  
#endif

#ifdef __cplusplus
}
#endif
