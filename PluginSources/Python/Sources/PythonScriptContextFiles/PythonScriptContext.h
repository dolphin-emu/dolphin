#ifndef PYTHON_SCRIPT_CONTEXT_H
#define PYTHON_SCRIPT_CONTEXT_H

#include "../DolphinDefinedAPIs.h"
#include "../HelperFiles/ClassMetadata.h"
#include "../HelperFiles/IdentifierToCallback.h"
#include "../HelperFiles/MemoryAddressCallbackTriple.h"
#include <atomic>
#include <unordered_map>
#include <vector>
#include <queue>

#ifdef __cplusplus
extern "C" {
#endif

  struct PythonScriptContext
  {
    PythonScriptContext(void* new_base_script_context_ptr) {
      base_script_context_ptr = new_base_script_context_ptr;
      main_python_thread = nullptr;
      button_callbacks_to_run = std::queue<IdentifierToCallback>();
      frame_callbacks = std::vector<IdentifierToCallback>();
      gc_controller_input_polled_callbacks = std::vector<IdentifierToCallback>();
      wii_controller_input_polled_callbacks = std::vector<IdentifierToCallback>();
      map_of_instruction_address_to_python_callbacks = std::unordered_map<unsigned int, std::vector<IdentifierToCallback> >();
      memory_address_read_from_callbacks = std::vector<MemoryAddressCallbackTriple>();
      memory_address_written_to_callbacks = std::vector<MemoryAddressCallbackTriple>();
      map_of_button_id_to_callback = std::unordered_map<long long, IdentifierToCallback>();
      number_of_frame_callbacks_to_auto_deregister = 0;
      number_of_gc_controller_input_callbacks_to_auto_deregister = 0;
      number_of_instruction_address_callbacks_to_auto_deregister = 0;
      number_of_memory_address_read_callbacks_to_auto_deregister = 0;
      number_of_memory_address_write_callbacks_to_auto_deregister = 0;
      number_of_wii_input_callbacks_to_auto_deregister = 0;
      next_unique_identifier_for_callback = 1;
      class_metadata_buffer = ClassMetadata();
      single_function_metadata_buffer = FunctionMetadata();
      classes_to_delete = {};
      py_method_defs_to_delete = {};
    }

    void* base_script_context_ptr;

    void* main_python_thread;

    std::queue<IdentifierToCallback> button_callbacks_to_run;
    std::vector<IdentifierToCallback> frame_callbacks;
    std::vector<IdentifierToCallback> gc_controller_input_polled_callbacks;
    std::vector<IdentifierToCallback> wii_controller_input_polled_callbacks;

    std::unordered_map<unsigned int, std::vector<IdentifierToCallback> > map_of_instruction_address_to_python_callbacks;

    std::vector<MemoryAddressCallbackTriple> memory_address_read_from_callbacks;
    std::vector<MemoryAddressCallbackTriple> memory_address_written_to_callbacks;

    std::unordered_map<long long, IdentifierToCallback> map_of_button_id_to_callback;

    std::atomic<size_t> number_of_frame_callbacks_to_auto_deregister;
    std::atomic<size_t> number_of_gc_controller_input_callbacks_to_auto_deregister;
    std::atomic<size_t> number_of_wii_input_callbacks_to_auto_deregister;
    std::atomic<size_t> number_of_instruction_address_callbacks_to_auto_deregister;
    std::atomic<size_t> number_of_memory_address_read_callbacks_to_auto_deregister;
    std::atomic<size_t> number_of_memory_address_write_callbacks_to_auto_deregister;

    std::atomic<size_t>
      next_unique_identifier_for_callback;  // When a callback is registered, this is used as the
    // value for it, and is then incremented by 1. Unless of
    // course a registerWithAutoDeregister callback or
    // button callback are registered. When a
    // registerWithAutoDeregister or button callback is
    // registered, it's assigned an identifier of 0 (which
    // the user can't deregister).


    ClassMetadata class_metadata_buffer;
    FunctionMetadata single_function_metadata_buffer; // Unused - but allows for FunctionMetadatas to be run, if the DLLFunctionMetadataCopyHook_impl() is ever used

    std::vector<ClassMetadata*> classes_to_delete;
    std::vector<void*> py_method_defs_to_delete;
  };

  void* Init_PythonScriptContext_impl(void* new_base_script_context_ptr); // Creates + returns a new PythonScriptContext object, with all fields properly initialized from the base ScriptContext* passed into the function
  void Destroy_PythonScriptContext_impl(void* base_script_context_ptr); // Takes as input a ScriptContext*, and frees the associated PythonScriptContext*
  bool ShouldCallEndScriptFunction(void*);

  // See the declarations in the DLL_Defined_ScriptContext_APIs struct in the ScriptContext_APIs.h file for documentation on what these functions do.
  // Note that each of these functions takes as input for its 1st parameter an opaque void* handle to a ScriptContext* (in order to be callable from the Dolphin side).
  // However, you can use the get_derived_script_context_class_ptr() method on the ScriptContext* in order to get the PythonScriptContext* associated with the ScriptContext* object.

  void ImportModule_impl(void*, const char*, const char*);

  void* RunFunction_impl(void*, FunctionMetadata*, void*, void*);

  void StartScript_impl(void*);
  void RunGlobalScopeCode_impl(void*);
  void RunOnFrameStartCallbacks_impl(void*);
  void RunOnGCControllerPolledCallbacks_impl(void*);
  void RunOnWiiInputPolledCallbacks_impl(void*);
  void RunButtonCallbacksInQueue_impl(void*);
  void RunOnInstructionReachedCallbacks_impl(void*, unsigned int);
  void RunOnMemoryAddressReadFromCallbacks_impl(void*, unsigned int);
  void RunOnMemoryAddressWrittenToCallbacks_impl(void*, unsigned int);


  void* RegisterOnFrameStartCallback_impl(void*, void*);
  void RegisterOnFrameStartWithAutoDeregistrationCallback_impl(void*, void*);
  int UnregisterOnFrameStartCallback_impl(void*, void*);

  void* RegisterOnGCControllerPolledCallback_impl(void*, void*);
  void RegisterOnGCControllerPolledWithAutoDeregistrationCallback_impl(void*, void*);
  int UnregisterOnGCControllerPolledCallback_impl(void*, void*);

  void* RegisterOnWiiInputPolledCallback_impl(void*, void*);
  void RegisterOnWiiInputPolledWithAutoDeregistrationCallback_impl(void*, void*);
  int UnregisterOnWiiInputPolledCallback_impl(void*, void*);

  void RegisterButtonCallback_impl(void*, long long, void*);
  int IsButtonRegistered_impl(void*, long long);
  void GetButtonCallbackAndAddToQueue_impl(void*, long long);

  void* RegisterOnInstructionReachedCallback_impl(void*, unsigned int, void*);
  void RegisterOnInstructionReachedWithAutoDeregistrationCallback_impl(void*, unsigned int, void*);
  int UnregisterOnInstructionReachedCallback_impl(void*, void*);

  void* RegisterOnMemoryAddressReadFromCallback_impl(void*, unsigned int, unsigned int, void*);
  void RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback_impl(void*, unsigned int, unsigned int, void*);
  int UnregisterOnMemoryAddressReadFromCallback_impl(void*, void*);

  void* RegisterOnMemoryAddressWrittenToCallback_impl(void*, unsigned int, unsigned int, void*);
  void RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback_impl(void*, unsigned int, unsigned int, void*);
  int UnregisterOnMemoryAddressWrittenToCallback_impl(void*, void*);

  void DLLClassMetadataCopyHook_impl(void*, void*);
  void DLLFunctionMetadataCopyHook_impl(void*, void*);

#ifdef __cplusplus
}
#endif

#endif
