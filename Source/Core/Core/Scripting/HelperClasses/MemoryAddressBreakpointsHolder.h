#include <vector>
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"

class MemoryAddressBreakpointsHolder
{
public:
  MemoryAddressBreakpointsHolder() {}
  inline ~MemoryAddressBreakpointsHolder() { ClearAllBreakpoints(); }

  void AddReadBreakpoint(u32 addr)
  {
    if (!this->ContainsReadBreakpoint(addr))
    {
      TMemCheck check;

      check.start_address = addr;
      check.end_address = addr;
      check.is_break_on_read = true;
      check.is_break_on_write = this->ContainsWriteBreakpoint(addr);
      check.condition = {};
      PowerPC::memchecks.Add(std::move(check));
    }

    this->read_breakpoint_addresses.push_back(addr);  // add this to the list of breakpoints regardless of whether or not it's a duplicate
  }

  void AddWriteBreakpoint(u32 addr)
  {
    if (!this->ContainsWriteBreakpoint(addr))
    {
      TMemCheck check;

      check.start_address = addr;
      check.end_address = addr;
      check.is_break_on_read = this->ContainsReadBreakpoint(addr);
      check.is_break_on_write = true;
      check.condition = {};
      PowerPC::memchecks.Add(std::move(check));
    }

    this->write_breakpoint_addresses.push_back(addr);  // add this to the list of breakpoints regardless of whether or not its a duplicate
  }

  void RemoveReadBreakpoint(u32 addr)
  {
    size_t num_copies = this->GetNumReadCopiesOfBreakpoint(addr);
    if (num_copies == 0)
      return;
    else
    {
      read_breakpoint_addresses.erase(
          std::find(read_breakpoint_addresses.begin(), read_breakpoint_addresses.end(), addr));
      if (num_copies == 1 && !this->ContainsWriteBreakpoint(addr))
        Core::QueueHostJob(
            [=]() { Core::RunOnCPUThread([=]() { PowerPC::memchecks.Remove(addr); }, true); },
            true);
    }
  }

  void RemoveWriteBreakpoint(u32 addr)
  {
    size_t num_copies = this->GetNumWriteCopiesOfBreakpoint(addr);
    if (num_copies == 0)
      return;
    else
    {
      write_breakpoint_addresses.erase(
          std::find(write_breakpoint_addresses.begin(), write_breakpoint_addresses.end(), addr));
      if (num_copies == 1 && !this->ContainsReadBreakpoint(addr))
        Core::QueueHostJob(
            [=]() { Core::RunOnCPUThread([=]() { PowerPC::memchecks.Remove(addr); }, true); },
            true);
    }
  }

  void ClearAllBreakpoints()
  {
    while (read_breakpoint_addresses.size() > 0)
      this->RemoveReadBreakpoint(read_breakpoint_addresses[0]);

    while (write_breakpoint_addresses.size() > 0)
      this->RemoveWriteBreakpoint(write_breakpoint_addresses[0]);
  }

  bool ContainsReadBreakpoint(u32 addr)
  {
    return this->GetNumReadCopiesOfBreakpoint(addr) > 0;
  }

  bool ContainsWriteBreakpoint(u32 addr)
  {
    return this->GetNumWriteCopiesOfBreakpoint(addr) > 0;
  }

  private:

  size_t GetNumReadCopiesOfBreakpoint(u32 addr)
  {
    return std::count(read_breakpoint_addresses.begin(), read_breakpoint_addresses.end(), addr);
  }

  size_t GetNumWriteCopiesOfBreakpoint(u32 addr)
  {
    return std::count(write_breakpoint_addresses.begin(), write_breakpoint_addresses.end(), addr);
  }

  std::vector<u32> read_breakpoint_addresses;
  std::vector<u32> write_breakpoint_addresses;
};
