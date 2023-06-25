#include "PythonScriptContext.h"
#include "../HelperFiles/PythonInterface.h"
#include <vector>


void* original_python_thread = nullptr;

// Creates + returns a new PythonScriptContext object, with all fields properly initialized from the base ScriptContext* passed into the function.
void* Init_PythonScriptContext_impl(void* new_base_script_context_ptr)
{
  PythonScriptContext* new_python_script = new PythonScriptContext(new_base_script_context_ptr);
  if (original_python_thread == nullptr) // This means it's our first time initializing the Python runtime.
  {
    PythonInterface::CreateThisModule();

    const void* default_module_names = moduleLists_APIs.GetListOfDefaultModules();
    unsigned long long number_of_default_modules = moduleLists_APIs.GetSizeOfList(default_module_names);
    for (unsigned long long i = 0; i < number_of_default_modules; ++i)
      PythonInterface::CreateEmptyModule(moduleLists_APIs.GetElementAtListIndex(default_module_names, i));

    const void* non_default_module_names = moduleLists_APIs.GetListOfNonDefaultModules();
    unsigned long long number_of_non_default_modules = moduleLists_APIs.GetSizeOfList(non_default_module_names);
    for (unsigned long long i = 0; i < number_of_non_default_modules; ++i)
      PythonInterface::CreateEmptyModule(moduleLists_APIs.GetElementAtListIndex(non_default_module_names, i));

    PythonInterface::Python_Initialize();
    original_python_thread = PythonInterface::PythonThreadState_Get();
  }
  else
    PythonInterface::PythonEval_RestoreThread(original_python_thread);

  new_python_script->main_python_thread = PythonInterface::Python_NewInterpreter();
  PythonInterface::SetThisModule(new_python_script);

  const void* default_module_names = moduleLists_APIs.GetListOfDefaultModules();
  unsigned long long number_of_default_modules = moduleLists_APIs.GetSizeOfList(default_module_names);
  const char* script_version = dolphinDefinedScriptContext_APIs.get_script_version();
  for (unsigned long long i = 0; i < number_of_default_modules; ++i)
  {
    PythonInterface::SetModuleVersion(moduleLists_APIs.GetElementAtListIndex(default_module_names, i), script_version);
    classFunctionsResolver_APIs.Send_ClassMetadataForVersion_To_DLL_Buffer(new_python_script->base_script_context_ptr, moduleLists_APIs.GetElementAtListIndex(default_module_names, i), script_version);
    ClassMetadata* class_metadata_copy = new ClassMetadata(new_python_script->class_metadata_buffer);
    new_python_script->classes_to_delete.push_back(class_metadata_copy);
    PythonInterface::SetModuleFunctions(moduleLists_APIs.GetElementAtListIndex(default_module_names, i), class_metadata_copy, &new_python_script->py_method_defs_to_delete);
    PythonInterface::RunImportCommand(moduleLists_APIs.GetElementAtListIndex(default_module_names, i));
  }

  const void* non_default_module_names = moduleLists_APIs.GetListOfNonDefaultModules();
  unsigned long long number_of_non_default_modules = moduleLists_APIs.GetSizeOfList(non_default_module_names);
  for (unsigned long long i = 0; i < number_of_non_default_modules; ++i)
    PythonInterface::SetModuleVersion(moduleLists_APIs.GetElementAtListIndex(non_default_module_names, i), "0.0");


  std::string user_path = std::string(fileUtility_APIs.GetUserPath()) + "PythonLibs";
  std::replace(user_path.begin(), user_path.end(), '\\', '/');

  std::string system_path = std::string(fileUtility_APIs.GetSystemDirectory()) + "PythonLibs";
  std::replace(system_path.begin(), system_path.end(), '\\', '/');

  PythonInterface::AppendArgumentsToPath(user_path.c_str(), system_path.c_str());
  PythonInterface::RedirectStdOutAndStdErr();

  PythonInterface::PythonEval_ReleaseThread(new_python_script->main_python_thread);
  dolphinDefinedScriptContext_APIs.add_script_to_queue_of_scripts_waiting_to_start(new_python_script->base_script_context_ptr);
  return reinterpret_cast<void*>(new_python_script);
}

PythonScriptContext* GetPythonScriptContext(void* base_script_context_ptr)
{
  return reinterpret_cast<PythonScriptContext*>(dolphinDefinedScriptContext_APIs.get_derived_script_context_class_ptr(base_script_context_ptr));
}

void Destroy_PythonScriptContext_impl(void* base_script_context_ptr) // Takes as input a ScriptContext*, and frees the associated PythonScriptContext*
{
  PythonScriptContext* current_script = GetPythonScriptContext(base_script_context_ptr);
  unsigned long long num_classes = current_script->classes_to_delete.size();
  for (unsigned long long i = 0; i < num_classes; ++i)
    delete ((ClassMetadata*)current_script->classes_to_delete[i]);
  current_script->classes_to_delete.clear();
  PythonInterface::DeleteMethodDefsVector(&(current_script->py_method_defs_to_delete));
  delete current_script;
}

void RunEndOfIteraionTasks(void* base_script_context_ptr)
{

}

int getNumberOfCallbacksInMap(std::unordered_map<unsigned int, std::vector<PythonScriptContext::IdentifierToCallback>>& input_map)
{
  int return_val = 0;
  for (auto& element : input_map)
  {
    return_val += (int)element.second.size();
  }
  return return_val;
}

