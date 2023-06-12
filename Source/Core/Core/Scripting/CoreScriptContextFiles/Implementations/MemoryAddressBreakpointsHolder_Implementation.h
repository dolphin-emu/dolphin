#ifdef __cplusplus
extern "C" {
#endif

  #include <vector>

  typedef struct MemoryAddressBreakpointsHolder
{
    std::vector<unsigned int> read_breakpoint_addresses;
    std::vector<unsigned int> write_breakpoint_addresses;
} MemoryAddressBreakpointsHolder;

  // In each of the below functions, the void* parameter  _this represents a MemoryAddressBreakpointsHolder*

  void MemoryAddressBreakpointsHolder_AddReadBreakpoint(void* _this, unsigned int addr)
{
    // add this to the list of breakpoints regardless of whether or not it's a duplicate
    reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this)->read_breakpoint_addresses.push_back(addr);
}

unsigned int MemoryAddressBreakpointsHolder_GetNumReadCopiesOfBreakpoint(void* _this, unsigned int addr)
{
    MemoryAddressBreakpointsHolder* casted_struct_ptr = reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
    return std::count(casted_struct_ptr->read_breakpoint_addresses.begin(), casted_struct_ptr->read_breakpoint_addresses.end(), addr);
}

unsigned int MemoryAddressBreakpointsHolder_ContainsReadBreakpoint(void* _this,
                                                          unsigned int addr)
{
    MemoryAddressBreakpointsHolder* casted_struct_ptr = reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
    if (std::count(casted_struct_ptr->read_breakpoint_addresses.begin(), casted_struct_ptr->read_breakpoint_addresses.end(), addr) > 0)
      return 1;
    else
      return 0;
}

void MemoryAddressBreakpointsHolder_RemoveReadBreakpoint(void* _this, unsigned int addr)
{
    MemoryAddressBreakpointsHolder* casted_struct_ptr = reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
    auto it = std::find(casted_struct_ptr->read_breakpoint_addresses.begin(), casted_struct_ptr->read_breakpoint_addresses.end(), addr);
    if (it != casted_struct_ptr->read_breakpoint_addresses.end())
      casted_struct_ptr->read_breakpoint_addresses.erase(it);
}

void MemoryAddressBreakpointsHolder_AddWriteBreakpoint(void* _this, unsigned int addr)
{
  reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this)->write_breakpoint_addresses.push_back(addr); // add this to the list of breakpoints regardless of whether or not it's a duplicate
}

unsigned int MemoryAddressBreakpointsHolder_GetNumWriteCopiesOfBreakpoint(void* _this, unsigned int addr)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr = reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  return std::count(casted_struct_ptr->write_breakpoint_addresses.begin(), casted_struct_ptr->write_breakpoint_addresses.end(), addr);
}

unsigned int MemoryAddressBreakpointsHolder_ContainsWriteBreakpoint(void* _this, unsigned int addr)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr = reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  if (std::count(casted_struct_ptr->write_breakpoint_addresses.begin(), casted_struct_ptr->write_breakpoint_addresses.end(), addr) > 0)
      return 1;
  else
      return 0;
}

void MemoryAddressBreakpointsHolder_RemoveWriteBreakpoint(void* _this, unsigned int addr)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr = reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  auto it = std::find(casted_struct_ptr->write_breakpoint_addresses.begin(), casted_struct_ptr->write_breakpoint_addresses.end(), addr);
  if (it != casted_struct_ptr->write_breakpoint_addresses.end())
      casted_struct_ptr->write_breakpoint_addresses.erase(it);
}

unsigned int MemoryAddressBreakpointsHolder_RemoveReadBreakpoints_OneByOne(void* _this)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr = reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  if (casted_struct_ptr->read_breakpoint_addresses.size() == 0)
      return 0;
  else
  {
      unsigned int ret_val = casted_struct_ptr->read_breakpoint_addresses[0];
      casted_struct_ptr->read_breakpoint_addresses.erase(casted_struct_ptr->read_breakpoint_addresses.begin());
      return ret_val;
  }
}

unsigned int MemoryAddressBreakpointsHolder_RemoveWriteBreakpoints_OneByOne(void* _this)
{
  MemoryAddressBreakpointsHolder* casted_struct_ptr = reinterpret_cast<MemoryAddressBreakpointsHolder*>(_this);
  if (casted_struct_ptr->write_breakpoint_addresses.size() == 0)
      return 0;
  else
  {
      unsigned int ret_val = casted_struct_ptr->write_breakpoint_addresses[0];
      casted_struct_ptr->write_breakpoint_addresses.erase(casted_struct_ptr->write_breakpoint_addresses.begin());
      return ret_val;
  }
}

#ifdef __cplusplus
}
#endif
