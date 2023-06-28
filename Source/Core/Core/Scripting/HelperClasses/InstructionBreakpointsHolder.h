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
  ~InstructionBreakpointsHolder();
  void AddBreakpoint(unsigned int addr);
  bool ContainsBreakpoint(unsigned int addr);
  void RemoveBreakpoint(unsigned int addr);
  void RemoveAllBreakpoints();

private:
  std::vector<unsigned int> breakpoint_addresses;
};

#ifdef __cplusplus
}
#endif

#endif
