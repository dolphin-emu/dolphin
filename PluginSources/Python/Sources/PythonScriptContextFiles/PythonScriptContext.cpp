#include "PythonScriptContext.h"
#include "../HelperFiles/PythonInterface.h"

void* Init_PythonScriptContext_impl(void* new_base_script_context_ptr) {
  return nullptr;
} // Creates + returns a new PythonScriptContext object, with all fields properly initialized from the base ScriptContext* passed into the function
void Destroy_PythonScriptContext_impl(void* base_script_context_ptr) {} // Takes as input a ScriptContext*, and frees the memory associated with it.
int CustomPrintFunction() { return 0; }
bool ShouldCallEndScriptFunction() { return true; }


// See the declarations in the DLL_Defined_ScriptContext_APIs struct in the ScriptContext_APIs.h file for documentation on what these functions do.
// Note that each of these functions takes as input for its 1st parameter an opaque void* handle to a ScriptContext* (in order to be callable from the Dolphin side).
// However, you can use the get_derived_script_context_class_ptr() method on the ScriptContext* in order to get the PythonScriptContext* associated with the ScriptContext* object.

void ImportModule_impl(void* base_script_context_ptr, const char* api_name, const char* api_version)
{
  classFunctionsResolver_APIs.Send_ClassMetadataForVersion_To_DLL_Buffer(base_script_context_ptr, api_name, api_version);
  ClassMetadata* new_class_metadata = new ClassMetadata(((PythonScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_class_ptr(base_script_context_ptr))->class_metadata_buffer);
  CreatePythonModuleFromClassMetadata(new_class_metadata);
}

void StartScript_impl(void* x) {}

void RunGlobalScopeCode_impl(void* x) {}
void RunOnFrameStartCallbacks_impl(void* x) {}
void RunOnGCControllerPolledCallbacks_impl(void* x) {}
void RunOnWiiInputPolledCallbacks_impl(void* x) {}
void RunButtonCallbacksInQueue_impl(void* x) {}
void RunOnInstructionReachedCallbacks_impl(void* x, unsigned int y) {}
void RunOnMemoryAddressReadFromCallbacks_impl(void* s, unsigned int y) {}
void RunOnMemoryAddressWrittenToCallbacks_impl(void* x, unsigned int y) {}


void* RegisterOnFrameStartCallback_impl(void* x, void* y) {
  return nullptr;
}
void RegisterOnFrameStartWithAutoDeregistrationCallback_impl(void* x, void* y) {}
int UnregisterOnFrameStartCallback_impl(void* x, void* y) { return 3; }

void* RegisterOnGCControllerPolledCallback_impl(void* x, void* y) {
  return nullptr;
}
void RegisterOnGCControllerPolledWithAutoDeregistrationCallback_impl(void* x, void* y) {}
int UnregisterOnGCControllerPolledCallback_impl(void* x, void* y) { return 4; }

void* RegisterOnWiiInputPolledCallback_impl(void* x, void* y) { return nullptr; }
void RegisterOnWiiInputPolledWithAutoDeregistrationCallback_impl(void* x, void* y) {}
int UnregisterOnWiiInputPolledCallback_impl(void* x, void* y) { return 2; }

void RegisterButtonCallback_impl(void* x, long long y, void* z) {}
int IsButtonRegistered_impl(void* x, long long y) { return 2; }
void GetButtonCallbackAndAddToQueue_impl(void* x, long long y){}

void* RegisterOnInstructionReachedCallback_impl(void* x, unsigned int y, void* z) { return nullptr; }
void RegisterOnInstructioReachedWithAutoDeregistrationCallback_impl(void* x, unsigned int y, void* z) {}
int UnregisterOnInstructionReachedCallback_impl(void* x, unsigned int y, void* z) { return 2; }

void* RegisterOnMemoryAddressReadFromCallback_impl(void* x, unsigned int y, void* z) { return nullptr; }
void RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback_impl(void* x, unsigned int y, void* z){}
int UnregisterOnMemoryAddressReadFromCallback_impl(void* x, unsigned int y, void* z) { return 2; }

void* RegisterOnMemoryAddressWrittenToCallback_impl(void* x, unsigned int y, void* z) { return nullptr; }
void RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback_impl(void* x, unsigned int y, void* z) {}
int UnregisterOnMemoryAddressWrittenToCallback_impl(void* x, unsigned int y, void* z) { return 2; }

void DLLClassMetadataCopyHook_impl(void* x, void* y) {}
void DLLClassMetadataCopyHook_impl(void* base_script_context_ptr, void* class_metadata_ptr)
{
  std::string class_name = std::string(classMetadata_APIs.GetClassName(class_metadata_ptr));
  std::vector<FunctionMetadata> functions_list = std::vector<FunctionMetadata>();
  unsigned long long number_of_functions = classMetadata_APIs.GetNumberOfFunctions(class_metadata_ptr);
  for (unsigned long long i = 0; i < number_of_functions; ++i)
  {
    void* function_metadata_ptr = classMetadata_APIs.GetFunctionMetadataAtPosition(class_metadata_ptr, i);
    FunctionMetadata current_function = FunctionMetadata();
    std::string function_name = std::string(functionMetadata_APIs.GetFunctionName(function_metadata_ptr));
    std::string function_version = std::string(functionMetadata_APIs.GetFunctionVersion(function_metadata_ptr));
    std::string example_function_call = std::string(functionMetadata_APIs.GetExampleFunctionCall(function_metadata_ptr));
    void* (*wrapped_function_ptr)(void*, void*) = functionMetadata_APIs.GetFunctionPointer(function_metadata_ptr);
    ArgTypeEnum return_type = (ArgTypeEnum)functionMetadata_APIs.GetReturnType(function_metadata_ptr);
    unsigned long long num_args = functionMetadata_APIs.GetNumberOfArguments(function_metadata_ptr);
    std::vector<ArgTypeEnum> argument_type_list = std::vector<ArgTypeEnum>();
    for (unsigned long long arg_index = 0; arg_index < num_args; ++arg_index)
      argument_type_list.push_back((ArgTypeEnum)functionMetadata_APIs.GetTypeOfArgumentAtIndex(function_metadata_ptr, arg_index));
    current_function = FunctionMetadata(function_name.c_str(), function_version.c_str(), example_function_call.c_str(), wrapped_function_ptr, return_type, argument_type_list);
    functions_list.push_back(current_function);
  }

  ((PythonScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_class_ptr(base_script_context_ptr))->class_metadata_buffer = ClassMetadata(class_name, functions_list);
}
