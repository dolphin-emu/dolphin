#ifndef SCRIPT_CONTEXT_IMPL
#define SCRIPT_CONTEXT_IMPL

#include <memory>
#include <mutex>
#include "Common/ThreadSafeQueue.h"
#include "Core/Scripting/CoreScriptContextFiles/Enums/ScriptCallLocations.h"
#include "Core/Scripting/CoreScriptContextFiles/Enums/ScriptReturnCodes.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ScriptContext_APIs.h"
#include "Core/Scripting/HelperClasses/InstructionBreakpointsHolder.h"
#include "Core/Scripting/HelperClasses/MemoryAddressBreakpointsHolder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScriptContext
{
  void (*print_callback_function)(void*, const char*);
  void (*script_end_callback_function)(void*, int);

  int unique_script_identifier;
  std::string script_filename;
  ScriptingEnums::ScriptCallLocations current_script_call_location;
  ScriptingEnums::ScriptReturnCodes script_return_code;
  std::string last_script_error;
  int is_script_active;
  int finished_with_global_code;
  int called_yielding_function_in_last_global_script_resume;
  int called_yielding_function_in_last_frame_callback_script_resume;
  std::recursive_mutex script_specific_lock;

  InstructionBreakpointsHolder instructionBreakpointsHolder;
  MemoryAddressBreakpointsHolder memoryAddressBreakpointsHolder;

  DLL_Defined_ScriptContext_APIs dll_specific_api_definitions;

  void* derived_script_context_class_ptr;

} ScriptContext;

extern ThreadSafeQueue<ScriptContext*> queue_of_scripts_waiting_to_start;

extern const char* most_recent_script_version;

void* ScriptContext_Initializer_impl(int unique_identifier, const char* script_file_name,
                                     void (*print_callback_function)(void*, const char*),
                                     void (*script_end_callback)(void*, int),
                                     void* new_dll_api_definitions);

void ScriptContext_Destructor_impl(void* script_context);

typedef void (*PRINT_CALLBACK_TYPE)(void*, const char*);
typedef void (*SCRIPT_END_CALLBACK_TYPE)(void*, int);

PRINT_CALLBACK_TYPE ScriptContext_GetPrintCallback_impl(void*);
SCRIPT_END_CALLBACK_TYPE ScriptContext_GetScriptEndCallback_impl(void*);

int ScriptContext_GetUniqueScriptIdentifier_impl(void*);
const char* ScriptContext_GetScriptFilename_impl(void*);

int ScriptContext_GetScriptCallLocation_impl(void*);

int ScriptContext_GetScriptReturnCode_impl(void*);
void ScriptContext_SetScriptReturnCode_impl(void*, int);

const char* ScriptContext_GetErrorMessage_impl(void*);
void ScriptContext_SetErrorMessage_impl(void*, const char*);

int ScriptContext_GetIsScriptActive_impl(void*);
void ScriptContext_SetIsScriptActive_impl(void*, int);

int ScriptContext_GetIsFinishedWithGlobalCode_impl(void*);

void ScriptContext_SetIsFinishedWithGlobalCode_impl(void*, int);

int ScriptContext_GetCalledYieldingFunctionInLastGlobalScriptResume_impl(void*);

void ScriptContext_SetCalledYieldingFunctionInLastGlobalScriptResume_impl(void*, int);

int ScriptContext_GetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl(void*);
void ScriptContext_SetCalledYieldingFunctionInLastFrameCallbackScriptResume_impl(void*, int);

void* ScriptContext_GetDllDefinedScriptContextAPIs_impl(void*);
void ScriptContext_SetDLLDefinedScriptContextAPIs_impl(void*, void*);

void* ScriptContext_GetDerivedScriptContextPtr_impl(void*);
void ScriptContext_SetDerivedScriptContextPtr_impl(void*, void*);

const char* ScriptContext_GetScriptVersion_impl();

void ScriptContext_AddScriptToQueueOfScriptsWaitingToStart_impl(void*);

#ifdef __cplusplus
}
#endif

#endif
