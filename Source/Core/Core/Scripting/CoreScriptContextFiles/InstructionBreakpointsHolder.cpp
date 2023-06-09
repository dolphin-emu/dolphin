#ifdef __cplusplus
extern "C" {
#endif

#include "Core/Scripting/CoreScriptContextFiles/InstructionBreakpointsHolder.h"

  void InstructionBreakpointsHolder_AddBreakpoint(
    InstructionBreakpointsHolder* instructionBreakpointsHolder, unsigned int addr)
{
  instructionBreakpointsHolder->breakpoint_addresses.push_back(
      &(instructionBreakpointsHolder->breakpoint_addresses),
      (void*)*((void**)&addr));  // add this to the list of breakpoints regardless of whether or not
                                 // its a duplicate
}

int InstructionBreakpointsHolder_GetNumCopiesOfBreakpoint(
    InstructionBreakpointsHolder* instructionBreakpointsHolder, unsigned int addr)
{
  return instructionBreakpointsHolder->breakpoint_addresses.count(
      &(instructionBreakpointsHolder->breakpoint_addresses), (void*)*((void**)&addr));
}

int InstructionBreakpointsHolder_ContainsBreakpoint(
    InstructionBreakpointsHolder* instructionBreakpointsHolder, unsigned int addr)
{
  return InstructionBreakpointsHolder_GetNumCopiesOfBreakpoint(instructionBreakpointsHolder, addr) >
         0;
}

void InstructionBreakpointsHolder_RemoveBreakpoint(
    InstructionBreakpointsHolder* instructionBreakpointsHolder, unsigned int addr)
{
  if (!InstructionBreakpointsHolder_ContainsBreakpoint(instructionBreakpointsHolder, addr))
    return;
  else
  {
    instructionBreakpointsHolder->breakpoint_addresses.remove_by_value(
        &(instructionBreakpointsHolder->breakpoint_addresses), (void*)*((void**)&addr));
  }
}

void InstuctionBreakpointsHolder_ClearAllBreakpoints(
    InstructionBreakpointsHolder* instructionBreakpointsHolder)
{
  Vector_Destructor(&(instructionBreakpointsHolder->breakpoint_addresses));
}

InstructionBreakpointsHolder InstructionBreakpointsHolder_Initialize()
{
  InstructionBreakpointsHolder instructionBreakpointsHolder;
  instructionBreakpointsHolder.breakpoint_addresses = Vector_Initializer(NULL);
  instructionBreakpointsHolder.AddBreakpoint = InstructionBreakpointsHolder_AddBreakpoint;
  instructionBreakpointsHolder.ContainsBreakpoint = InstructionBreakpointsHolder_ContainsBreakpoint;
  instructionBreakpointsHolder.GetNumCopiesOfBreakpoint =
      InstructionBreakpointsHolder_GetNumCopiesOfBreakpoint;
  instructionBreakpointsHolder.RemoveBreakpoint = InstructionBreakpointsHolder_RemoveBreakpoint;
  return instructionBreakpointsHolder;
}

void InstructionBreakpointsHolder_Destructor(
    InstructionBreakpointsHolder* instructionBreakpointsHolder)
{
  InstuctionBreakpointsHolder_ClearAllBreakpoints(instructionBreakpointsHolder);
  memset(instructionBreakpointsHolder, 0, sizeof(InstructionBreakpointsHolder));
}
#ifdef __cplusplus
  }
#endif
