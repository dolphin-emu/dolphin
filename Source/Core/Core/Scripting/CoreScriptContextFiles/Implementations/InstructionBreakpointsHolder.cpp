#include "Core/Scripting/CoreScriptContextFiles/Implementations/InstructionBreakpointsHolder.h"

void InstructionBreakpointsHolder_AddBreakpoint_impl(void* _this, unsigned int addr)
{
  reinterpret_cast<InstructionBreakpointsHolder*>(_this)->breakpoint_addresses.push_back(
      addr);  // add this to the list of breakpoints regardless of whether or not its a duplicate
}

unsigned int InstructionBreakpointsHolder_GetNumCopiesOfBreakpoint_impl(void* _this, unsigned int addr)
{
  InstructionBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<InstructionBreakpointsHolder*>(_this);
  return std::count(casted_struct_ptr->breakpoint_addresses.begin(),
                    casted_struct_ptr->breakpoint_addresses.end(), addr);
}

unsigned int InstructionBreakpointsHolder_ContainsBreakpoint_impl(void* _this, unsigned int addr)
{
  InstructionBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<InstructionBreakpointsHolder*>(_this);
  if (std::count(casted_struct_ptr->breakpoint_addresses.begin(),
                 casted_struct_ptr->breakpoint_addresses.end(), addr) > 0)
    return 1;
  else
    return 0;
}

void InstructionBreakpointsHolder_RemoveBreakpoint_impl(void* _this, unsigned int addr)
{
  InstructionBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<InstructionBreakpointsHolder*>(_this);
  auto it = std::find(casted_struct_ptr->breakpoint_addresses.begin(),
                      casted_struct_ptr->breakpoint_addresses.end(), addr);
  if (it != casted_struct_ptr->breakpoint_addresses.end())
    casted_struct_ptr->breakpoint_addresses.erase(it);
}

unsigned int InstuctionBreakpointsHolder_RemoveBreakpoints_OneByOne_impl(void* _this)
{
  InstructionBreakpointsHolder* casted_struct_ptr =
      reinterpret_cast<InstructionBreakpointsHolder*>(_this);
  if (casted_struct_ptr->breakpoint_addresses.size() == 0)
    return 0;
  else
  {
    unsigned int ret_val = casted_struct_ptr->breakpoint_addresses[0];
    casted_struct_ptr->breakpoint_addresses.erase(casted_struct_ptr->breakpoint_addresses.begin());
    return ret_val;
  }
}
