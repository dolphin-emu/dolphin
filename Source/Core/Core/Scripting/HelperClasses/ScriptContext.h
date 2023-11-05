#pragma once

#include <memory>
#include <mutex>
#include "Common/SPSCQueue.h"
#include "Core/Scripting/CoreScriptInterface/Enums/ScriptCallLocationsEnum.h"
#include "Core/Scripting/CoreScriptInterface/Enums/ScriptReturnCodesEnum.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/ScriptContext_APIs.h"
#include "Core/Scripting/HelperClasses/InstructionBreakpointsHolder.h"
#include "Core/Scripting/HelperClasses/MemoryAddressBreakpointsHolder.h"

// This file contains the definition for the ScriptContext struct, and the implementations
// for each of the APIs for Dolphin_Defined_ScriptContext_APIs
namespace Scripting
{

struct ScriptContext
{
  void (*print_callback_function)(void*, const char*);
  void (*script_end_callback_function)(void*, int);

  int unique_script_identifier;
  std::string script_filename;
  ScriptCallLocationsEnum current_script_call_location;
  ScriptReturnCodesEnum script_return_code;
  std::string last_script_error;
  int is_script_active;
  int finished_with_global_code;
  int called_yielding_function_in_last_global_script_resume;
  int called_yielding_function_in_last_frame_callback_script_resume;
  std::recursive_mutex script_specific_lock;

  InstructionBreakpointsHolder instruction_breakpoints_holder;
  MemoryAddressBreakpointsHolder memory_address_breakpoints_holder;

  DLL_Defined_ScriptContext_APIs dll_specific_api_definitions;

  void* derived_script_context_object_ptr;
};

extern Common::SPSCQueue<ScriptContext*, false> queue_of_scripts_waiting_to_start;

void* ScriptContext_Initializer_impl(int unique_identifier, const char* script_file_name,
                                     void (*print_callback_function)(void*, const char*),
                                     void (*script_end_callback)(void*, int),
                                     void* new_dll_api_definitions);

void ScriptContext_Destructor_impl(void* script_context);

using PRINT_CALLBACK_TYPE = void (*)(void*, const char*);
using SCRIPT_END_CALLBACK_TYPE = void (*)(void*, int);

PRINT_CALLBACK_TYPE GetPrintCallback_impl(void*);
SCRIPT_END_CALLBACK_TYPE GetScriptEndCallback_impl(void*);

int GetUniqueScriptIdentifier_impl(void*);
const char* GetScriptFilename_impl(void*);

int GetScriptCallLocation_impl(void*);

int GetScriptReturnCode_impl(void*);
void SetScriptReturnCode_impl(void*, int);

const char* GetErrorMessage_impl(void*);
void SetErrorMessage_impl(void*, const char*);

int GetIsScriptActive_impl(void*);
void SetIsScriptActive_impl(void*, int);

int GetIsFinishedWithGlobalCode_impl(void*);
void SetIsFinishedWithGlobalCode_impl(void*, int);

int GetCalledYieldingFunctionInLastGlobalScriptResume_impl(void*);
void SetCalledYieldingFunctionInLastGlobalScriptResume_impl(void*, int);

int GetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl(void*);
void SetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl(void*, int);

void* GetDLLDefinedScriptContextAPIs_impl(void*);
void SetDLLDefinedScriptContextAPIs_impl(void*, void*);

void* GetDerivedScriptContextPtr_impl(void*);
void SetDerivedScriptContextPtr_impl(void*, void*);

const char* GetScriptVersion_impl();

void AddScriptToQueueOfScriptsWaitingToStart_impl(void*);

}  // namespace Scripting
