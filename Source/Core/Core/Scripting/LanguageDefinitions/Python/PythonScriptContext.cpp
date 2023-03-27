#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"

#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/BitModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/EmuModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/ImportModuleImporter.h"
#include "Core/Scripting/InternalAPIModules/BitAPI.h"
#include "Core/Scripting/InternalAPIModules/EmuAPI.h"
#include "Core/Scripting/InternalAPIModules/ImportAPI.h"
#include <fstream>

namespace Scripting::Python
{
static const char* THIS_MODULE_NAME = "ThisPointerModule";
static bool python_initialized = false;
static PyThreadState* original_python_thread = nullptr;
static std::string error_buffer_str;


int getNumberOfCallbacksInMap(std::unordered_map<size_t, std::vector<PyObject*>>& input_map)
{
  int return_val = 0;
  for (auto& element : input_map)
  {
    return_val += (int)element.second.size();
  }
  return return_val;
}

bool PythonScriptContext::ShouldCallEndScriptFunction()
{
  if (finished_with_global_code &&
      (frame_callbacks.size() == 0 ||
       frame_callbacks.size() - number_of_frame_callbacks_to_auto_deregister <= 0) &&
      (gc_controller_input_polled_callbacks.size() == 0 ||
       gc_controller_input_polled_callbacks.size() -
               number_of_gc_controller_input_callbacks_to_auto_deregister <=
           0) &&
      (wii_controller_input_polled_callbacks.size() == 0 ||
       wii_controller_input_polled_callbacks.size() -
               number_of_wii_input_callbacks_to_auto_deregister <=
           0) &&
      (map_of_instruction_address_to_python_callbacks.size() == 0 ||
       getNumberOfCallbacksInMap(map_of_instruction_address_to_python_callbacks) -
               number_of_instruction_address_callbacks_to_auto_deregister <=
           0) &&
      (map_of_memory_address_read_from_to_python_callbacks.size() == 0 ||
       getNumberOfCallbacksInMap(map_of_memory_address_read_from_to_python_callbacks) -
               number_of_memory_address_read_callbacks_to_auto_deregister <=
           0) &&
      (map_of_memory_address_written_to_to_python_callbacks.size() == 0 ||
       getNumberOfCallbacksInMap(map_of_memory_address_written_to_to_python_callbacks) -
               number_of_memory_address_write_callbacks_to_auto_deregister <=
           0))
    return true;
  return false;
}


PyModuleDef ThisModuleDef = {
    PyModuleDef_HEAD_INIT, THIS_MODULE_NAME, /* name of module */
    nullptr,                                 /* module documentation, may be NULL */
    sizeof(long long*),                       /* size of per-interpreter state of the module,
                                                             or -1 if the module keeps state in global variables. */
    nullptr};


PyMODINIT_FUNC PyInit_ThisPointerModule()
{
  return PyModule_Create(&ThisModuleDef);
}


PythonScriptContext::PythonScriptContext(int new_unique_script_identifier, const std::string& new_script_filename,
                    std::vector<ScriptContext*>* new_pointer_to_list_of_all_scripts,
                    const std::string& api_version,
                    std::function<void(const std::string&)>* new_print_callback,
                    std::function<void(int)>* new_script_end_callback)
    : ScriptContext(new_unique_script_identifier, new_script_filename,
                    new_pointer_to_list_of_all_scripts, new_print_callback, new_script_end_callback)
{
  const std::lock_guard<std::mutex> lock(script_specific_lock);
  error_buffer_str = "";
  list_of_imported_modules = std::vector<PyObject*>();
  if (!python_initialized)
  {
    PyImport_AppendInittab(THIS_MODULE_NAME, PyInit_ThisPointerModule);
    PyImport_AppendInittab(ImportAPI::class_name, ImportModuleImporter::PyInit_ImportAPI);
    PyImport_AppendInittab(BitApi::class_name, BitModuleImporter::PyInit_BitAPI);
    PyImport_AppendInittab(EmuApi::class_name, EmuModuleImporter::PyInit_EmuAPI);
    Py_Initialize();
    original_python_thread = PyThreadState_Get();
    python_initialized = true;
  }
  else
  {
    PyEval_RestoreThread(original_python_thread);
  }
  main_python_thread = Py_NewInterpreter();
  long long this_address = (long long) (ScriptContext*) this;
  *((long long*)PyModule_GetState(PyImport_ImportModule(THIS_MODULE_NAME))) =  this_address;
  *((std::string*)PyModule_GetState(PyImport_ImportModule(ImportAPI::class_name))) = std::string("1.0");
  PyRun_SimpleString("import dolphin");
  *((std::string*)PyModule_GetState(PyImport_ImportModule(BitApi::class_name))) = std::string("0.0");
  *((std::string*)PyModule_GetState(PyImport_ImportModule(EmuApi::class_name))) = std::string("0.0");


  current_script_call_location = ScriptCallLocations::FromScriptStartup;

  std::string stdOutErr = "import sys\n\
class CatchOutErr:\n\
    def __init__(self):\n\
        self.value = ''\n\
    def write(self, txt):\n\
        self.value += txt\n\
catchOutErr = CatchOutErr()\n\
sys.stdout = catchOutErr\n\
sys.stderr = catchOutErr\n\
";                                        // this is python code to redirect stdouts/stderr
  PyRun_SimpleString(stdOutErr.c_str());  // invoke code to redirect
  PyEval_ReleaseThread(main_python_thread);
  AddScriptToQueueOfScriptsWaitingToStart(this);
}

PyObject* PythonScriptContext::RunFunction(PyObject* self, PyObject* args, std::string class_name, std::string function_name)
{
  if (class_name.empty())
  {
    return HandleError(nullptr, nullptr, false, "Class name was NULL!");
  }

  if (function_name.empty())
  {
    return HandleError(class_name.c_str(), nullptr, false, 
        "Function metadata in was NULL! This means that the functions in this class weren't properly initialized, which is probably an error in Dolphin's internal code");
   
  }

  ScriptContext* this_pointer = reinterpret_cast<ScriptContext*>(*((long long*)PyModule_GetState(PyImport_ImportModule(THIS_MODULE_NAME))));
  std::string version_number = *((std::string*)PyModule_GetState(PyImport_ImportModule(class_name.c_str())));

  if (version_number == "0.0")
  {
    return HandleError(
        class_name.c_str(), nullptr, false,
        "Attempted to call function of class without properly importing it. Please import "
        "the builtin classes for Dolphin like this: 'dolphin.importModule(\"EmuAPI\", \"1.0\")' "
        "(note that version number must be >= \"1.0\")");
  }
  FunctionMetadata functionMetadata = {};

  if (class_name == ImportAPI::class_name)
    functionMetadata = ImportAPI::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == BitApi::class_name)
    functionMetadata = BitApi::GetFunctionMetadataForVersion(version_number, function_name);
  else if (class_name == EmuApi::class_name)
    functionMetadata = EmuApi::GetFunctionMetadataForVersion(version_number, function_name);
  else
  {
    return HandleError(nullptr, nullptr, false,
                       fmt::format("Class name {} was undefined", class_name));
  }

