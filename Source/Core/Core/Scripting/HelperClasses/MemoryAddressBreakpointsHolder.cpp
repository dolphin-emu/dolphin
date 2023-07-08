#include "Core/Scripting/HelperClasses/MemoryAddressBreakpointsHolder.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

MemoryAddressBreakpointsHolder::MemoryAddressBreakpointsHolder()
    : memory_breakpoints(std::vector<MemCheckAndCounts>())
{
}

MemoryAddressBreakpointsHolder::~MemoryAddressBreakpointsHolder()
{
  this->RemoveAllBreakpoints();
}

void MemoryAddressBreakpointsHolder::AddReadBreakpoint(unsigned int start_address,
                                                       unsigned int end_address)

{
  int indexOfBreakpoint = GetIndexOfStartAddress(start_address);
  if (indexOfBreakpoint == -1)
  {
    memory_breakpoints.push_back(MemCheckAndCounts());
    indexOfBreakpoint = static_cast<int>(memory_breakpoints.size()) - 1;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.start_address = start_address;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.end_address = end_address;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.is_break_on_read = true;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.is_break_on_write = false;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.condition = std::nullopt;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.break_on_hit = true;
    memory_breakpoints[indexOfBreakpoint].read_breakpoint_count = 1;
  }
  else
  {
    if (end_address > memory_breakpoints[indexOfBreakpoint].memory_breakpoint.end_address)
      memory_breakpoints[indexOfBreakpoint].memory_breakpoint.end_address = end_address;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.is_break_on_read = true;
    memory_breakpoints[indexOfBreakpoint].read_breakpoint_count++;
  }

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(
      std::move(memory_breakpoints[indexOfBreakpoint].memory_breakpoint));
}

void MemoryAddressBreakpointsHolder::AddWriteBreakpoint(unsigned int start_address,
                                                        unsigned int end_address)
{
  int indexOfBreakpoint = GetIndexOfStartAddress(start_address);
  if (indexOfBreakpoint == -1)
  {
    memory_breakpoints.push_back(MemCheckAndCounts());
    indexOfBreakpoint = static_cast<int>(memory_breakpoints.size()) - 1;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.start_address = start_address;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.end_address = end_address;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.is_break_on_read = false;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.is_break_on_write = true;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.condition = std::nullopt;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.break_on_hit = true;
    memory_breakpoints[indexOfBreakpoint].write_breakpoint_count = 1;
  }
  else
  {
    if (end_address > memory_breakpoints[indexOfBreakpoint].memory_breakpoint.end_address)
      memory_breakpoints[indexOfBreakpoint].memory_breakpoint.end_address = end_address;
    memory_breakpoints[indexOfBreakpoint].memory_breakpoint.is_break_on_write = true;
    memory_breakpoints[indexOfBreakpoint].write_breakpoint_count++;
  }

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(
      std::move(memory_breakpoints[indexOfBreakpoint].memory_breakpoint));
}

int MemoryAddressBreakpointsHolder::GetIndexOfStartAddress(unsigned int start_address) const
{
  int list_size = static_cast<int>(memory_breakpoints.size());
  for (int i = 0; i < list_size; ++i)
  {
    if (memory_breakpoints[i].memory_breakpoint.start_address == start_address)
      return i;
  }
  return -1;
}

bool MemoryAddressBreakpointsHolder::ContainsReadBreakpoint(unsigned int start_address) const
{
  int index_of_breakpoint = GetIndexOfStartAddress(start_address);
  return (index_of_breakpoint != -1 &&
          memory_breakpoints[index_of_breakpoint].read_breakpoint_count > 0);
}

bool MemoryAddressBreakpointsHolder::ContainsWriteBreakpoint(unsigned int start_address) const
{
  int index_of_breakpoint = GetIndexOfStartAddress(start_address);
  return (index_of_breakpoint != -1 &&
          memory_breakpoints[index_of_breakpoint].write_breakpoint_count > 0);
}

void MemoryAddressBreakpointsHolder::RemoveReadBreakpoint(unsigned int start_address)
{
  int index_of_last_read_breakpoint = GetIndexOfStartAddress(start_address);
  if (index_of_last_read_breakpoint == -1 ||
      memory_breakpoints[index_of_last_read_breakpoint].read_breakpoint_count == 0)
    return;

  memory_breakpoints[index_of_last_read_breakpoint].read_breakpoint_count--;
  if (memory_breakpoints[index_of_last_read_breakpoint].read_breakpoint_count == 0)
    memory_breakpoints[index_of_last_read_breakpoint].memory_breakpoint.is_break_on_read = false;

  if (this->ContainsReadBreakpoint(start_address) || this->ContainsWriteBreakpoint(start_address))
  {
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(
        std::move(memory_breakpoints[index_of_last_read_breakpoint].memory_breakpoint));
  }
  else
  {
    memory_breakpoints.erase(memory_breakpoints.begin() + index_of_last_read_breakpoint);
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Remove(start_address);
  }
}

void MemoryAddressBreakpointsHolder::RemoveWriteBreakpoint(unsigned int start_address)
{
  int index_of_last_write_breakpoint = GetIndexOfStartAddress(start_address);
  if (index_of_last_write_breakpoint == -1 ||
      memory_breakpoints[index_of_last_write_breakpoint].write_breakpoint_count == 0)
    return;

  memory_breakpoints[index_of_last_write_breakpoint].write_breakpoint_count--;
  if (memory_breakpoints[index_of_last_write_breakpoint].write_breakpoint_count == 0)
    memory_breakpoints[index_of_last_write_breakpoint].memory_breakpoint.is_break_on_write = false;

  if (this->ContainsReadBreakpoint(start_address) || this->ContainsWriteBreakpoint(start_address))
  {
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(
        std::move(memory_breakpoints[index_of_last_write_breakpoint].memory_breakpoint));
  }
  else
  {
    memory_breakpoints.erase(memory_breakpoints.begin() + index_of_last_write_breakpoint);
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Remove(start_address);
  }
}

void MemoryAddressBreakpointsHolder::RemoveAllBreakpoints()
{
  while (memory_breakpoints.size() > 0)
  {
    if (memory_breakpoints[0].read_breakpoint_count > 0)
      RemoveReadBreakpoint(memory_breakpoints[0].memory_breakpoint.start_address);
    else
      RemoveWriteBreakpoint(memory_breakpoints[0].memory_breakpoint.start_address);
  }
}
