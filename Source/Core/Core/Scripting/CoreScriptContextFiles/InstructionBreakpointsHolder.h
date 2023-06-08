#ifdef __cplusplus
extern "C" {
#endif

#ifndef INSTRUCTION_BREAKPOINTS_HOLDER
#define INSTRUCTION_BREAKPOINTS_HOLDER

#include "Core/Scripting/CoreScriptContextFiles/Vector.h"


typedef struct InstructionBreakpointsHolder
{
  Vector breakpoint_addresses;
  void (*AddBreakpoint)(struct InstructionBreakpointsHolder*, u32);
  void (*RemoveBreakpoint)(struct InstructionBreakpointsHolder*, u32);
  int (*ContainsBreakpoint)(struct InstructionBreakpointsHolder*, u32);
  int (*GetNumCopiesOfBreakpoint)(struct InstructionBreakpointsHolder*, u32);
} InstructionBreakpointsHolder;


void InstructionBreakpointsHolder_AddBreakpoint(InstructionBreakpointsHolder* instructionBreakpointsHolder, u32 addr)
{
  instructionBreakpointsHolder->breakpoint_addresses.push_back(
      &(instructionBreakpointsHolder->breakpoint_addresses),
      (void*)*((void**)&addr));  // add this to the list of breakpoints regardless of whether or not
                                 // its a duplicate
}

int InstructionBreakpointsHolder_GetNumCopiesOfBreakpoint(
    InstructionBreakpointsHolder* instructionBreakpointsHolder, u32 addr)
{
  return instructionBreakpointsHolder->breakpoint_addresses.count(
      &(instructionBreakpointsHolder->breakpoint_addresses), (void*)*((void**)&addr));
}

int InstructionBreakpointsHolder_ContainsBreakpoint(InstructionBreakpointsHolder* instructionBreakpointsHolder, u32 addr)
{
  return InstructionBreakpointsHolder_GetNumCopiesOfBreakpoint(instructionBreakpointsHolder, addr) > 0;
}

  void InstructionBreakpointsHolder_RemoveBreakpoint(InstructionBreakpointsHolder* instructionBreakpointsHolder, u32 addr)
  {
  if (!InstructionBreakpointsHolder_ContainsBreakpoint(instructionBreakpointsHolder, addr))
    return;
  else
  {
    instructionBreakpointsHolder->breakpoint_addresses.remove_by_value(
        &(instructionBreakpointsHolder->breakpoint_addresses), (void*)*((void**)&addr));
  }
  }

  void InstuctionBreakpointsHolder_ClearAllBreakpoints(InstructionBreakpointsHolder* instructionBreakpointsHolder)
  {
    Vector_Destructor(&(instructionBreakpointsHolder->breakpoint_addresses));
  }

  InstructionBreakpointsHolder InstructionBreakpointsHolder_Initialize()
  {
    InstructionBreakpointsHolder instructionBreakpointsHolder;
    instructionBreakpointsHolder.breakpoint_addresses = Vector_Initializer(NULL);
    instructionBreakpointsHolder.AddBreakpoint = InstructionBreakpointsHolder_AddBreakpoint;
    instructionBreakpointsHolder.ContainsBreakpoint = InstructionBreakpointsHolder_ContainsBreakpoint;
    instructionBreakpointsHolder.GetNumCopiesOfBreakpoint = InstructionBreakpointsHolder_GetNumCopiesOfBreakpoint;
    instructionBreakpointsHolder.RemoveBreakpoint = InstructionBreakpointsHolder_RemoveBreakpoint;
    return instructionBreakpointsHolder;
  }

  void InstructionBreakpointsHolder_Destructor(InstructionBreakpointsHolder* instructionBreakpointsHolder)
  {
    InstuctionBreakpointsHolder_ClearAllBreakpoints(instructionBreakpointsHolder);
    memset(instructionBreakpointsHolder, 0, sizeof(InstructionBreakpointsHolder));
  }

#endif

#ifdef __cplusplus
  }
#endif

