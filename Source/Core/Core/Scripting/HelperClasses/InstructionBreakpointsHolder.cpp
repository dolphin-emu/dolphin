#include "Core/Scripting/HelperClasses/InstructionBreakpointsHolder.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

InstructionBreakpointsHolder::InstructionBreakpointsHolder() = default;

InstructionBreakpointsHolder::~InstructionBreakpointsHolder()
{
  RemoveAllBreakpoints();
}

void InstructionBreakpointsHolder::AddBreakpoint(unsigned int addr)
{
  if (!ContainsBreakpoint(addr))
  {
    Core::System::GetInstance().GetPowerPC().GetBreakPoints().Add(addr, false, true, false,
                                                                  std::nullopt);
  }

  // add this to the list of breakpoints regardless of whether or not its a duplicate
  breakpoint_addresses.push_back(addr);
}

bool InstructionBreakpointsHolder::ContainsBreakpoint(unsigned int addr) const
{
  return (std::count(breakpoint_addresses.begin(), breakpoint_addresses.end(), addr) > 0);
}

void InstructionBreakpointsHolder::RemoveBreakpoint(unsigned int addr)
{
  auto it = std::find(breakpoint_addresses.begin(), breakpoint_addresses.end(), addr);
  if (it != breakpoint_addresses.end())
    breakpoint_addresses.erase(it);

  if (!ContainsBreakpoint(addr))
    Core::System::GetInstance().GetPowerPC().GetBreakPoints().Remove(addr);
}

void InstructionBreakpointsHolder::RemoveAllBreakpoints()
{
  if (breakpoint_addresses.empty())
    return;

  while (!breakpoint_addresses.empty())
  {
    unsigned int addr = breakpoint_addresses[0];
    breakpoint_addresses.erase(breakpoint_addresses.begin());
    if (!ContainsBreakpoint(addr))
      Core::System::GetInstance().GetPowerPC().GetBreakPoints().Remove(addr);
  }
}
