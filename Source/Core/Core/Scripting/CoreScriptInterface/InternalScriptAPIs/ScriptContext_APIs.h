#pragma once

namespace Scripting
{

#ifdef __cplusplus
extern "C" {
#endif

// The ScriptContext_APIs file defines 2 API Types:
//  1. Dolphin_Defined_ScriptContext_APIs
//  2. DLL_Defined_ScriptContext_APIs
//
//  Both of these 2 APIs take an opaque handle to a ScriptContext* (an object which is only defined
//  in the Dolphin side) as their 1st parameter unless specified otherwise in the documentation.
//  However, Dolphin_Defined_ScriptContext_APIs have their function definitions implemented in the
//  Dolphin code, and are the same across all DLLs, while the DLL_Defined_ScriptContext_APIs have
//  their implementations defined in each specific DLL - which means they're different for each DLL.
typedef struct Dolphin_Defined_ScriptContext_APIs
{
  typedef void (*PRINT_CALLBACK_FUNCTION_TYPE)(void*, const char*);
  typedef void (*SCRIPT_END_CALLBACK_FUNCTION_TYPE)(void*, int);

  // Creates a new ScriptContext*, and returns a pointer to it. The first parameter is the
  // unique ID of the script, the 2nd parameter is the script filename, the 3rd parameter is the
  // print_callback function (invoked when a script calls its print function), the 4th parameter is
  // the script_end_callback function (invoked when a script terminates), and the 5th parameter is a
  // DLL_Defined_ScriptContext_APIs* (this is how scripts from different DLLs/languages can have
  // different DLL_Defined_ScriptContext_API function definitions)
  void* (*ScriptContext_Initializer)(int, const char*, PRINT_CALLBACK_FUNCTION_TYPE,
                                     SCRIPT_END_CALLBACK_FUNCTION_TYPE, void*);

  // Calls the DLL-Specific destructor, and then frees the memory allocated by the ScriptContext*
  // passed into the function.
  void (*ScriptContext_Destructor)(void*);

  // Returns the function pointer to the print_callback function for the ScriptContext* which is
  // passed into the function.
  PRINT_CALLBACK_FUNCTION_TYPE (*GetPrintCallbackFunction)(void*);

  // Returns the function pointer to the script_end_callback function for the ScriptContext* which
  // is passed into the function.
  SCRIPT_END_CALLBACK_FUNCTION_TYPE (*GetScriptEndCallbackFunction)(void*);

  // Returns the integer that represents the unique ID for the ScriptContext*
  int (*GetUniqueScriptIdentifier)(void*);

  // Retuns the filename for the script.
  const char* (*GetScriptFilename)(void*);

  // Returns the location that the ScriptContext* is executing from.
  int (*GetScriptCallLocation)(void*);

  // We intentionally don't include a set_script_call_location API, since this should only ever be
  // set by Dolphin in the ScriptUtilities.cpp file - and it therefore doesn't require an API.

  // Gets the return code of the last script execution (which is really of type
  // ScriptReturnCodesEnum).
  int (*GetScriptReturnCode)(void*);

  // sets the return code for the last script execution (should only be called by the DLL).
  // 2nd parameter represents a ScriptReturnCodesEnum.
  void (*SetScriptReturnCode)(void*, int);

  // Returns the error message from the last script execution (only valid when script_return_code is
  // not Success)
  const char* (*GetErrorMessage)(void*);

  // Sets the error message from the last script execution to the specified value (should only be
  // called by the DLL).
  void (*SetErrorMessage)(void*, const char*);

  // Returns 1 if the ScriptContext* is currently active, and 0 otherwise.
  int (*GetIsScriptActive)(void*);

  // Sets the ScriptContext*'s is_script_active variable to the specified value.
  void (*SetIsScriptActive)(void*, int);

  // Returns 1 if the ScriptContext* has finished running all global code, and 0 otherwise.
  int (*GetIsFinishedWithGlobalCode)(void*);

  // Sets the ScriptContext*'s is_finished_with_global_code variable to the specified value.
  void (*SetIsFinishedWithGlobalCode)(void*, int);

  // Returns 1 if the ScriptContext* yielded in its last global scope code, and 0 otherwise.
  int (*GetCalledYieldingFunctionInLastGlobalScriptResume)(void*);

  //  Sets the ScriptContext*'s called_yielding_function_in_last_global_script_resume variable to
  //  the specified value
  void (*SetCalledYieldingFunctionInLastGlobalScriptResume)(void*, int);

  // Returns 1 if the ScriptContext* yielded in its last frame callback code, and 0 otherwise.
  int (*GetCalledYieldingFunctionInLastFrameCallbackScriptResume)(void*);

  // Sets the ScriptContext*'s called_yielding_function_in_last_frame_callback_script_resume
  // variable to the specified value.
  void (*SetCalledYieldingFunctionInLastFrameCallbackScriptResume)(void*, int);

  // Returns the DLL_Defined_ScriptContext_APIs* for the ScriptContext* (see below for definition)
  void* (*GetDLLDefinedScriptContextAPIs)(void*);

  // Sets the DLL_Defined_ScriptContext_APIs* for the ScriptContext* to the value specified by the
  // 2nd parameter
  void (*SetDLLDefinedScriptContextAPIs)(void*, void*);

  // Returns a pointer to the associated derived ScriptContext* object (ex. a LuaScriptContext*
  // defined in the DLL)
  void* (*GetDerivedScriptContextPtr)(void*);

  // Sets the associated derived ScriptContext* for the ScriptContext* to the value specified by the
  // 2nd parameter.
  void (*SetDerivedScriptContextPtr)(void*, void*);

  // This is the same for all scripts, so no void* is passed into it.
  const char* (*GetScriptVersion)();

  // Adds the ScriptContext* passed in as an argument to the queue of scripts waiting to be started.
  void (*AddScriptToQueueOfScriptsWaitingToStart)(void*);

} Dolphin_Defined_ScriptContext_APIs;

// These are functions whose definitions are provided by the implementing DLL (each DLL has their
// own definitions for these functions) The first parameter for each function is a void* which
// represents a ScriptContext* (NOT a derived class pointer). A pointer to the struct containing
// these APIs is included in ScriptContext.dll_specific_api_definitions.
typedef struct DLL_Defined_ScriptContext_APIs
{
  // 1st parameter is ScriptContext*, 2nd parameter is API name, and 3rd parameter is API version.
  // Imports the specified module into the ScriptContext* (scripts will be able to call the imported
  // functions after this function terminates).
  void (*ImportModule)(void*, const char*, const char*);

  // Makes the Script start running for the first time.
  void (*StartScript)(void*);

  // Continues running the ScriptContext* from the global level (used to resume a script which has
  // already started, which isn't supported by all languages. Languages that don't support this
  // should just return right away in their implementation of this function)
  void (*RunGlobalScopeCode)(void*);

  // Runs all the OnFrameStart callbacks currently registered for the ScriptContext*
  void (*RunOnFrameStartCallbacks)(void*);

  // Runs all the OnGCControllerPolled callbacks currently registered for the ScriptContext*
  void (*RunOnGCControllerPolledCallbacks)(void*);

  // Runs all OnWiiInputPolled callbacks currently registered for the ScriptContext*
  void (*RunOnWiiInputPolledCallbacks)(void*);

  // Runs all OnInstructionHit callbacks currently registered for the specified instruction
  // address for the ScriptContext*
  void (*RunOnInstructionHitCallbacks)(void*, unsigned int);

  // Runs all OnMemoryAddressReadFrom callbacks currently registered for the specified memory
  // address for the ScriptContext*
  void (*RunOnMemoryAddressReadFromCallbacks)(void*, unsigned int);

  // Runs all OnMemoryAddressWrittenTo callbacks currently registered for the specified memory
  // address for the ScriptContext*
  void (*RunOnMemoryAddressWrittenToCallbacks)(void*, unsigned int);

  // Registers a new OnFrameStart callback function (the 2nd parameter passed into the function),
  // and returns a handle that can be used to unregister the callback later on.
  void* (*RegisterOnFrameStartCallback)(void*, void*);

  // Registers a new OnFrameStart callback function (the 2nd parameter passed into the function).
  // This callback should auto-deregister when only auto-deregistered callbacks are still running.
  void (*RegisterOnFrameStartWithAutoDeregistrationCallback)(void*, void*);

  // Unregisters/removes an OnFrameStart callback (the 2nd parameter passed into the function should
  // be the return result of RegisterOnFrameStartCallback()). Should return 1 if attempt to
  // unregister succeded, and should return 0 otherwise.
  int (*UnregisterOnFrameStartCallback)(void*, void*);

  // Registers a new OnGCControllerPolled callback function (the 2nd parameter passed into the
  // function), and returns a handle that can be used to unregister the callback later on.
  void* (*RegisterOnGCControllerPolledCallback)(void*, void*);

  // Registers a new OnGCControllerPolled callback function (the 2nd parameter passed into the
  // function). This callback should auto-deregister when only auto-deregistered callbacks are still
  // running.
  void (*RegisterOnGCControllerPolledWithAutoDeregistrationCallback)(void*, void*);

  // Unregisters/removes an OnGCControllerPolled callback (the 2nd parameter passed into the
  // function should be the return result of RegisterOnGCControllerPolledCallback()). Should return
  // 1 if attempt to unregister succeded, and should return 0 otherwise.
  int (*UnregisterOnGCControllerPolledCallback)(void*, void*);

  // Registers a new OnWiiInputPolled callback function (the 2nd paramter passed into the
  // function), and returns a handle that can be used to unregister the callback later on.
  void* (*RegisterOnWiiInputPolledCallback)(void*, void*);

  // Registers a new OnWiiInputPolled callback function (the 2nd parameter passed into the
  // function). This callback should auto-deregister when only auto-deregistered callbacks are still
  // running.
  void (*RegisterOnWiiInputPolledWithAutoDeregistrationCallback)(void*, void*);

  // Unregisters/removes an OnWiiInputPolled callback (the 2nd parameter passed into
  // the function should be the return result of RegisterOnWiiInputPolledCallback()). Should return
  // 1 if attempt to unregister succeded, and should return 0 otherwise.
  int (*UnregisterOnWiiInputPolledCallback)(void*, void*);

  // Registers a new OnInstructionHit callback function (the 3rd paramater passed into the
  // function) to run when the instruction address specified by the 2nd parameter to
  // the function is hit. This function returns a handle that can be used to unregister
  // the callback later on.
  void* (*RegisterOnInstructionHitCallback)(void*, unsigned int, void*);

  // Registers a new OnInstructionHit callback function (the 3rd parameter passed into the
  // function) to run when the instruction address specified by the 2nd parameter to the function is
  // hit. This function should auto-deregister when only auto-deregistered callbacks are still
  // running.
  void (*RegisterOnInstructionHitWithAutoDeregistrationCallback)(void*, unsigned int, void*);

  // Unregisters/removes an OnInstructionHit callback (the 3rd parameter passed into the function
  // should be the return result of RegisterOnInstructionHitCallback()) for the address specified by
  // the 2nd parameter. Should return 1 if attempt to unregister succeded, and should return 0
  // otherwise.
  int (*UnregisterOnInstructionHitCallback)(void*, unsigned int, void*);

  // Registers a new OnMemoryAddressReadFrom callback function (the 4th parameter passed into the
  // function) to run when a memory address which is >= the 2nd parameter to the function and <= the
  // 3rd parameter to the function is read from. The function returns a handle that can be used to
  // unregister the callback later on.
  void* (*RegisterOnMemoryAddressReadFromCallback)(void*, unsigned int, unsigned int, void*);

  // Registers a new OnMemoryAddressReadFrom callback function (the 4th parameter passed into the
  // function) to run when a memory address which is >= the 2nd parameter to the function and <=
  // the 3rd parameter to the function is read from. This function should auto-deregister when only
  // auto-deregistered callbacks are still running.
  void (*RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback)(void*, unsigned int,
                                                                        unsigned int, void*);

  // Unregisters/removes an OnMemoryAddressReadFrom callback (the 2nd parameter passed into the
  // function should be the return result of RegisterOnMemoryAddressReadFromCallback()). Should
  // return 1 if attempt to unregister succeded, and should return 0 otherwise.
  int (*UnregisterOnMemoryAddressReadFromCallback)(void*, void*);

  // Registers a new OnMemoryAddressWrittenTo callback function (the 4th parameter passed into the
  // function) to run when a memory address which is >= the 2nd parameter to the function and <= the
  // 3rd parameter to the function is written to. The function returns a handle that can be used to
  // unregister the callback later on.
  void* (*RegisterOnMemoryAddressWrittenToCallback)(void*, unsigned int, unsigned int, void*);

  // Registers a new OnMemoryAddressWrittenTo callback function (the 4th parameter passed into the
  // function) to run when a memory address which is >= the 2nd parameter to the function and <= the
  // 3rd parameter to the function is written to. This function should auto-deregister when only
  // auto-deregistered callbacks are still running.
  void (*RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback)(void*, unsigned int,
                                                                         unsigned int, void*);

  // Unregisters/removes an OnMemoryAddressWrittenTo callback (the 2nd parameter passed into the
  // function should be the return result of RegisterOnMemoryAddressWrittenToCallback()). Should
  // return 1 if attempt to unregister succeded, and should return 0 otherwise.
  int (*UnregisterOnMemoryAddressWrittenToCallback)(void*, void*);

  // Registers a callback function (the 3rd parameter passed into the function) to run when the
  // button with the ID specified by the 2nd parameter is clicked.
  void (*RegisterButtonCallback)(void*, long long, void*);

  //  Returns 1 if the 2nd parameter represents a button ID with a registered callback, and 0
  //  otherwise.
  int (*IsButtonRegistered)(void*, long long);

  // Queues the callback associated with the button ID (the 2nd parameter passed into the function)
  // to run the next time RunButtonCallbacksInQueue() is called.
  void (*GetButtonCallbackAndAddToQueue)(void*, long long);

  // Runs all button callbacks which are queued to run in the order that they were added to the
  // queue (and removes all of them from the queue afterwards). This function should only be called
  // by Dolphin, and not by the DLL.
  void (*RunButtonCallbacksInQueue)(void*);

  // This function is called by Dolphin when the DLL calls the
  // Send_ClassMetadataForVersion_To_DLL_Buffer function (from ClassFunctionsResolver_APIs). This
  // function gives the DLL a chance to copy the ClassMetadata*'s variables to the DLL's fields,
  // since it will remain in scope until the end of this function. 1st param is ScriptContext*, 2nd
  // param is an opaque handle to a ClassMetadata*. Can use the methods in ClassMetadata_APIs to
  // extract the fields of the opaque ClassMetadata*
  void (*DLLClassMetadataCopyHook)(void*, void*);

  // This function is called by Dolphin when the DLL calls the
  // Send_FunctionMetadataForVersion_To_DLL_Buffer function (from ClassResolver_APIs). This gives
  // the DLL a chance to copy the FunctionMetadata*'s variables to the DLL's fields, since it will
  // remain in scope until the end of this function. 1st param is ScriptContext*, 2nd param is an
  // opaque handle to a FunctionMetadata*. Can use the methods in FunctionMetadata_APIs to extract
  // the fields of the opaque FunctionMetadata*
  void (*DLLFunctionMetadataCopyHook)(void*, void*);

  // The destructor for the DLL-specific object(s) associated with the ScriptContext* (this is
  // called by Dolphin in ScriptContext_Destructor)
  void (*DLLSpecificDestructor)(void*);

} DLL_Defined_ScriptContext_APIs;

#ifdef __cplusplus
}
#endif

}  // namespace Scripting
