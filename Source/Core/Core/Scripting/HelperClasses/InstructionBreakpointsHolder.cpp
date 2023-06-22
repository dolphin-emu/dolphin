#include "Core/Scripting/HelperClasses/InstructionBreakpointsHolder.h"

InstructionBreakpointsHolder::InstructionBreakpointsHolder()
{
  this->breakpoint_addresses = std::vector<unsigned int>();
}

void InstructionBreakpointsHolder::AddBreakpoint(unsigned int addr)
{
  this->breakpoint_addresses.push_back(
      addr);  // add this to the list of breakpoints regardless of whether or not its a duplicate
}

unsigned int InstructionBreakpointsHolder::GetNumCopiesOfBreakpoint(unsigned int addr)
{
  return std::count(this->breakpoint_addresses.begin(), this->breakpoint_addresses.end(), addr);
}

bool InstructionBreakpointsHolder::ContainsBreakpoint(unsigned int addr)
{
  return (std::count(this->breakpoint_addresses.begin(), this->breakpoint_addresses.end(), addr) >
          0);
}

void InstructionBreakpointsHolder::RemoveBreakpoint(unsigned int addr)
{
  auto it = std::find(this->breakpoint_addresses.begin(), this->breakpoint_addresses.end(), addr);
  if (it != this->breakpoint_addresses.end())
    this->breakpoint_addresses.erase(it);
}

unsigned int InstructionBreakpointsHolder::RemoveBreakpoints_OneByOne()
{
  if (this->breakpoint_addresses.size() == 0)
    return 0;
  else
  {
    unsigned int ret_val = this->breakpoint_addresses[0];
    this->breakpoint_addresses.erase(this->breakpoint_addresses.begin());
    return ret_val;
  }
}