  if (functionMetadata.function_pointer == nullptr)
  {
    return HandleError(class_name.c_str(), nullptr, false,
                       fmt::format("Function {} was undefined for class {} and version {}",
                                   function_name, class_name, version_number));
  }

  std::vector<ArgHolder> arguments_list = std::vector<ArgHolder>();

  if (PyTuple_GET_SIZE(args) != (int) functionMetadata.arguments_list.size())
  {
    return HandleError(class_name.c_str(), &functionMetadata, true, fmt::format("Expected {} arguments, but got {} arguments instead.", functionMetadata.arguments_list.size(), PyTuple_GET_SIZE(args)));
  }

  int argument_index = 0;
  for (auto argument_type : functionMetadata.arguments_list)
  {
    PyObject* current_item = PyTuple_GET_ITEM(args, argument_index);

    switch (argument_type)
    {
    case ArgTypeEnum::Boolean:
      arguments_list.push_back(CreateBoolArgHolder(static_cast<bool>(PyObject_IsTrue(current_item))));
      break;

    case ArgTypeEnum::U8:
      arguments_list.push_back(CreateU8ArgHolder(static_cast<u8>(PyLong_AsUnsignedLongLong(current_item))));
      break;
    case ArgTypeEnum::U16:
      arguments_list.push_back(
          CreateU16ArgHolder(static_cast<u16>(PyLong_AsUnsignedLongLong(current_item))));
      break;
    case ArgTypeEnum::U32:
      arguments_list.push_back(
          CreateU32ArgHolder(static_cast<u32>(PyLong_AsUnsignedLongLong(current_item))));
      break;
    case ArgTypeEnum::U64:
      arguments_list.push_back(CreateU64ArgHolder(static_cast<u64>(PyLong_AsUnsignedLongLong(current_item))));
      break;

    case ArgTypeEnum::S8:
      arguments_list.push_back(CreateS8ArgHolder(static_cast<s8>(PyLong_AsLongLong(current_item))));
      break;
    case ArgTypeEnum::S16:
      arguments_list.push_back(
          CreateS16ArgHolder(static_cast<s16>(PyLong_AsLongLong(current_item))));
      break;
    case ArgTypeEnum::Integer:
      arguments_list.push_back(
          CreateIntArgHolder(static_cast<int>(PyLong_AsLongLong(current_item))));
      break;
    case ArgTypeEnum::LongLong:
      arguments_list.push_back(CreateLongLongArgHolder(PyLong_AsLongLong(current_item)));
      break;

    case ArgTypeEnum::Float:
      arguments_list.push_back(CreateFloatArgHolder(static_cast<float>(PyFloat_AsDouble(current_item))));
      break;
    case ArgTypeEnum::Double:
      arguments_list.push_back(CreateDoubleArgHolder(PyFloat_AsDouble(current_item)));
      break;

    case ArgTypeEnum::String:
      arguments_list.push_back(CreateStringArgHolder(std::string(PyUnicode_AsUTF8(current_item))));
      break;

    default:
     return HandleError(class_name.c_str(), &functionMetadata, true, "Argument type not supported yet!");
    }
    ++argument_index;
  }

