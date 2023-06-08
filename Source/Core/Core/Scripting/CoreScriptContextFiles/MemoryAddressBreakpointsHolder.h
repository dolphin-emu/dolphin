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
  void (*AddReadBreakpoint)(struct MemoryAddressBreakpointsHolder*, u32);
  int (*ContainsReadBreakpoint)(struct MemoryAddressBreakpointsHolder*, u32);
  void (*RemoveReadBreakpoint)(struct MemoryAddressBreakpointsHolder*, u32);
  void (*AddWriteBreakpoint)(struct MemoryAddressBreakpointsHolder*, u32);
  int (*ContainsWriteBreakpoint)(struct MemoryAddressBreakpointsHolder*, u32);
  void (*RemoveWriteBreakpoint)(struct MemoryAddressBreakpointsHolder*, u32);
  u32 (*RemoveReadBreakpoints_OneByOne)(struct MemoryAddressBreakpointsHolder*);
  u32 (*RemoveWriteBreakpoints_OneByOne)(struct MemoryAddressBreakpointsHolder*);
} MemoryAddressBreakpointsHolder;


  void MemoryAddressBreakpointsHolder_AddReadBreakpoint(MemoryAddressBreakpointsHolder* __this, u32 addr)
  {
    __this->read_breakpoint_addresses.push_back(&(__this->read_breakpoint_addresses), reinterpret_cast<void*>(addr));  // add this to the list of breakpoints regardless of whether or not it's a duplicate
  }

  int MemoryAddressBreakpointsHolder_GetNumReadCopiesOfBreakpoint(MemoryAddressBreakpointsHolder* __this, u32 addr)
  {
    return __this->read_breakpoint_addresses.count(&(__this->read_breakpoint_addresses), reinterpret_cast<void*>(addr));
  }

  int MemoryAddressBreakpointsHolder_ContainsReadBreakpoint(MemoryAddressBreakpointsHolder* __this, u32 addr)
  {
    return __this->read_breakpoint_addresses.count(&(__this->read_breakpoint_addresses), reinterpret_cast<void*>(addr)) > 0;
  }

  void MemoryAddressBreakpointsHolder_RemoveReadBreakpoint(MemoryAddressBreakpointsHolder* __this, u32 addr)
  {
    if (MemoryAddressBreakpointsHolder_ContainsReadBreakpoint(__this, addr))
      __this->read_breakpoint_addresses.remove_by_value(&(__this->read_breakpoint_addresses), reinterpret_cast<void*>(addr));
  }

  void MemoryAddressBreakpointsHolder_AddWriteBreakpoint(MemoryAddressBreakpointsHolder* __this, u32 addr)
  {
    __this->write_breakpoint_addresses.push_back(&(__this->write_breakpoint_addresses), reinterpret_cast<void*>(addr)); // add this to the list of breakpoints regardless of whether or not it's a duplicate
  }

  int MemoryAddressBreakpointsHolder_GetNumWriteCopiesOfBreakpoint(MemoryAddressBreakpointsHolder* __this, u32 addr)
  {
    return __this->write_breakpoint_addresses.count(&(__this->write_breakpoint_addresses), reinterpret_cast<void*>(addr));
  }

  int MemoryAddressBreakpointsHolder_ContainsWriteBreakpoint(MemoryAddressBreakpointsHolder* __this, u32 addr)
  {
    return __this->write_breakpoint_addresses.count(&(__this->write_breakpoint_addresses), reinterpret_cast<void*>(addr)) > 0;
  }

  void MemoryAddressBreakpointsHolder_RemoveWriteBreakpoint(MemoryAddressBreakpointsHolder* __this, u32 addr)
  {
    if (MemoryAddressBreakpointsHolder_ContainsWriteBreakpoint(__this, addr))
      __this->write_breakpoint_addresses.remove_by_value(&(__this->write_breakpoint_addresses), reinterpret_cast<void*>(addr));
  }

  u32 MemoryAddressBreakpointsHolder_RemoveReadBreakpoints_OneByOne(MemoryAddressBreakpointsHolder* __this)
  {
    u32 ret_val = 0;
    if (__this->read_breakpoint_addresses.length > 0)
    {
      ret_val = reinterpret_cast<u32>(__this->read_breakpoint_addresses.get(&(__this->read_breakpoint_addresses), __this->read_breakpoint_addresses.length - 1));
      __this->read_breakpoint_addresses.pop(&(__this->read_breakpoint_addresses));
    }
    return ret_val;
  }

  u32 MemoryAddressBreakpointsHolder_RemoveWriteBreakpoints_OneByOne(MemoryAddressBreakpointsHolder* __this)
  {
    u32 ret_val = 0;
    if (__this->write_breakpoint_addresses.length > 0)
    {
      ret_val = reinterpret_cast<u32>(__this->write_breakpoint_addresses.get(&(__this->write_breakpoint_addresses), __this->write_breakpoint_addresses.length - 1));
      __this->write_breakpoint_addresses.pop(&(__this->write_breakpoint_addresses));
    }
    return ret_val;
  }

  MemoryAddressBreakpointsHolder MemoryAddressBreakpointsHolder_Initializer()
  {
    MemoryAddressBreakpointsHolder ret_val;
    ret_val.read_breakpoint_addresses = Vector_Initializer(NULL);
    ret_val.write_breakpoint_addresses = Vector_Initializer(NULL);
    ret_val.AddReadBreakpoint = MemoryAddressBreakpointsHolder_AddReadBreakpoint;
    ret_val.AddWriteBreakpoint = MemoryAddressBreakpointsHolder_AddWriteBreakpoint;
    ret_val.ContainsReadBreakpoint = MemoryAddressBreakpointsHolder_ContainsReadBreakpoint;
    ret_val.ContainsWriteBreakpoint = MemoryAddressBreakpointsHolder_ContainsWriteBreakpoint;
    ret_val.RemoveReadBreakpoint = MemoryAddressBreakpointsHolder_RemoveReadBreakpoint;
    ret_val.RemoveWriteBreakpoint = MemoryAddressBreakpointsHolder_RemoveWriteBreakpoint;
    ret_val.RemoveReadBreakpoints_OneByOne = MemoryAddressBreakpointsHolder_RemoveReadBreakpoints_OneByOne;
    ret_val.RemoveWriteBreakpoints_OneByOne = MemoryAddressBreakpointsHolder_RemoveWriteBreakpoints_OneByOne;
    return ret_val;
  }

   void MemoryAddressBreakpointsHolder_Destructor(MemoryAddressBreakpointsHolder* __this)
  {
    Vector_Destructor(&(__this->read_breakpoint_addresses));
    Vector_Destructor(&(__this->write_breakpoint_addresses));
    memset(__this, 0, sizeof(MemoryAddressBreakpointsHolder));
  }
  
#endif

#ifdef __cplusplus
}
#endif
