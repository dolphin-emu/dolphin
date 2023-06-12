#ifndef INSTRUCTION_BREAKPOINTS_HOLDER_IMPL
#define INSTRUCTION_BREAKPOINTS_HOLDER_IMPL

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct InstructionBreakpointsHolder
{
  std::vector<unsigned int> breakpoint_addresses;
} InstructionBreakpointsHolder;

void InstructionBreakpointsHolder_AddBreakpoint_impl(void* _this, unsigned int addr);
unsigned int InstructionBreakpointsHolder_GetNumCopiesOfBreakpoint_impl(void* _this, unsigned int addr);
unsigned int InstructionBreakpointsHolder_ContainsBreakpoint_impl(void* _this, unsigned int addr);
void InstructionBreakpointsHolder_RemoveBreakpoint_impl(void* _this, unsigned int addr);
unsigned int InstuctionBreakpointsHolder_RemoveBreakpoints_OneByOne_impl(void* _this);


#ifdef __cplusplus
  }
#endif

#endif
