#ifdef __cplusplus
extern "C" {
#endif

#ifndef SCRIPT_CONTEXT
#define SCRIPT_CONTEXT


#include "Core/Scripting/CoreScriptContextFiles/InstructionBreakpointsHolder.h"
#include "Core/Scripting/CoreScriptContextFiles/MemoryAddressBreakpointsHolder.h"
#include "Core/Scripting/CoreScriptContextFiles/ScriptCallLocations.h"

extern const char* most_recent_script_version = "1.0.0";


typedef struct ScriptContextBaseFunctionsTable
{

  void (*print_callback)(const char*);
  void (*script_end_callback)(int);

  void (*ImportModule)(const char*, const char*);

  void (*StartScript)();
  void (*RunGlobalScopeCode)();

  void (*RunOnFrameStartCallbacks)();
  void (*RunOnFrameStartCallbacks)();
  void (*RunOnGCControllerPolledCallbacks)();
  void (*RunOnInstructionReachedCallbacks)(u32);
  void (*RunOnMemoryAddressReadFromCallbacks)(u32);
  void (*RunOnMemoryAddressWrittenToCallbacks)(u32);
  void (*RunOnWiiInputPolledCallbacks)();

  void* (*RegisterOnFrameStartCallbacks)(void*);
  void (*RegisterOnFrameStartWithAutoDeregistrationCallbacks)(void*);
  int (*UnregisterOnFrameStartCallbacks)(void*);

  void* (*RegisterOnGCControllerPolledCallbacks)(void*);
  void (*RegisterOnGCControllerPolledWithAutoDeregistrationCallbacks)(void*);
  int (*UnregisterOnGCControllerPolledCallbacks)(void*);

  void* (*RegisterOnInstructionReachedCallbacks)(u32, void*);
  void (*RegisterOnInstructioReachedWithAutoDeregistrationCallbacks)(u32, void*);
  int (*UnregisterOnInstructionReachedCallbacks)(u32, void*);

  void* (*RegisterOnMemoryAddressReadFromCallbacks)(u32, void*);
  void (*RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallbacks)(u32, void*);
  int (*UnregisterOnMemoryAddressReadFromCallbacks)(u32, void*);

  void* (*RegisterOnMemoryAddressWrittenToCallbacks)(u32, void*);
  void (*RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallbacks)(u32, void*);
  int (*UnregisterOnMemoryAddressWrittenToCallbacks)(u32, void*);

  void* (*RegisterOnWiiInputPolledCallbacks)(void*);
  void (*RegisterOnWiiInputPolledWithAutoDeregistrationCallbacks)(void*);
  int (*UnregisterOnWiiInputPolledCallbacks)(void*);

  void (*RegisterButtonCallback)(long long, void*);
  int (*IsButtonRegistered)(long long);
  void (*GetButtonCallbacksAndAddToQueue)(long long);
  void (*RunButtonCallbacksInQueue)();

} ScriptContextBaseFunctionsTable;


typedef struct ScriptContext
{
  int unique_script_identifier;
  const char* script_filename;
  ScriptCallLocations current_script_call_location;
  int is_script_active;
  int finished_with_global_code;
  int called_yielding_function_in_last_global_script_resume;
  int called_yielding_function_in_last_frame_callback_script_resume;
  void* script_specific_lock;

  InstructionBreakpointsHolder instructionBreakpointsHolder;
  MemoryAddressBreakpointsHolder memoryAddressBreakpointsHolder;
  ScriptContextBaseFunctionsTable scriptContextBaseFunctionsTable;
  ScriptContext* (*Initialize_ScriptContext)(int, const char*, void (*)(const char*), void (*)(int));

} ScriptContext;

ScriptContext* CreateScript(int unique_identifier, const char* script_file_name,
                            void (*print_callback_function)(const char*),
                            void(*script))
  void ShutdownScript(ScriptContext* script_context)
  {
  script_context->is_script_active = 0;
  script_context->scriptContextBaseFunctionsTable.script_end_callback(script_context->unique_script_identifier);
  }

#endif

#ifdef __cplusplus
}
#endif
