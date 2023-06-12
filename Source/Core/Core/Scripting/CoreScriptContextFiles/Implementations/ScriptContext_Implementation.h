#ifdef __cplusplus
extern "C" {
#endif

#include <memory>
#include <mutex>
#include "Core/Scripting/CoreScriptContextFiles/Implementations/InstructionBreakpointsHolder_Implementation.h"
#include "Core/Scripting/CoreScriptContextFiles/Implementations/MemoryAddressBreakpointsHolder_Implementation.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ScriptContext_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ScriptCallLocations.h"

typedef struct ScriptContext
{
  void (*print_callback_function)(void*, const char*);
  void (*script_end_callback_function)(void*, int);

  int unique_script_identifier;
  const char* script_filename;
  ScriptCallLocations current_script_call_location;
  int is_script_active;
  int finished_with_global_code;
  int called_yielding_function_in_last_global_script_resume;
  int called_yielding_function_in_last_frame_callback_script_resume;
  std::recursive_mutex script_specific_lock;

  InstructionBreakpointsHolder instructionBreakpointsHolder;
  MemoryAddressBreakpointsHolder memoryAddressBreakpointsHolder;

  DLL_Defined_ScriptContext_APIs dll_specific_api_definitions;

} ScriptContext;

const char* most_recent_script_version = "1.0.0";

void* CreateScript(int unique_identifier, const char* script_file_name,
                   void (*print_callback_function)(void*, const char*),
                   void (*script_end_callback)(void*, int), void* new_dll_api_definitions)
{
  ScriptContext* ret_val = new ScriptContext();
  ret_val->unique_script_identifier = unique_identifier;
  ret_val->script_filename = script_file_name;
  ret_val->print_callback_function = print_callback_function;
  ret_val->script_end_callback_function = script_end_callback;
  ret_val->current_script_call_location = ScriptCallLocations::FromScriptStartup;
  ret_val->is_script_active = 1;
  ret_val->finished_with_global_code = 0;
  ret_val->called_yielding_function_in_last_global_script_resume = 0;
  ret_val->called_yielding_function_in_last_frame_callback_script_resume = 0;
  ret_val->instructionBreakpointsHolder.breakpoint_addresses = std::vector<unsigned int>();
  ret_val->memoryAddressBreakpointsHolder.read_breakpoint_addresses = std::vector<unsigned int>();
  ret_val->memoryAddressBreakpointsHolder.write_breakpoint_addresses = std::vector<unsigned int>();
  ret_val->dll_specific_api_definitions = new_dll_api_definitions;

  return *((void**)&ret_val);
}

void ShutdownScript(void* script_context)
{
  ScriptContext* casted_struct_ptr = reinterpret_cast<ScriptContext*>(script_context);
  casted_struct_ptr->is_script_active = 0;
  casted_struct_ptr->script_end_callback_function(script_context, casted_struct_ptr->unique_script_identifier);
}

#ifdef __cplusplus
}
#endif
