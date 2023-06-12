#ifndef INSTRUCTION_BREAKPOINTS_HOLDER
#define INSTRUCTION_BREAKPOINTS_HOLDER

#ifdef __cplusplus
extern "C" {
#endif

 // The 1st void* parameter for each function represents an InstructionBreakpointsHolder*
 // The InstructionBreakpointsHolder struct is essentially just a list of unsigned ints representing instruction-hit breakpoints that have been set.
typedef struct InstructionBreakpointsHolder_APIs
{
  void (*AddBreakpoint)(void*, unsigned int); // Adds the specified breakpoint to the InstructionBreakpointsHolder*
  void (*RemoveBreakpoint)(void*, unsigned int); // Removes one instance of the specified breakpoint (the unsigned int) from the InstructionBreakpointsHolder*
  int (*ContainsBreakpoint)(void*, unsigned int); // Returns 1 if the InstructionBreakpointsHolder* contains the specified breakpoint, and 0 otherwise.
  int (*GetNumCopiesOfBreakpoint)(void*, unsigned int); // Returns the number of times that the specified breakpoint appears in the InstructionBreakpointsHolder*
} InstructionBreakpointsHolder_APIs;


#ifdef __cplusplus
  }
#endif

#endif

