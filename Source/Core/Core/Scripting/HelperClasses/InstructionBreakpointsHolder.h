#ifndef INSTRUCTION_BREAKPOINTS_HOLDER_IMPL
#define INSTRUCTION_BREAKPOINTS_HOLDER_IMPL

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

class InstructionBreakpointsHolder
{
public:
  InstructionBreakpointsHolder();
  void AddBreakpoint(unsigned int addr);
  unsigned int GetNumCopiesOfBreakpoint(unsigned int addr);
  bool ContainsBreakpoint(unsigned int addr);
  void RemoveBreakpoint(unsigned int addr);
  unsigned int RemoveBreakpoints_OneByOne();

private:
  std::vector<unsigned int> breakpoint_addresses;
};

#ifdef __cplusplus
}
#endif

#endif
