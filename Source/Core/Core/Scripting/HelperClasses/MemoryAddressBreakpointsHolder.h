#include <vector>

#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

class MemoryAddressBreakpointsHolder
{
public:
  MemoryAddressBreakpointsHolder() {}
  inline ~MemoryAddressBreakpointsHolder() { ClearAllBreakpoints(); }

  void AddReadBreakpoint(u32 addr)
  {
    this->read_breakpoint_addresses.push_back(addr);  // add this to the list of breakpoints regardless of whether or not it's a duplicate
    bool write_breakpoint_exists = this->ContainsWriteBreakpoint(addr);

            TMemCheck check;

            check.start_address = addr;
            check.end_address = addr;
            check.is_break_on_read = true;
            check.is_break_on_write = write_breakpoint_exists;
            check.condition = std::nullopt;
            check.break_on_hit = true;

            Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));

  }

  void AddWriteBreakpoint(u32 addr)
  {
    this->write_breakpoint_addresses.push_back(addr);  // add this to the list of breakpoints regardless of whether or not its a duplicate

    bool read_breakpoint_exists = this->ContainsReadBreakpoint(addr);


            TMemCheck check;

            check.start_address = addr;
            check.end_address = addr;
            check.is_break_on_read = read_breakpoint_exists;
            check.is_break_on_write = true;
            check.condition = {};
            check.break_on_hit = true;

            Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(check));

  }

  void RemoveReadBreakpoint(u32 addr)
  {
   
    if (!this->ContainsReadBreakpoint(addr))
      return;
    else
    {
      read_breakpoint_addresses.erase(
          std::find(read_breakpoint_addresses.begin(), read_breakpoint_addresses.end(), addr));

      if (!this->ContainsReadBreakpoint(addr) && !this->ContainsWriteBreakpoint(addr))
      {
        Core::System::GetInstance().GetPowerPC().GetMemChecks().Remove(addr);
      }
    }
  }

  void RemoveWriteBreakpoint(u32 addr)
  {
    if (!this->ContainsWriteBreakpoint(addr))
      return;
    else
    {
      write_breakpoint_addresses.erase(
          std::find(write_breakpoint_addresses.begin(), write_breakpoint_addresses.end(), addr));

      if (!this->ContainsReadBreakpoint(addr) && !this->ContainsWriteBreakpoint(addr))
      {
        Core::System::GetInstance().GetPowerPC().GetMemChecks().Remove(addr);
      }
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
