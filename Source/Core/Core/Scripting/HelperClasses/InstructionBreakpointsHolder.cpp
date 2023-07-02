#include "Core/Scripting/HelperClasses/InstructionBreakpointsHolder.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

InstructionBreakpointsHolder::InstructionBreakpointsHolder()
{
  this->breakpoint_addresses = std::vector<unsigned int>();
}

InstructionBreakpointsHolder::~InstructionBreakpointsHolder()
{
  this->RemoveAllBreakpoints();
}

void InstructionBreakpointsHolder::AddBreakpoint(unsigned int addr)
{
  if (!this->ContainsBreakpoint(addr))
    Core::System::GetInstance().GetPowerPC().GetBreakPoints().Add(addr, false, true, false,
                                                                  std::nullopt);

  breakpoint_addresses.push_back(
      addr);  // add this to the list of breakpoints regardless of whether or not its a duplicate
}

bool InstructionBreakpointsHolder::ContainsBreakpoint(unsigned int addr)
{
  return (std::count(breakpoint_addresses.begin(), breakpoint_addresses.end(), addr) > 0);
}

void InstructionBreakpointsHolder::RemoveBreakpoint(unsigned int addr)
{
  auto it = std::find(breakpoint_addresses.begin(), breakpoint_addresses.end(), addr);
  if (it != breakpoint_addresses.end())
    breakpoint_addresses.erase(it);

  if (!this->ContainsBreakpoint(addr))
    Core::System::GetInstance().GetPowerPC().GetBreakPoints().Remove(addr);
}

void InstructionBreakpointsHolder::RemoveAllBreakpoints()
{
  if (breakpoint_addresses.size() == 0)
    return;

  while (breakpoint_addresses.size() > 0)
  {
    unsigned int addr = breakpoint_addresses[0];
    breakpoint_addresses.erase(breakpoint_addresses.begin());
    if (!this->ContainsBreakpoint(addr))
      Core::System::GetInstance().GetPowerPC().GetBreakPoints().Remove(addr);
  }
}
