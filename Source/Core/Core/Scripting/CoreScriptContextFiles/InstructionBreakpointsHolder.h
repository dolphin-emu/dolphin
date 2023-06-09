#ifdef __cplusplus
extern "C" {
#endif

#ifndef INSTRUCTION_BREAKPOINTS_HOLDER
#define INSTRUCTION_BREAKPOINTS_HOLDER

#include "Core/Scripting/CoreScriptContextFiles/Vector.h"


typedef struct InstructionBreakpointsHolder
{
  Vector breakpoint_addresses;
  void (*AddBreakpoint)(struct InstructionBreakpointsHolder*, unsigned int);
  void (*RemoveBreakpoint)(struct InstructionBreakpointsHolder*, unsigned int);
  int (*ContainsBreakpoint)(struct InstructionBreakpointsHolder*, unsigned int);
  int (*GetNumCopiesOfBreakpoint)(struct InstructionBreakpointsHolder*, unsigned int);
} InstructionBreakpointsHolder;

void InstructionBreakpointsHolder_AddBreakpoint(InstructionBreakpointsHolder* instructionBreakpointsHolder, unsigned int addr);
int InstructionBreakpointsHolder_GetNumCopiesOfBreakpoint(InstructionBreakpointsHolder* instructionBreakpointsHolder, unsigned int addr);
int InstructionBreakpointsHolder_ContainsBreakpoint(InstructionBreakpointsHolder* instructionBreakpointsHolder, unsigned int addr);
void InstructionBreakpointsHolder_RemoveBreakpoint(InstructionBreakpointsHolder* instructionBreakpointsHolder, unsigned int addr);
void InstuctionBreakpointsHolder_ClearAllBreakpoints(InstructionBreakpointsHolder* instructionBreakpointsHolder);
InstructionBreakpointsHolder InstructionBreakpointsHolder_Initialize();
void InstructionBreakpointsHolder_Destructor(InstructionBreakpointsHolder* instructionBreakpointsHolder);

#endif

#ifdef __cplusplus
  }
#endif