  ArgHolder return_value = (*functionMetadata.function_pointer)(this_pointer, arguments_list);

  switch (return_value.argument_type)
  {
  case ArgTypeEnum::VoidType:
    Py_INCREF(Py_None);
    return Py_None;

  case ArgTypeEnum::YieldType:
    return HandleError(
        nullptr, nullptr, false,
        "EmuAPI.frameAdvance() function is not supported for Python scripts. You can register a "
        "fucntion to run once at the start of each frame with: 'funcRef = "
        "OnFrameStart.register(funcName)'. You can later stop this function from running each "
        "frame by running 'OnFrameStart.unregister(funcRef)'");

  case ArgTypeEnum::Boolean:
    if (return_value.bool_val)
      Py_RETURN_TRUE;
    else
      Py_RETURN_FALSE;
    break;

  case ArgTypeEnum::U8:
    return Py_BuildValue("K", static_cast<unsigned long long>(return_value.u8_val));
  case ArgTypeEnum::U16:
    return Py_BuildValue("K", static_cast<unsigned long long>(return_value.u16_val));
  case ArgTypeEnum::U32:
    return Py_BuildValue("K", static_cast<unsigned long long>(return_value.u32_val));
  case ArgTypeEnum::U64:
    return Py_BuildValue("K", static_cast<unsigned long long>(return_value.u64_val));

  case ArgTypeEnum::S8:
    return Py_BuildValue("L", static_cast<long long>(return_value.s8_val));
  case ArgTypeEnum::S16:
    return Py_BuildValue("L", static_cast<long long>(return_value.s16_val));
  case ArgTypeEnum::Integer:
    return Py_BuildValue("L", static_cast<long long>(return_value.int_val));
  case ArgTypeEnum::LongLong:
    return Py_BuildValue("L", return_value.long_long_val);

  case ArgTypeEnum::Float:
    return Py_BuildValue("f", return_value.float_val);
  case ArgTypeEnum::Double:
    return Py_BuildValue("d", return_value.double_val);

  case ArgTypeEnum::String:
    return Py_BuildValue("s", return_value.string_val.c_str());

  case ArgTypeEnum::ErrorStringType:
    return HandleError(class_name.c_str(), &functionMetadata, true, return_value.error_string_val);

  default:
   return HandleError(class_name.c_str(), &functionMetadata, true, "Return Type not supported yet!");
  }
  return nullptr;
}

