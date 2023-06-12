#include "Core/Scripting/CoreScriptContextFiles/Implementations/MemoryAddressBreakpointsHolder.h"

MemoryAddressBreakpointsHolder::MemoryAddressBreakpointsHolder()
{
  this->read_breakpoint_addresses = std::vector<unsigned int>();
  this->write_breakpoint_addresses = std::vector<unsigned int>();
}

void MemoryAddressBreakpointsHolder::AddReadBreakpoint(unsigned int addr)
{
  this->read_breakpoint_addresses.push_back(addr);   // add this to the list of breakpoints regardless of whether or not it's a duplicate
}

unsigned int MemoryAddressBreakpointsHolder::GetNumReadCopiesOfBreakpoint(unsigned int addr)
{
  return std::count(this->read_breakpoint_addresses.begin(), this->read_breakpoint_addresses.end(), addr);
}

bool MemoryAddressBreakpointsHolder::ContainsReadBreakpoint(unsigned int addr)
{
  return (std::count(this->read_breakpoint_addresses.begin(), this->read_breakpoint_addresses.end(), addr) > 0);
}

void MemoryAddressBreakpointsHolder::RemoveReadBreakpoint(unsigned int addr)
{
  auto it = std::find(this->read_breakpoint_addresses.begin(), this->read_breakpoint_addresses.end(), addr);
  if (it != this->read_breakpoint_addresses.end())
    this->read_breakpoint_addresses.erase(it);
}

void MemoryAddressBreakpointsHolder::AddWriteBreakpoint(unsigned int addr)
{
  this->write_breakpoint_addresses.push_back(addr);  // add this to the list of breakpoints regardless of whether or not it's a duplicate
}

unsigned int MemoryAddressBreakpointsHolder::GetNumWriteCopiesOfBreakpoint(unsigned int addr)
{
  return std::count(this->write_breakpoint_addresses.begin(), this->write_breakpoint_addresses.end(), addr);
}

bool MemoryAddressBreakpointsHolder::ContainsWriteBreakpoint(unsigned int addr)
{
  return (std::count(this->write_breakpoint_addresses.begin(), this->write_breakpoint_addresses.end(), addr) > 0);
}

void MemoryAddressBreakpointsHolder::RemoveWriteBreakpoint(unsigned int addr)
{
  auto it = std::find(this->write_breakpoint_addresses.begin(), this->write_breakpoint_addresses.end(), addr);
  if (it != this->write_breakpoint_addresses.end())
    this->write_breakpoint_addresses.erase(it);
}

unsigned int MemoryAddressBreakpointsHolder::RemoveReadBreakpoints_OneByOne()
{
  if (this->read_breakpoint_addresses.size() == 0)
    return 0;
  else
  {
    unsigned int ret_val = this->read_breakpoint_addresses[0];
    this->read_breakpoint_addresses.erase(this->read_breakpoint_addresses.begin());
    return ret_val;
  }
}

unsigned int MemoryAddressBreakpointsHolder::RemoveWriteBreakpoints_OneByOne()
{
  if (this->write_breakpoint_addresses.size() == 0)
    return 0;
  else
  {
    unsigned int ret_val = this->write_breakpoint_addresses[0];
    this->write_breakpoint_addresses.erase(this->write_breakpoint_addresses.begin());
    return ret_val;
  }
}
