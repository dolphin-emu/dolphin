#ifndef SCRIPT_CONTEXT_APIS
#define SCRIPT_CONTEXT_APIS

#ifdef __cplusplus
extern "C" {
#endif

// These are functions whose definitions are provided by Dolphin. The void* which is passed as a
// parameter to each function represents a ScriptContext*
// These functions are the same for all DLLs.
typedef struct Dolphin_Defined_ScriptContext_APIs
{
  typedef void (*PRINT_CALLBACK_FUNCTION_TYPE)(void*, const char*);
  typedef void (*SCRIPT_END_CALLBACK_FUNCTION_TYPE)(void*, int);

  // Creates a new ScriptContext struct, and returns a  pointer to it. The first parameter is the
  // unique ID of the script, the 2nd parameter is the script filename, the 3rd parameter is the
  // print_callback function (invoked when a script calls its print function), the 4th parameter is
  // the script_end_callback (invoked when a script terminates), and the 5th parameter is a
  // DLL_Defined_ScriptContext_APIs*
  void* (*ScriptContext_Initializer)(int, const char*, PRINT_CALLBACK_FUNCTION_TYPE,
                                     SCRIPT_END_CALLBACK_FUNCTION_TYPE, void*);

  // Calls the DLL-Specific destructor, and then frees the memory allocated by the ScriptContext*
  // passed into the function.
  void (*ScriptContext_Destructor)(void*);

  // Returns the function pointer to the print_callback function for the ScriptContext* which is
  // passed into the function.
  PRINT_CALLBACK_FUNCTION_TYPE (*get_print_callback_function)(void*);

  // Returns the function pointer to the script_end_callback function for the ScriptContext* which
  // is passed into the function.
  SCRIPT_END_CALLBACK_FUNCTION_TYPE (*get_script_end_callback_function)(void*);

  // Returns the integer that represents the unique ID for the ScriptContext*
  int (*get_unique_script_identifier)(void*);

  // Retuns the filename for the script.
  const char* (*get_script_filename)(void*);

  // Returns the location that the ScriptContext* is executing from.
  int (*get_script_call_location)(void*);

  // Gets the return code of the last script execution (which is really of type ScriptReturnCodes).
  int (*get_script_return_code)(void*);

  // sets the return code for the last script execution (should only be called by the DLL).
  // 2nd parameter represents a ScriptReturnCodes enum.
  void (*set_script_return_code)(void*, int);

  // Returns the error message from the last script execution (only valid when script_return_code is
  // not Success)
  const char* (*get_error_message)(void*);

  // Sets the error message from the last script execution to the specified value (should only be
  // called by the DLL).
  void (*set_error_message)(void*, const char*);

  // Returns 1 if the ScriptContext* is currently active, and 0 otherwise.
  int (*get_is_script_active)(void*);

  // Sets the ScriptContext*'s is_script_active variable to the specified value.
  void (*set_is_script_active)(void*, int);

  // Returns 1 if the ScriptContext* has finished running all global code, and 0 otherwise.
  int (*get_is_finished_with_global_code)(void*);

  // Sets the ScriptContext*'s is_finished_with_global_code variable to the specified value.
  void (*set_is_finished_with_global_code)(void*, int);

  // Returns 1 if the ScriptContext* yielded in its last global scope code, and 0 otherwise.
  int (*get_called_yielding_function_in_last_global_script_resume)(void*);

  //  Sets the ScriptContext*'s called_yielding_function_in_last_global_script_resume variable to
  //  the specified value
  void (*set_called_yielding_function_in_last_global_script_resume)(void*, int);

  // Returns 1 if the ScriptContext* yielded in its last frame callback code, and 0 otherwise.
  int (*get_called_yielding_function_in_last_frame_callback_script_resume)(void*);

  // Sets the ScriptContext*'s called_yielding_function_in_last_frame_callback_script_resume
  // variable to the specified value.
  void (*set_called_yielding_function_in_last_frame_callback_script_resume)(void*, int);

  // Returns the DLL_Defined_ScriptContext_APIs* for the ScriptContext* (see below for definition)
  void* (*get_dll_defined_script_context_apis)(void*);

  // Sets the DLL_Defined_ScriptContext_APIs* for the ScriptContext*
  void (*set_dll_defined_script_context_apis)(void*, void*);

  // Returns a pointer to the associated derived ScriptContext* object (ex. a LuaScriptContext*
  // defined in the DLL)
  void* (*get_derived_script_context_ptr)(void*);

  // Sets the associated derived ScriptContext* for the ScriptContext* to the specified value.
  void (*set_derived_script_context_ptr)(void*, void*);

  // This is the same for all scripts, so no void* is passed into it.
  const char* (*get_script_version)();

  // Adds the ScriptContext* passed in as an argument to the queue of scripts waiting to be started.
  void (*add_script_to_queue_of_scripts_waiting_to_start)(void*);

} Dolphin_Defined_ScriptContext_APIs;

// These are functions whose definitions are provided by the implementing DLL (each DLL has their
// own definitions for these functions) The first parameter for each function is a void* which
// represents a ScriptContext* (NOT a derived class pointer) A pointer to the struct containing
// these APIs is included in ScriptContext.dll_specific_api_definitions.
typedef struct DLL_Defined_ScriptContext_APIs
{
  // 1st parameter is ScriptContext*, 2nd parameter is API name, and 3rd parameter is API version.
  // Imports the specified module into the ScriptContext* (scripts will be able to call the imported
  // functions after this function terminates).
  void (*ImportModule)(void*, const char*, const char*);

  // Makes the Script start running for the 1st time.
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

  // Runs all OnInstructionReached callbacks currently registered for the specified instruction
  // address for the ScriptContext*
  void (*RunOnInstructionReachedCallbacks)(void*, unsigned int);

  // Runs all OnMemoryAddressReadFrom callbacks currently registered for the ScriptContext* which
  // have a start address <= to the 2nd parameter to this function and an end address >= the 2nd
  // parameter to this function (2nd param needs to be inbetween start and end range for callback).
  void (*RunOnMemoryAddressReadFromCallbacks)(void*, unsigned int);

  // Runs all OnMemoryAddressWrittenTo callbacks currently registered for the ScriptContext* which
  // have a start address <= to the 2nd parameter to this function and an end address >= the 2nd
  // parameter to this function (2nd param needs to be inbetween start and end range for callback).
  void (*RunOnMemoryAddressWrittenToCallbacks)(void*, unsigned int);

  // Registers a new OnFrameStart callback function (the 2nd parameter passed into the function),
  // and returns a handle that can be used to unregister the callback later on.
  void* (*RegisterOnFrameStartCallback)(void*, void*);

  // Registers a new OnFrameStart callback function (the 2nd parameter passed into the function).
  // This callback should auto-deregister when only auto-deregistered callbacks are still running.
  void (*RegisterOnFrameStartWithAutoDeregistrationCallback)(void*, void*);

  // Unregisters/removes an OnFrameStart callback (the 2nd parameter passed into the function should
  // be the return result of RegisterOnFrameStartCallback()).
  int (*UnregisterOnFrameStartCallback)(void*, void*);

  // Registers a new OnGCControllerPolled callback function (the 2nd parameter passed into the
  // function), and returns a handle that can be used to unregister the callback later on.
  void* (*RegisterOnGCControllerPolledCallback)(void*, void*);

  // Registers a new OnGCControllerPolled callback function (the 2nd parameter passed into the
  // function). This callback should auto-deregister when only auto-deregistered callbacks are still
  // running.
  void (*RegisterOnGCControllerPolledWithAutoDeregistrationCallback)(void*, void*);

  // Unregisters/removes an OnGCControllerPolled callback (the 2nd parameter passed into the
  // function should be the return result of RegisterOnGCControllerPolledCallback()).
  int (*UnregisterOnGCControllerPolledCallback)(void*, void*);

  // Registers a new OnWiiInputPolled callback function (the 2nd paramter passed into the
  // function), and returns a handle that can be used to unregister the callback later on.
  void* (*RegisterOnWiiInputPolledCallback)(void*, void*);

  // Registers a new OnWiiInputPolled callback function (the 2nd parameter passed into the
  // function). This callback should auto-deregister when only auto-deregistered callbacks are still
  // running.
  void (*RegisterOnWiiInputPolledWithAutoDeregistrationCallback)(void*, void*);

  // Unregisters/removes an OnWiiInputPolled callback (the 2nd parameter passed into
  // the function should be the return result of RegisterOnWiiInputPolledCallback()).
  int (*UnregisterOnWiiInputPolledCallback)(void*, void*);

  // Registers a new OnInstructionReached callback function (the 3rd paramater passed into the
  // function) to run when the instruction address specified by the 2nd parameter to
  // the function is hit. This function returns a handle that can be used to unregister
  // the callback later on.
  void* (*RegisterOnInstructionReachedCallback)(void*, unsigned int, void*);

  // Registers a new OnInstructionReached callback function (the 3rd parameter passed into the
  // function) to run when the instruction address specified by the 2nd parameter to the function is
  // hit. This function should auto-deregister when only auto-deregistered callbacks are still
  // running.
  void (*RegisterOnInstructionReachedWithAutoDeregistrationCallback)(void*, unsigned int, void*);

  // Unregisters/removes an OnInstructionReached callback (the 2nd parameter passed into the
  // function should be the return result of RegisterOnInstructionReachedCallback()).
  int (*UnregisterOnInstructionReachedCallback)(void*, void*);

  // Registers a new OnMemoryAddressReadFrom callback function (the 4th parameter passed into the
  // function) to run when a memory address which is >= the 2nd parameter to the function and <=
  // the 3rd parameter to the function is read from. The function returns a handle that can be used
  // to unregister the callback later on.
  void* (*RegisterOnMemoryAddressReadFromCallback)(void*, unsigned int, unsigned int, void*);

  // Registers a new OnMemoryAddressReadFrom callback function (the 4th parameter passed into the
  // function) to run when a memory address which is >= the 2nd parameter to the function and <= the
  // 3rd parameter to the function is read from. This function should auto-deregister when only
  // auto-deregistered callbacks are still running.
  void (*RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback)(void*, unsigned int,
                                                                        unsigned int, void*);

  // Unregisters/removes an OnMemoryAddressReadFrom callback (the 2nd parameter passed into the
  // function should be the return result of RegisterOnMemoryAddressReadFromCallback()).
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
  // function should be the return result of RegisterOnMemoryAddressWrittenToCallback()).
  int (*UnregisterOnMemoryAddressWrittenToCallback)(void*, void*);

  // Registers a callback function (the 3rd parameter passed into the function) to run when the
  // button with the ID specified by the 2nd parameter is clicked.
  void (*RegisterButtonCallback)(void*, long long, void*);

  //  Returns 1 if the 2nd parameter represents a button ID with a registered callback, and 0
  //  otherwise.
  int (*IsButtonRegistered)(void*, long long);

  // Queues the callback passed into the function to run the next time RunButtonCallbacksInQueue()
  // is called.
  void (*GetButtonCallbackAndAddToQueue)(void*, long long);

  // Runs all button callbacks which are queued to run in the order that they were added to the
  // queue (and removes all of them from the queue afterwards).
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
  // called by Dolphin).
  void (*DLLSpecificDestructor)(void*);

} DLL_Defined_ScriptContext_APIs;

#ifdef __cplusplus
}
#endif

#endif
