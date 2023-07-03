#include "CreateScript.h"
#include "PythonScriptContextFiles/PythonScriptContext.h"
#include "HelperFiles/PythonDynamicLibrary.h"

static DLL_Defined_ScriptContext_APIs dll_defined_scriptContext_apis = {};
static bool initialized_dll_specific_script_context_apis = false;

// Validates that there's no NULL variables in the API struct passed in as an argument.
static bool ValidateApiStruct(void* start_of_struct, unsigned int struct_size, const char* struct_name)
{
  unsigned long long* travel_ptr = ((unsigned long long*)start_of_struct);

  while (((unsigned char*)travel_ptr) < (((unsigned char*)start_of_struct) + struct_size))
  {
    if (static_cast<unsigned long long>(*travel_ptr) == 0)
    {
      std::quick_exit(68);
    }
    travel_ptr = (unsigned  long long*)(((unsigned char*)travel_ptr) + sizeof(unsigned long long));
  }

  return true;
}

static void InitDLLSpecificAPIs()
{
  dll_defined_scriptContext_apis.DLLClassMetadataCopyHook = DLLClassMetadataCopyHook_impl;
  dll_defined_scriptContext_apis.DLLFunctionMetadataCopyHook = DLLFunctionMetadataCopyHook_impl;
  dll_defined_scriptContext_apis.DLLSpecificDestructor = Destroy_PythonScriptContext_impl;
  dll_defined_scriptContext_apis.GetButtonCallbackAndAddToQueue = GetButtonCallbackAndAddToQueue_impl;
  dll_defined_scriptContext_apis.ImportModule = ImportModule_impl;
  dll_defined_scriptContext_apis.IsButtonRegistered = IsButtonRegistered_impl;
  dll_defined_scriptContext_apis.RegisterButtonCallback = RegisterButtonCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnFrameStartCallback = RegisterOnFrameStartCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnFrameStartWithAutoDeregistrationCallback = RegisterOnFrameStartWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnGCControllerPolledCallback = RegisterOnGCControllerPolledCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnGCControllerPolledWithAutoDeregistrationCallback = RegisterOnGCControllerPolledWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnInstructionReachedCallback = RegisterOnInstructionReachedCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnInstructionReachedWithAutoDeregistrationCallback = RegisterOnInstructionReachedWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnMemoryAddressReadFromCallback = RegisterOnMemoryAddressReadFromCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback = RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnMemoryAddressWrittenToCallback = RegisterOnMemoryAddressWrittenToCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback = RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnWiiInputPolledCallback = RegisterOnWiiInputPolledCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnWiiInputPolledWithAutoDeregistrationCallback = RegisterOnWiiInputPolledWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RunButtonCallbacksInQueue = RunButtonCallbacksInQueue_impl;
  dll_defined_scriptContext_apis.RunGlobalScopeCode = RunGlobalScopeCode_impl;
  dll_defined_scriptContext_apis.RunOnFrameStartCallbacks = RunOnFrameStartCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnGCControllerPolledCallbacks = RunOnGCControllerPolledCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnInstructionReachedCallbacks = RunOnInstructionReachedCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnMemoryAddressReadFromCallbacks = RunOnMemoryAddressReadFromCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnMemoryAddressWrittenToCallbacks = RunOnMemoryAddressWrittenToCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnWiiInputPolledCallbacks = RunOnWiiInputPolledCallbacks_impl;
  dll_defined_scriptContext_apis.StartScript = StartScript_impl;
  dll_defined_scriptContext_apis.UnregisterOnFrameStartCallback = UnregisterOnFrameStartCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnGCControllerPolledCallback = UnregisterOnGCControllerPolledCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnInstructionReachedCallback = UnregisterOnInstructionReachedCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnMemoryAddressReadFromCallback = UnregisterOnMemoryAddressReadFromCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnMemoryAddressWrittenToCallback = UnregisterOnMemoryAddressWrittenToCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnWiiInputPolledCallback = UnregisterOnWiiInputPolledCallback_impl;

  ValidateApiStruct(&dll_defined_scriptContext_apis, sizeof(dll_defined_scriptContext_apis), "DolphinDefined_ScriptContext_APIs");

  initialized_dll_specific_script_context_apis = true;
}

DLL_Export void* CreateNewScript(int unique_script_identifier, const char* script_filename,
  Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE print_callback_fn_ptr,
  Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE script_end_callback_fn_ptr)
{
  if (!initialized_dll_specific_script_context_apis)
    InitDLLSpecificAPIs();

  void* opaque_script_context_handle = dolphinDefinedScriptContext_APIs.ScriptContext_Initializer(unique_script_identifier, script_filename, print_callback_fn_ptr, script_end_callback_fn_ptr, &dll_defined_scriptContext_apis);

  if (PythonDynamicLibrary::python_lib_ptr == nullptr)
  {
    PythonDynamicLibrary::InitPythonLib(opaque_script_context_handle);
    if (dolphinDefinedScriptContext_APIs.get_script_return_code(opaque_script_context_handle) != 0)
    {
      dolphinDefinedScriptContext_APIs.set_is_script_active(opaque_script_context_handle, 0);
      dolphinDefinedScriptContext_APIs.get_script_end_callback_function(opaque_script_context_handle)(opaque_script_context_handle, dolphinDefinedScriptContext_APIs.get_unique_script_identifier(opaque_script_context_handle));
      return opaque_script_context_handle;
    }
  }

  PythonScriptContext* new_python_script_context_ptr = reinterpret_cast<PythonScriptContext*>(Init_PythonScriptContext_impl(opaque_script_context_handle));
  return opaque_script_context_handle;
}
