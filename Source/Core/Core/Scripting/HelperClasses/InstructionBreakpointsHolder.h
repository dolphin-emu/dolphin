#pragma once

#include <vector>

// This is a helper class which stores all instruction breakpoints for a particular ScriptContext*
class InstructionBreakpointsHolder
{
public:
  InstructionBreakpointsHolder();
  ~InstructionBreakpointsHolder();
  void AddBreakpoint(unsigned int addr);
  bool ContainsBreakpoint(unsigned int addr) const;
  void RemoveBreakpoint(unsigned int addr);
  void RemoveAllBreakpoints();

private:
  std::vector<unsigned int> breakpoint_addresses;
};