void PythonScriptContext::ImportModule(const std::string& api_name, const std::string& api_version)
{
  PyObject* current_module = PyImport_ImportModule(api_name.c_str());
  if (current_module == nullptr)
   return;

  *((std::string*)PyModule_GetState(current_module)) = api_version;

  PyRun_SimpleString(std::string("import " + api_name).c_str());
}

void PythonScriptContext::StartScript()
{
  PyEval_RestoreThread(main_python_thread);
  FILE* fp = fopen(script_filename.c_str(), "rb");
  PyRun_AnyFile(fp, nullptr);
  fclose(fp);
  this->finished_with_global_code = true;
  this->RunEndOfIteraionTasks();
  PyEval_ReleaseThread(main_python_thread);
}

void PythonScriptContext::RunGlobalScopeCode()
{
}

void PythonScriptContext::RunOnFrameStartCallbacks()
{
}

void PythonScriptContext::RunOnGCControllerPolledCallbacks()
{
}

void PythonScriptContext::RunOnInstructionReachedCallbacks(size_t current_address)
{
}

void PythonScriptContext::RunOnMemoryAddressReadFromCallbacks(size_t current_memory_address)
{
}

void PythonScriptContext::RunOnMemoryAddressWrittenToCallbacks(size_t current_memory_address)
{
}

void PythonScriptContext::RunOnWiiInputPolledCallbacks()
{
}

void* PythonScriptContext::RegisterOnFrameStartCallbacks(void* callbacks)
{
  return nullptr;
}

void PythonScriptContext::RegisterOnFrameStartWithAutoDeregistrationCallbacks(void* callbacks)
{
}

bool PythonScriptContext::UnregisterOnFrameStartCallbacks(void* callbacks)
{
  return false;
}

void* PythonScriptContext::RegisterOnGCCControllerPolledCallbacks(void* callbacks)
{
  return nullptr;
}

void PythonScriptContext::RegisterOnGCControllerPolledWithAutoDeregistrationCallbacks(
    void* callbacks)
{
}

bool PythonScriptContext::UnregisterOnGCControllerPolledCallbacks(void* callbacks)
{
  return false;
}

void* PythonScriptContext::RegisterOnInstructionReachedCallbacks(size_t address, void* callbacks)
{
  return nullptr;
}

void PythonScriptContext::RegisterOnInstructionReachedWithAutoDeregistrationCallbacks(
    size_t address, void* callbacks)
{
}

bool PythonScriptContext::UnregisterOnInstructionReachedCallbacks(size_t address, void* callbacks)
{
  return false;
}

void* PythonScriptContext::RegisterOnMemoryAddressReadFromCallbacks(size_t memory_address,
                                                                    void* callbacks)
{
  return nullptr;
}

void PythonScriptContext::RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallbacks(
    size_t memory_address, void* callbacks)
{
}

bool PythonScriptContext::UnregisterOnMemoryAddressReadFromCallbacks(size_t memory_address,
                                                                     void* callbacks)
{
  return false;
}

void* PythonScriptContext::RegisterOnMemoryAddressWrittenToCallbacks(size_t memory_address,
                                                                     void* callbacks)
{
  return nullptr;
}

void PythonScriptContext::RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallbacks(
    size_t memory_address, void* callbacks)
{
}

bool PythonScriptContext::UnregisterOnMemoryAddressWrittenToCallbacks(size_t memory_address,
                                                                      void* callbacks)
{
  return false;
}

void* PythonScriptContext::RegisterOnWiiInputPolledCallbacks(void* callbacks)
{
  return nullptr;
}

void PythonScriptContext::RegisterOnWiiInputPolledWithAutoDeregistrationCallbacks(void* callbacks)
{
}

bool PythonScriptContext::UnregisterOnWiiInputPolledCallbacks(void* callbacks)
{
  return false;
}

