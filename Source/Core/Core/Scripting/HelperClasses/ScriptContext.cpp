#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting
{

static const char most_recent_script_version[] = "1.0.0";

Common::SPSCQueue<ScriptContext*, false> queue_of_scripts_waiting_to_start =
    Common::SPSCQueue<ScriptContext*, false>();

static ScriptContext* CastToScriptContextPtr(void* input)
{
  return reinterpret_cast<ScriptContext*>(input);
}

void* ScriptContext_Initializer_impl(int unique_identifier, const char* script_file_name,
                                     void (*print_callback_function)(void*, const char*),
                                     void (*script_end_callback)(void*, int),
                                     void* new_dll_api_definitions)
{
  ScriptContext* ret_val = new ScriptContext();
  ret_val->unique_script_identifier = unique_identifier;
  ret_val->script_filename = std::string(script_file_name);
  ret_val->print_callback_function = print_callback_function;
  ret_val->script_end_callback_function = script_end_callback;
  ret_val->current_script_call_location = ScriptCallLocationsEnum::FromScriptStartup;
  ret_val->script_return_code = ScriptReturnCodesEnum::SuccessCode;
  ret_val->last_script_error = std::string("");
  ret_val->is_script_active = 1;
  ret_val->finished_with_global_code = 0;
  ret_val->called_yielding_function_in_last_global_script_resume = 0;
  ret_val->called_yielding_function_in_last_frame_callback_script_resume = 0;
  ret_val->instruction_breakpoints_holder = InstructionBreakpointsHolder();
  ret_val->memory_address_breakpoints_holder = MemoryAddressBreakpointsHolder();
  DLL_Defined_ScriptContext_APIs temp_dll_apis =
      *((DLL_Defined_ScriptContext_APIs*)(new_dll_api_definitions));
  ret_val->dll_specific_api_definitions = temp_dll_apis;
  ret_val->derived_script_context_object_ptr = nullptr;

  return *((void**)&ret_val);
}
void ScriptContext_Destructor_impl(void* script_context)
{
  ScriptContext* casted_script_ptr = CastToScriptContextPtr(script_context);
  casted_script_ptr->is_script_active = 0;
  casted_script_ptr->instruction_breakpoints_holder.RemoveAllBreakpoints();
  casted_script_ptr->memory_address_breakpoints_holder.RemoveAllBreakpoints();
  casted_script_ptr->dll_specific_api_definitions.DLLSpecificDestructor(script_context);
  delete casted_script_ptr;
}

PRINT_CALLBACK_TYPE GetPrintCallback_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)->print_callback_function;
}

SCRIPT_END_CALLBACK_TYPE GetScriptEndCallback_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)->script_end_callback_function;
}

int GetUniqueScriptIdentifier_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)->unique_script_identifier;
}

const char* GetScriptFilename_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)->script_filename.c_str();
}

int GetScriptCallLocation_impl(void* script_context)
{
  return (int)CastToScriptContextPtr(script_context)->current_script_call_location;
}

int GetScriptReturnCode_impl(void* script_context)
{
  return (int)CastToScriptContextPtr(script_context)->script_return_code;
}

void SetScriptReturnCode_impl(void* script_context, int new_script_return_code)
{
  CastToScriptContextPtr(script_context)->script_return_code =
      (ScriptReturnCodesEnum)new_script_return_code;
}

const char* GetErrorMessage_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)->last_script_error.c_str();
}

void SetErrorMessage_impl(void* script_context, const char* new_error_msg)
{
  CastToScriptContextPtr(script_context)->last_script_error = std::string(new_error_msg);
}

int GetIsScriptActive_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)->is_script_active;
}

void SetIsScriptActive_impl(void* script_context, int new_val)
{
  int value_to_set = new_val != 0 ? 1 : 0;
  CastToScriptContextPtr(script_context)->is_script_active = value_to_set;
}

int GetIsFinishedWithGlobalCode_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)->finished_with_global_code;
}

void SetIsFinishedWithGlobalCode_impl(void* script_context, int new_val)
{
  int value_to_set = new_val != 0 ? 1 : 0;
  CastToScriptContextPtr(script_context)->finished_with_global_code = value_to_set;
}

int GetCalledYieldingFunctionInLastGlobalScriptResume_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)
      ->called_yielding_function_in_last_global_script_resume;
}

void SetCalledYieldingFunctionInLastGlobalScriptResume_impl(void* script_context, int new_val)
{
  int value_to_set = new_val != 0 ? 1 : 0;
  CastToScriptContextPtr(script_context)->called_yielding_function_in_last_global_script_resume =
      value_to_set;
}

int GetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)
      ->called_yielding_function_in_last_frame_callback_script_resume;
}

void SetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl(void* script_context,
                                                                   int new_val)
{
  int value_to_set = new_val != 0 ? 1 : 0;
  CastToScriptContextPtr(script_context)
      ->called_yielding_function_in_last_frame_callback_script_resume = value_to_set;
}

void* GetDLLDefinedScriptContextAPIs_impl(void* script_context)
{
  return reinterpret_cast<void*>(
      &(CastToScriptContextPtr(script_context)->dll_specific_api_definitions));
}

void SetDLLDefinedScriptContextAPIs_impl(void* script_context, void* new_dll_definitions)
{
  CastToScriptContextPtr(script_context)->dll_specific_api_definitions =
      *(reinterpret_cast<DLL_Defined_ScriptContext_APIs*>(new_dll_definitions));
}

void* GetDerivedScriptContextPtr_impl(void* script_context)
{
  return CastToScriptContextPtr(script_context)->derived_script_context_object_ptr;
}

void SetDerivedScriptContextPtr_impl(void* base_script_context_ptr,
                                     void* derived_script_context_ptr)
{
  CastToScriptContextPtr(base_script_context_ptr)->derived_script_context_object_ptr =
      derived_script_context_ptr;
}

const char* GetScriptVersion_impl()
{
  return most_recent_script_version;
}

void AddScriptToQueueOfScriptsWaitingToStart_impl(void* script_context)
{
  queue_of_scripts_waiting_to_start.Push(CastToScriptContextPtr(script_context));
}

}  // namespace Scripting
