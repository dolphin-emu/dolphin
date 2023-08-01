#include "Core/Scripting/HelperClasses/MemoryAddressBreakpointsHolder.h"

#include "Core/System.h"

MemoryAddressBreakpointsHolder::MemoryAddressBreakpointsHolder() = default;

MemoryAddressBreakpointsHolder::~MemoryAddressBreakpointsHolder()
{
  RemoveAllBreakpoints();
}

void MemoryAddressBreakpointsHolder::AddReadBreakpoint(unsigned int start_address,
                                                       unsigned int end_address)

{
  int index_of_breakpoint = GetIndexOfStartAddress(start_address);
  if (index_of_breakpoint == -1)
  {
    memory_breakpoints.push_back(MemCheckAndCounts());
    index_of_breakpoint = static_cast<int>(memory_breakpoints.size()) - 1;
    memory_breakpoints[index_of_breakpoint].start_address = start_address;
    memory_breakpoints[index_of_breakpoint].end_address = end_address;
    memory_breakpoints[index_of_breakpoint].read_breakpoint_count = 1;
  }
  else
  {
    if (end_address > memory_breakpoints[index_of_breakpoint].end_address)
      memory_breakpoints[index_of_breakpoint].end_address = end_address;
    memory_breakpoints[index_of_breakpoint].read_breakpoint_count++;
  }

  TMemCheck check = {};
  PopulateBreakpointFromValueAtIndex(check, index_of_breakpoint);

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));
}

void MemoryAddressBreakpointsHolder::AddWriteBreakpoint(unsigned int start_address,
                                                        unsigned int end_address)
{
  int index_of_breakpoint = GetIndexOfStartAddress(start_address);
  if (index_of_breakpoint == -1)
  {
    memory_breakpoints.push_back(MemCheckAndCounts());
    index_of_breakpoint = static_cast<int>(memory_breakpoints.size()) - 1;
    memory_breakpoints[index_of_breakpoint].start_address = start_address;
    memory_breakpoints[index_of_breakpoint].end_address = end_address;
    memory_breakpoints[index_of_breakpoint].write_breakpoint_count = 1;
  }
  else
  {
    if (end_address > memory_breakpoints[index_of_breakpoint].end_address)
      memory_breakpoints[index_of_breakpoint].end_address = end_address;
    memory_breakpoints[index_of_breakpoint].write_breakpoint_count++;
  }

  TMemCheck check = {};
  PopulateBreakpointFromValueAtIndex(check, index_of_breakpoint);

  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));
}

int MemoryAddressBreakpointsHolder::GetIndexOfStartAddress(unsigned int start_address) const
{
  int list_size = static_cast<int>(memory_breakpoints.size());
  for (int i = 0; i < list_size; ++i)
  {
    if (memory_breakpoints[i].start_address == start_address)
      return i;
  }
  return -1;
}

void MemoryAddressBreakpointsHolder::PopulateBreakpointFromValueAtIndex(TMemCheck& check, int index)
{
  check.start_address = memory_breakpoints[index].start_address;
  check.end_address = memory_breakpoints[index].end_address;

  check.is_enabled = true;
  check.is_ranged = (check.start_address != check.end_address);

  check.is_break_on_read = (memory_breakpoints[index].read_breakpoint_count > 0);
  check.is_break_on_write = (memory_breakpoints[index].write_breakpoint_count > 0);

  check.log_on_hit = false;
  check.break_on_hit = true;

  check.condition = std::nullopt;
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

  if (ContainsReadBreakpoint(start_address) || ContainsWriteBreakpoint(start_address))
  {
    TMemCheck check = {};
    PopulateBreakpointFromValueAtIndex(check, index_of_last_read_breakpoint);
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));
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
  {
    return;
  }

  memory_breakpoints[index_of_last_write_breakpoint].write_breakpoint_count--;

  if (ContainsReadBreakpoint(start_address) || ContainsWriteBreakpoint(start_address))
  {
    TMemCheck check = {};
    PopulateBreakpointFromValueAtIndex(check, index_of_last_write_breakpoint);
    Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));
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
      RemoveReadBreakpoint(memory_breakpoints[0].start_address);
    else
      RemoveWriteBreakpoint(memory_breakpoints[0].start_address);
  }
}