void PythonScriptContext::RegisterButtonCallback(long long button_id, void* callbacks)
{
}

void PythonScriptContext::AddButtonCallbackToQueue(void* callbacks)
{
}

bool PythonScriptContext::IsButtonRegistered(long long button_id)
{
  return false;
}

void PythonScriptContext::GetButtonCallbackAndAddToQueue(long long button_id)
{
}

void PythonScriptContext::RunButtonCallbacksInQueue()
{
}

PyObject* PythonScriptContext::HandleError(const char* class_name, const FunctionMetadata* function_metadata, bool include_example, const std::string& base_error_msg)
{
  std::string error_msg = "";
  if (class_name == nullptr)
  {
   if (function_metadata == nullptr)
      error_msg = fmt::format("Error: {}", base_error_msg);
   else
   {
      if (include_example)
        error_msg = fmt::format("Error: In {}(), {} The method should be called like this: {}",
                                function_metadata->function_name, base_error_msg,
                                function_metadata->example_function_call);
      else
        error_msg =
            fmt::format("Error: In {}(), {}", function_metadata->function_name, base_error_msg);
   }
  }
  else
  {
   if (function_metadata == nullptr)
      error_msg =
          fmt::format("Error: In function call in {} module, {}", class_name, base_error_msg);
   else
   {
      if (include_example)
        error_msg =
            fmt::format("Error: In {}.{}, {} The method should be called like this: {}.{}",
                        class_name, function_metadata->function_name, base_error_msg, class_name,
                        function_metadata->example_function_call);
      else
        error_msg = fmt::format("Error: In {}.{}, {}", class_name,
                                function_metadata->function_name, base_error_msg);
   }
  }

  PyErr_SetString(PyExc_RuntimeError, error_msg.c_str());
  return nullptr;
}

void PythonScriptContext::RunEndOfIteraionTasks()
{
  PyObject* pModule = PyImport_AddModule("__main__");
  PyObject* catcher = PyObject_GetAttrString(pModule, "catchOutErr");  // get our catchOutErr created above
  PyObject* output = PyObject_GetAttrString(catcher, "value");

  const char* output_msg = PyUnicode_AsUTF8(output);
  if (output_msg != nullptr && !std::string(output_msg).empty())
   (*GetPrintCallback())(output_msg);

  const char* error_msg = PyUnicode_AsUTF8(catcher);
  if (error_msg != nullptr && !std::string(error_msg).empty())
   (*GetPrintCallback())(error_msg);

  if (ShouldCallEndScriptFunction())
   ShutdownScript();
}


void PythonScriptContext::GenericRunCallbacksHelperFunction(
    PyObject* curr_state,
                                       std::vector<PyObject*>& vector_of_callbacks,
                                       int& index_of_next_callback_to_run,
                                       bool& yielded_on_last_callback_call, bool yields_are_allowed)
{

}

void* PythonScriptContext::RegisterForVectorHelper(std::vector<PyObject*>& input_vector,
                                                   void* callbacks)
{
  return nullptr;
}

void PythonScriptContext::RegisterForVectorWithAutoDeregistrationHelper(
    std::vector<PyObject*>& input_vector, void* callbacks,
    std::atomic<size_t>& number_of_auto_deregister_callbacks)
{
}
bool PythonScriptContext::UnregisterForVectorHelper(std::vector<PyObject*>& input_vector,
                                                    void* callbacks)
{
  return false;
}

void* PythonScriptContext::RegisterForMapHelper(
    size_t address, std::unordered_map<size_t, std::vector<PyObject*>>& input_map, void* callbacks)
{
  return nullptr;
}

void PythonScriptContext::RegisterForMapWithAutoDeregistrationHelper(
    size_t address, std::unordered_map<size_t, std::vector<PyObject*>>& input_map, void* callbacks,
    std::atomic<size_t>& number_of_auto_deregistration_callbacks)
{
}

bool PythonScriptContext::UnregisterForMapHelper(
    size_t address, std::unordered_map<size_t, std::vector<PyObject*>>& input_map, void* callbacks)
{
  return false;
}

}  // namespace Scripting::Python