bool ShouldCallEndScriptFunction(void* base_script_context_ptr)
{
  PythonScriptContext* current_script = GetPythonScriptContext(base_script_context_ptr);

  if (dolphinDefinedScriptContext_APIs.get_is_finished_with_global_code(base_script_context_ptr) &&
    (current_script->frame_callbacks.size() == 0 || (current_script->frame_callbacks.size() - current_script->number_of_frame_callbacks_to_auto_deregister <= 0)) &&
    (current_script->gc_controller_input_polled_callbacks.size() == 0 || (current_script->gc_controller_input_polled_callbacks.size() - current_script->number_of_gc_controller_input_callbacks_to_auto_deregister <= 0)) &&
    (current_script->wii_controller_input_polled_callbacks.size() == 0 || (current_script->wii_controller_input_polled_callbacks.size() - current_script->number_of_wii_input_callbacks_to_auto_deregister <=0)) &&
    (current_script->map_of_instruction_address_to_python_callbacks.size() == 0 || (getNumberOfCallbacksInMap(current_script->map_of_instruction_address_to_python_callbacks) - current_script->number_of_instruction_address_callbacks_to_auto_deregister <= 0)) &&
    (current_script->map_of_memory_address_read_from_to_python_callbacks.size() == 0 || (getNumberOfCallbacksInMap(current_script->map_of_memory_address_read_from_to_python_callbacks) - current_script->number_of_memory_address_read_callbacks_to_auto_deregister <= 0)) &&
    (current_script->map_of_memory_address_written_to_to_python_callbacks.size() == 0 || (getNumberOfCallbacksInMap(current_script->map_of_memory_address_written_to_to_python_callbacks) - current_script->number_of_memory_address_write_callbacks_to_auto_deregister <= 0)) &&
     current_script->button_callbacks_to_run.empty())
    return true;
  return false;
}


// See the declarations in the DLL_Defined_ScriptContext_APIs struct in the ScriptContext_APIs.h file for documentation on what these functions do.
// Note that each of these functions takes as input for its 1st parameter an opaque void* handle to a ScriptContext* (in order to be callable from the Dolphin side).
// However, you can use the get_derived_script_context_class_ptr() method on the ScriptContext* in order to get the PythonScriptContext* associated with the ScriptContext* object.

// This function is called when a class is imported. It sets the version of the class module, sets the class module to have the right functions in it, and then runs the "import module" command.
void ImportModule_impl(void* base_script_context_ptr, const char* api_name, const char* api_version)
{
  PythonInterface::SetModuleVersion(api_name, api_version);
  classFunctionsResolver_APIs.Send_ClassMetadataForVersion_To_DLL_Buffer(base_script_context_ptr, api_name, api_version);
  ClassMetadata* new_class_metadata = new ClassMetadata(((PythonScriptContext*)dolphinDefinedScriptContext_APIs.get_derived_script_context_class_ptr(base_script_context_ptr))->class_metadata_buffer);
  GetPythonScriptContext(base_script_context_ptr)->classes_to_delete.push_back(new_class_metadata); // storing the pointer so it can be deleted later on when the Python DLL shuts down.
  PythonInterface::SetModuleFunctions(api_name, new_class_metadata, &(GetPythonScriptContext(base_script_context_ptr)->py_method_defs_to_delete));
  PythonInterface::RunImportCommand(api_name);
}

void* RunFunction_impl(FunctionMetadata* current_function_metadata, void* self_raw, void* args_raw)
{
  return self_raw;
}

void StartScript_impl(void* base_script_context_ptr)
{
  if (!dolphinDefinedScriptContext_APIs.get_is_script_active(base_script_context_ptr))
    return;
  PythonScriptContext* python_script = GetPythonScriptContext(base_script_context_ptr);
  PythonInterface::PythonEval_RestoreThread(python_script->main_python_thread);
  PythonInterface::Python_RunFile(dolphinDefinedScriptContext_APIs.get_script_filename(base_script_context_ptr));
  dolphinDefinedScriptContext_APIs.set_is_finished_with_global_code(base_script_context_ptr, 1); // Since Python can only run global code when it first starts a script, we always set this to true after we finish executing the file for the 1st time.
  RunEndOfIteraionTasks(base_script_context_ptr);
  PythonInterface::PythonEval_ReleaseThread(python_script->main_python_thread);
}


void RunGlobalScopeCode_impl(void* base_script_context_ptr)
{
  return; // Python can only run global code when it starts a script for the 1st time (in StartScript_impl()). Thus, this function does nothing.
}

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
void GetButtonCallbackAndAddToQueue_impl(void* x, long long y) {}

void* RegisterOnInstructionReachedCallback_impl(void* x, unsigned int y, void* z) { return nullptr; }
void RegisterOnInstructioReachedWithAutoDeregistrationCallback_impl(void* x, unsigned int y, void* z) {}
int UnregisterOnInstructionReachedCallback_impl(void* x, unsigned int y, void* z) { return 2; }

void* RegisterOnMemoryAddressReadFromCallback_impl(void* x, unsigned int y, void* z) { return nullptr; }
void RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback_impl(void* x, unsigned int y, void* z) {}
int UnregisterOnMemoryAddressReadFromCallback_impl(void* x, unsigned int y, void* z) { return 2; }

void* RegisterOnMemoryAddressWrittenToCallback_impl(void* x, unsigned int y, void* z) { return nullptr; }
void RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback_impl(void* x, unsigned int y, void* z) {}
int UnregisterOnMemoryAddressWrittenToCallback_impl(void* x, unsigned int y, void* z) { return 2; }

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

void DLLFunctionMetadataCopyHook_impl(void* x, void* y) {}
