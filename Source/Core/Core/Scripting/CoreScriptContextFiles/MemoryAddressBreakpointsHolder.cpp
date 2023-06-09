#ifdef __cplusplus
extern "C" {
#endif

  #include "Core/Scripting/CoreScriptContextFiles/MemoryAddressBreakpointsHolder.h"

  void MemoryAddressBreakpointsHolder_AddReadBreakpoint(MemoryAddressBreakpointsHolder* __this, unsigned int addr)
{
  __this->read_breakpoint_addresses.push_back(
      &(__this->read_breakpoint_addresses),
      (void*)*((void**)&addr));  // add this to the list of breakpoints regardless of whether or not
                                 // it's a duplicate
}

int MemoryAddressBreakpointsHolder_GetNumReadCopiesOfBreakpoint(
    MemoryAddressBreakpointsHolder* __this, unsigned int addr)
{
  return __this->read_breakpoint_addresses.count(&(__this->read_breakpoint_addresses),
                                                 (void*)*((void**)&addr));
}

int MemoryAddressBreakpointsHolder_ContainsReadBreakpoint(MemoryAddressBreakpointsHolder* __this,
                                                          unsigned int addr)
{
  return __this->read_breakpoint_addresses.count(&(__this->read_breakpoint_addresses),
                                                 (void*)*((void**)&addr)) > 0;
}

void MemoryAddressBreakpointsHolder_RemoveReadBreakpoint(MemoryAddressBreakpointsHolder* __this,
                                                         unsigned int addr)
{
  if (MemoryAddressBreakpointsHolder_ContainsReadBreakpoint(__this, addr))
    __this->read_breakpoint_addresses.remove_by_value(&(__this->read_breakpoint_addresses),
                                                      (void*)*((void**)&addr));
}

void MemoryAddressBreakpointsHolder_AddWriteBreakpoint(MemoryAddressBreakpointsHolder* __this,
                                                       unsigned int addr)
{
  __this->write_breakpoint_addresses.push_back(
      &(__this->write_breakpoint_addresses),
      (void*)*((void**)&addr));  // add this to the list of breakpoints regardless of whether or
                                 // not it's a duplicate
}

int MemoryAddressBreakpointsHolder_GetNumWriteCopiesOfBreakpoint(
    MemoryAddressBreakpointsHolder* __this, unsigned int addr)
{
  return __this->write_breakpoint_addresses.count(&(__this->write_breakpoint_addresses),
                                                  (void*)*((void**)&addr));
}

int MemoryAddressBreakpointsHolder_ContainsWriteBreakpoint(MemoryAddressBreakpointsHolder* __this,
                                                           unsigned int addr)
{
  return __this->write_breakpoint_addresses.count(&(__this->write_breakpoint_addresses),
                                                  (void*)*((void**)&addr)) > 0;
}

void MemoryAddressBreakpointsHolder_RemoveWriteBreakpoint(MemoryAddressBreakpointsHolder* __this,
                                                          unsigned int addr)
{
  if (MemoryAddressBreakpointsHolder_ContainsWriteBreakpoint(__this, addr))
    __this->write_breakpoint_addresses.remove_by_value(&(__this->write_breakpoint_addresses),
                                                       (void*)*((void**)&addr));
}

unsigned int MemoryAddressBreakpointsHolder_RemoveReadBreakpoints_OneByOne(
    MemoryAddressBreakpointsHolder* __this)
{
  unsigned int ret_val = 0;
  if (__this->read_breakpoint_addresses.length > 0)
  {
    void* temp = __this->read_breakpoint_addresses.get(
        &(__this->read_breakpoint_addresses), __this->read_breakpoint_addresses.length - 1);
    ret_val = *((unsigned int*)(&temp));
    __this->read_breakpoint_addresses.pop(&(__this->read_breakpoint_addresses));
  }
  return ret_val;
}

unsigned int MemoryAddressBreakpointsHolder_RemoveWriteBreakpoints_OneByOne(
    MemoryAddressBreakpointsHolder* __this)
{
  unsigned int ret_val = 0;
  if (__this->write_breakpoint_addresses.length > 0)
  {
    void* temp = __this->write_breakpoint_addresses.get(
        &(__this->write_breakpoint_addresses), __this->write_breakpoint_addresses.length - 1);
    ret_val = *((unsigned int*)&temp);
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
  ret_val.RemoveReadBreakpoints_OneByOne =
      MemoryAddressBreakpointsHolder_RemoveReadBreakpoints_OneByOne;
  ret_val.RemoveWriteBreakpoints_OneByOne =
      MemoryAddressBreakpointsHolder_RemoveWriteBreakpoints_OneByOne;
  return ret_val;
}

void MemoryAddressBreakpointsHolder_Destructor(MemoryAddressBreakpointsHolder* __this)
{
  Vector_Destructor(&(__this->read_breakpoint_addresses));
  Vector_Destructor(&(__this->write_breakpoint_addresses));
  memset(__this, 0, sizeof(MemoryAddressBreakpointsHolder));
}

#ifdef __cplusplus
}
#endif
