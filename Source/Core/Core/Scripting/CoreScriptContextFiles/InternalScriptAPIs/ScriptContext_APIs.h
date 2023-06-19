#ifndef SCRIPT_CONTEXT_APIs
#define SCRIPT_CONTEXT_APIs

#ifdef __cplusplus
extern "C" {
#endif


  // These are functions whose definitions are provided by Dolphin. The void* which is passed as a parameter to each
  // function represents a ScriptContext*
  //These functions are the same for all DLLs.
 typedef struct Dolphin_Defined_ScriptContext_APIs
{
  typedef void (*PRINT_CALLBACK_FUNCTION_TYPE)(void*, const char*);
  typedef void (*SCRIPT_END_CALLBACK_FUNCTION_TYPE)(void*, int);

   void* (*ScriptContext_Initializer)(int, const char*, PRINT_CALLBACK_FUNCTION_TYPE, SCRIPT_END_CALLBACK_FUNCTION_TYPE, void*);  // Creates a new ScriptContext struct, and returns a
                                           // pointer to it.
   // The first parameter is the unique ID of the script, the 2nd parameter is the script filename, the 3rd parameter is the print_callback function (invoked when a script calls its print function)
   // the 4th parameter is the script_end_callback (invoked when a script terminates), and the 5th parameter is a DLL_Defined_ScriptContext_APIs*

   void (*ScriptContext_Destructor)(void*); // Calls, the DLL-specific destructor, and then frees the memory allocated by the ScriptContext* passed into the function.

   void (*Shutdown_Script)(void*);  // Sets is_script_active to false, and calls the script_end_callback
  PRINT_CALLBACK_FUNCTION_TYPE (*get_print_callback_function)(void*);  // Returns the function pointer to the print_callback function for the ScriptContext which is passed into the function.
  SCRIPT_END_CALLBACK_FUNCTION_TYPE (*get_script_end_callback_function)(void*); // Returns the function pointer to the script_end_callback function for the ScriptContext which is passed into the function.

  int (*get_unique_script_identifier)(void*); // Returns the integer that represents the unique ID for the ScriptContext*

  const char* (*get_script_filename)(void*); // Retuns the filename for the script.

  int (*get_script_call_location)(void*); // Returns the location that the Script is executing from.
  int (*get_is_script_active)(void*); // Returns 1 if the ScriptContext is currently active, and 0 otherwise.
  void (*set_is_script_active)(void*, int); // Sets the ScriptContext's is_script_active variable to the specified value.

  int (*get_is_finished_with_global_code)(void*); // Returns 1 if the ScriptContext has finished running all global code, and 0 otherwise.
  void (*set_is_finished_with_global_code)(void*, int); // Sets the ScriptContext's is_finished_with_global_code variable to the specified value.

  int (*get_called_yielding_function_in_last_global_script_resume)(void*); // Returns 1 if the ScriptContext yielded in its last global scope code, and 0 otherwise.
  void (*set_called_yielding_function_in_last_global_script_resume)(void*, int); //  Sets the ScriptContext's called_yielding_function_in_last_global_script_resume variable to the specified value

  int (*get_called_yielding_function_in_last_frame_callback_script_resume)(void*); // Returns 1 if the ScriptContext yielded in its last frame callback code, and 0 otherwise.
  void (*set_called_yielding_function_in_last_frame_callback_script_resume)(void*, int); // Sets the ScriptContext's called_yielding_function_in_last_frame_callback_script_resume variable to the sspecified value.

  void* (*get_instruction_breakpoints_holder)(void*); // Returns a InstructionBreakpointsHolder*, which is a type defined by Dolphin.
  void* (*get_memory_address_breakpoints_holder)(void*); // Returns a MemoryAddressBreakpointsHolder*, which is a type defined by Dolphin.

  void* (*get_dll_defined_script_context_apis)(void*);  // Returns a DLL_Defined_ScriptContext_APIs struct (see below for definition)

  void* (*get_derived_script_context_class_ptr)(void*); // Returns a pointer to the derived ScriptContext* class (ex. a LuaScriptContext* defined in the DLL)

  const char* (*get_script_version)(); // this is the same for all scripts, so no void* is passed into it.

  void(*set_dll_script_context_ptr)(void*, void*);  // Sets the pointer to the DLL (ex. a LuaScriptContext*)

  void (*add_script_to_queue_of_scripts_waiting_to_start)(void*); // Adds the script passed in as an argument to the queue of scripts waiting to be started.

} Dolphin_Defined_ScriptContext_APIs;

 //These are functions whose definitions are provided by the implementing DLL (each DLL has their own definitions for these functions)
 // The first parameter for each function is a void* which represents a ScriptContext*
 // A pointer to the struct containing these APIs is included in ScriptContext.dll_specific_api_definitions.
typedef struct DLL_Defined_ScriptContext_APIs
{
  void (*ImportModule)(void*, const char*, const char*); // 1st parameter is ScriptContext*, 2nd parameter is API name, and 3rd parameter is API version. Imports the specified module into the ScriptContext.

  void (*StartScript)(void*); // Makes the Script start running for the 1st time.

  void (*RunGlobalScopeCode)(void*); // Continues running the Script from the global level (used to resume a script which has already started, which isn't supported by all languages).
  void (*RunOnFrameStartCallbacks)(void*); // Runs all the OnFrameStart callbacks for the script.
  void (*RunOnGCControllerPolledCallbacks)(void*); // Runs all the OnGCControllerPolled callbacks for the script.
  void (*RunOnInstructionReachedCallbacks)(void*, unsigned int); // Runs all OnInstructionReached callbacks for the specified instruction address for the script.
  void (*RunOnMemoryAddressReadFromCallbacks)(void*, unsigned int); // Runs all OnMemoryAddressReadFrom callbacks for the specified memory address for the script.
  void (*RunOnMemoryAddressWrittenToCallbacks)(void*, unsigned int); // Runs all OnMemoryAddressWrittenTo callbacks for the specified memory address for the script.
  void (*RunOnWiiInputPolledCallbacks)(void*); // Runs all OnWiiInputPolled callbacks for the script.

  void* (*RegisterOnFrameStartCallback)(void*, void*); // Registers a new OnFrameStart callback (the 2nd parameter passed into the function), and returns a handle that can be used to unregister the callback.
  void (*RegisterOnFrameStartWithAutoDeregistrationCallback)(void*, void*); // Registers a new OnFrameStart callback (the 2nd parameter passed into the function). This callback should auto-deregister when only auto-deregistered callbacks are still running.
  int (*UnregisterOnFrameStartCallback)(void*, void*); // Unregisters/removes an OnFrameStart callback (the 2nd parameter passed into the function should be the return result of RegisterOnFrameStartCallback()).

  void* (*RegisterOnGCControllerPolledCallback)(void*, void*); // Registers a new OnGCControllerPolled callback (the 2nd parameter passed into the function), and returns a handle that can be used to unregister the callback.
  void (*RegisterOnGCControllerPolledWithAutoDeregistrationCallback)(void*, void*); // Registers a new OnGCControllerPolled callback (the 2nd parameter passed into the function). This callback should auto-deregister when only auto-deregistered callbacks are still running.
  int (*UnregisterOnGCControllerPolledCallback)(void*, void*); // Unregisters/removes an OnGCControllerPolled callback (the 2nd parameter passed into the function should be the return result of RegisterOnGCControllerPolledCallback()).

  void* (*RegisterOnInstructionReachedCallback)(void*, unsigned int, void*); // Registers a new OnInstructionReached callback (the 3rd paramater passed into the function) to run when the instruction address specified by the 2nd parameter to the function is hit. This function returns a handle that can be used to unregister the callback.
  void (*RegisterOnInstructioReachedWithAutoDeregistrationCallback)(void*, unsigned int, void*); // Registers a new OnInstructionReached callback (the 3rd parameter passed into the function) to run when the instruction address specified by the 2nd parameter to the function is hit. This function should auto-deregister when only auto-deregistered callbacks are still running.
  int (*UnregisterOnInstructionReachedCallback)(void*, unsigned int, void*); // Unregisters/removes an OnInstructionReached callback (the 3rd parameter passed into the function should be the return result of RegisterOnInstructionReachedCallback()) for the specified instruction address (the 2nd parameter).

  void* (*RegisterOnMemoryAddressReadFromCallback)(void*, unsigned int, void*); // Registers a new OnMemoryAddressReadFrom callback (the 3rd parameter passed into the function) to run when the memory address specified by the 2nd parameter to the function is read from. The function returns a handle that can be used to unregister the callback.
  void (*RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback)(void*, unsigned int, void*); // Registers a new OnMemoryAddressReadFrom callback (the 3rd parameter passed into the function) to run when the memory address specified by the 2nd parameter to the function is read from. This function should auto-deregister when only auto-deregistered callbacks are still running.
  int (*UnregisterOnMemoryAddressReadFromCallback)(void*, unsigned int, void*); // Unregisters/removes an OnMemoryAddressReadFrom callback (the 3rd parameter passed into the function should be the return result of RegisterOnMemoryAddressReadFromCallback()) for the specified memory address (the 2nd parameter).

  void* (*RegisterOnMemoryAddressWrittenToCallback)(void*, unsigned int, void*); //Registers a new OnMemoryAddressWrittenTo callback (the 3rd parameter passed into the function) to run when the memory address specified by the 2nd parameter to the function is written to. The function returns a handle that can be used to unregister the callback.
  void (*RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback)(void*, unsigned int, void*); // Registers a new OnMemoryAddressWrittenTo callback (the 3rd parameter passed into the function) to run when the memory address specified by the 2nd parameter to the function is written to. This function should auto-deregister when only auto-deregistered callbacks are still running.
  int (*UnregisterOnMemoryAddressWrittenToCallback)(void*, unsigned int, void*); // Unregisters/removes an OnMemoryAddressWrittenTo callback (the 3rd parameter passed into the function should be the return result of RegisterOnMemoryAddressWrittenToCallback()) for the specified memory address (the 3rd parameter).

  void* (*RegisterOnWiiInputPolledCallback)(void*, void*); // Registers a new OnWiiInputPolled callback (the 2nd paramter passed into the function), and returns a handle that can be used to unregister the callback.
  void(*RegisterOnWiiInputPolledWithAutoDeregistrationCallback)(void*, void*); // Registers a new OnWiiInputPolled callback (the 2nd parameter passed into the function). This callback should auto-deregister when only auto-deregistered callbacks are still running.
  int (*UnregisterOnWiiInputPolledCallback)(void*, void*); // Unregisters/removes an OnWiiInputPolled callback (the 2nd parameter  passed into the function should be the return result of RegisterOnWiiInputPolledCallback()).

  void (*RegisterButtonCallback)(void*, long long, void*); // Registers a callback (the 3rd parameter passed into the function) to run when the button with the ID specified by the 2nd parameter is clicked.
  int (*IsButtonRegistered)(void*, long long); //  Returns 1 if the 2nd parameter represents a button ID with a registered callback, and false otherwise.
  void (*GetButtonCallbackAndAddToQueue)(void*, long long); // Queues the callback passed into the function to run the next time RunButtonCallbacksInQueue() is called.
  void (*RunButtonCallbacksInQueue)(void*); // Runs all button callbacks which are queued to run.

  void (*DLLClassMetadataCopyHook)(void*, void*);  // Called by Dolphin when the DLL requests a ClassMetadata object. This gives the DLL a chance to copy the ClassMetadata object's variables to the DLL's pecific ScriptContext* fields, since it will remain in scope until the end of this function.  1st param is ScriptContext*, 2nd param is an opaque  handler which is a ClassMetadata*
  void (*DLLFunctionMetadataCopyHook)(void*, void*);  // Called by Dolphin when the DLL requests a specific FunctionMetadata object (when it passes in module name + version + function name, and expects a FunctionMetadata to be returned). This gives the DLL a chance to copy the FunctionMetadata object's variables to the DLL's specific ScriptContext* fields, since it will remain in scope until the end of this function. 1st param is ScriptContext*, 2nd param is an opaque handler which is a FunctionMetadata*

  void (*DLLSpecificDestructor)(void*);
} DLL_Defined_ScriptContext_APIs;

#ifdef __cplusplus
}
#endif

#endif
