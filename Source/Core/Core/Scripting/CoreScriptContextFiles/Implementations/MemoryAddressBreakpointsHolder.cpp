#include "Core/Scripting/CoreScriptContextFiles/Implementations/MemoryAddressBreakpointsHolder.h"

void MemoryAddressBreakpointsHolder_AddReadBreakpoint_impl(void* _this, unsigned int addr)
{
  // add this to the list of breakpoints regardless of whether or not it's a duplicate
  reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this)->read_breakpoint_addresses.push_back(
      addr);
}

unsigned int MemoryAddressBreakpointsHolder_GetNumReadCopiesOfBreakpoint_impl(void* _this,
                                                                         unsigned int addr)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  return std::count(casted_struct_ptr->read_breakpoint_addresses.begin(),
                    casted_struct_ptr->read_breakpoint_addresses.end(), addr);
}

unsigned int MemoryAddressBreakpointsHolder_ContainsReadBreakpoint_impl(void* _this, unsigned int addr)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  if (std::count(casted_struct_ptr->read_breakpoint_addresses.begin(),
                 casted_struct_ptr->read_breakpoint_addresses.end(), addr) > 0)
    return 1;
  else
    return 0;
}

void MemoryAddressBreakpointsHolder_RemoveReadBreakpoint_impl(void* _this, unsigned int addr)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  auto it = std::find(casted_struct_ptr->read_breakpoint_addresses.begin(),
                      casted_struct_ptr->read_breakpoint_addresses.end(), addr);
  if (it != casted_struct_ptr->read_breakpoint_addresses.end())
    casted_struct_ptr->read_breakpoint_addresses.erase(it);
}

void MemoryAddressBreakpointsHolder_AddWriteBreakpoint_impl(void* _this, unsigned int addr)
{
  reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this)->write_breakpoint_addresses.push_back(
      addr);  // add this to the list of breakpoints regardless of whether or not it's a duplicate
}

unsigned int MemoryAddressBreakpointsHolder_GetNumWriteCopiesOfBreakpoint_impl(void* _this,
                                                                          unsigned int addr)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  return std::count(casted_struct_ptr->write_breakpoint_addresses.begin(),
                    casted_struct_ptr->write_breakpoint_addresses.end(), addr);
}

unsigned int MemoryAddressBreakpointsHolder_ContainsWriteBreakpoint_impl(void* _this, unsigned int addr)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  if (std::count(casted_struct_ptr->write_breakpoint_addresses.begin(),
                 casted_struct_ptr->write_breakpoint_addresses.end(), addr) > 0)
    return 1;
  else
    return 0;
}

void MemoryAddressBreakpointsHolder_RemoveWriteBreakpoint_impl(void* _this, unsigned int addr)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  auto it = std::find(casted_struct_ptr->write_breakpoint_addresses.begin(),
                      casted_struct_ptr->write_breakpoint_addresses.end(), addr);
  if (it != casted_struct_ptr->write_breakpoint_addresses.end())
    casted_struct_ptr->write_breakpoint_addresses.erase(it);
}

unsigned int MemoryAddressBreakpointsHolder_RemoveReadBreakpoints_OneByOne_impl(void* _this)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  if (casted_struct_ptr->read_breakpoint_addresses.size() == 0)
    return 0;
  else
  {
    unsigned int ret_val = casted_struct_ptr->read_breakpoint_addresses[0];
    casted_struct_ptr->read_breakpoint_addresses.erase(
        casted_struct_ptr->read_breakpoint_addresses.begin());
    return ret_val;
  }
}

unsigned int MemoryAddressBreakpointsHolder_RemoveWriteBreakpoints_OneByOne_impl(void* _this)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  if (casted_struct_ptr->write_breakpoint_addresses.size() == 0)
    return 0;
  else
  {
    unsigned int ret_val = casted_struct_ptr->write_breakpoint_addresses[0];
    casted_struct_ptr->write_breakpoint_addresses.erase(
        casted_struct_ptr->write_breakpoint_addresses.begin());
    return ret_val;
  }
}
