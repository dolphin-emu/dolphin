#include "Core/Scripting/HelperClasses/MemoryAddressBreakpointsHolder.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

MemoryAddressBreakpointsHolder::MemoryAddressBreakpointsHolder()
{
  this->read_breakpoint_addresses = std::vector<unsigned int>();
  this->write_breakpoint_addresses = std::vector<unsigned int>();
}

MemoryAddressBreakpointsHolder::~MemoryAddressBreakpointsHolder()
{
  this->RemoveAllBreakpoints();
}

void MemoryAddressBreakpointsHolder::AddReadBreakpoint(unsigned int addr)
{
  TMemCheck check;

  check.start_address = addr;
  check.end_address = addr;
  check.is_break_on_read = true;
  check.is_break_on_write = this->ContainsWriteBreakpoint(addr);
  check.condition = std::nullopt;
  check.break_on_hit = true;

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));

  this->read_breakpoint_addresses.push_back(
      addr);  // add this to the list of breakpoints regardless of whether or not it's a duplicate
}

bool MemoryAddressBreakpointsHolder::ContainsReadBreakpoint(unsigned int addr)
{
  return (std::count(this->read_breakpoint_addresses.begin(), this->read_breakpoint_addresses.end(),
                     addr) > 0);
}

void MemoryAddressBreakpointsHolder::RemoveReadBreakpoint(unsigned int addr)
{
  auto it = std::find(this->read_breakpoint_addresses.begin(),
                      this->read_breakpoint_addresses.end(), addr);
  if (it != this->read_breakpoint_addresses.end())
    this->read_breakpoint_addresses.erase(it);

  if (this->ContainsReadBreakpoint(addr) || this->ContainsWriteBreakpoint(addr))
  {
    TMemCheck check;

    check.start_address = addr;
    check.end_address = addr;
    check.is_break_on_read = this->ContainsReadBreakpoint(addr);
    check.is_break_on_write = this->ContainsWriteBreakpoint(addr);
    check.condition = std::nullopt;
    check.break_on_hit = true;

    Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));
  }
  else
  {
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Remove(addr);
  }
}

void MemoryAddressBreakpointsHolder::AddWriteBreakpoint(unsigned int addr)
{
  TMemCheck check;

  check.start_address = addr;
  check.end_address = addr;
  check.is_break_on_read = this->ContainsReadBreakpoint(addr);
  check.is_break_on_write = true;
  check.condition = std::nullopt;
  check.break_on_hit = true;

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));

  this->write_breakpoint_addresses.push_back(
      addr);  // add this to the list of breakpoints regardless of whether or not it's a duplicate
}

bool MemoryAddressBreakpointsHolder::ContainsWriteBreakpoint(unsigned int addr)
{
  return (std::count(this->write_breakpoint_addresses.begin(),
                     this->write_breakpoint_addresses.end(), addr) > 0);
}

void MemoryAddressBreakpointsHolder::RemoveWriteBreakpoint(unsigned int addr)
{
  auto it = std::find(this->write_breakpoint_addresses.begin(),
                      this->write_breakpoint_addresses.end(), addr);
  if (it != this->write_breakpoint_addresses.end())
    this->write_breakpoint_addresses.erase(it);

  if (this->ContainsReadBreakpoint(addr) || this->ContainsWriteBreakpoint(addr))
  {
    TMemCheck check;

    check.start_address = addr;
    check.end_address = addr;
    check.is_break_on_read = this->ContainsReadBreakpoint(addr);
    check.is_break_on_write = this->ContainsWriteBreakpoint(addr);
    check.condition = std::nullopt;
    check.break_on_hit = true;

    Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));
  }
  else
  {
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Remove(addr);
  }
}

void MemoryAddressBreakpointsHolder::RemoveAllBreakpoints()
{
  while (read_breakpoint_addresses.size() > 0)
    RemoveReadBreakpoint(read_breakpoint_addresses[0]);
  while (write_breakpoint_addresses.size() > 0)
    RemoveWriteBreakpoint(write_breakpoint_addresses[0]);
}
