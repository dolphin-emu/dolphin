#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"

#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/BitModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/ImportModuleImporter.h"
#include "Core/Scripting/InternalAPIModules/BitAPI.h"
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
    sizeof(PyObject*),                       /* size of per-interpreter state of the module,
                                                             or -1 if the module keeps state in global variables. */
    nullptr};

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
    Py_Initialize();
    original_python_thread = PyThreadState_Get();
    python_initialized = true;
  }
  else
  {
    PyEval_RestoreThread(original_python_thread);
  }
  python_thread = Py_NewInterpreter();
  long long this_address = (long long) (ScriptContext*) this;
  PyObject* this_module = PyModule_Create(&ThisModuleDef);
  *((long long*)PyModule_GetState(this_module)) = this_address;
  PyDict_SetItemString(PyImport_GetModuleDict(), THIS_MODULE_NAME, this_module);
  list_of_imported_modules.push_back(this_module);

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
";  // this is python code to redirect stdouts/stderr
  PyRun_SimpleString(stdOutErr.c_str());  // invoke code to redirect
  this->ImportModule("dolphin", api_version);
  this->ImportModule("OnFrameStart", api_version);
  this->ImportModule("OnGCControllerPolled", api_version);
  this->ImportModule("OnInstructionHit", api_version);
  this->ImportModule("OnMemoryAddressReadFrom", api_version);
  this->ImportModule("OnMemoryAddressWrittenTo", api_version);
  this->ImportModule("OnWiiInputPolled", api_version);
  PyObject* pModule = PyImport_AddModule("__main__");
  StartMainScript(new_script_filename.c_str());
  finished_with_global_code = true;

  PyObject* catcher = PyObject_GetAttrString(pModule, "catchOutErr");  // get our catchOutErr created above
  PyObject* output = PyObject_GetAttrString(catcher, "value");
  const char* output_msg = PyUnicode_AsUTF8(output);
  if (output_msg != nullptr && !std::string(output_msg).empty())
    (*GetPrintCallback())(output_msg);

  const char* error_msg = PyUnicode_AsUTF8(catcher);
  if (error_msg != nullptr && !std::string(error_msg).empty())
    (*GetPrintCallback())(error_msg);

  if (!error_buffer_str.empty())
  {
    (*GetPrintCallback())(error_buffer_str);
    error_buffer_str = "";
  }

  if (ShouldCallEndScriptFunction())
    ShutdownScript();
  PyEval_ReleaseThread(python_thread);
}

PyObject* PythonScriptContext::RunFunction(PyObject* self, PyObject* args, std::string class_name, FunctionMetadata* functionMetadata)
{
  if (functionMetadata == nullptr)
  {
    return HandleError(
        fmt::format("Error: function metadata in {} class was NULL! This means that the functions "
                    "in this class weren't properly initialized, which is probably an error in "
                    "Dolphin's internal code.",
                    class_name));
   
  }
  ScriptContext* this_pointer = reinterpret_cast<ScriptContext*>(*((long long*)PyModule_GetState(PyImport_ImportModule(THIS_MODULE_NAME))));

  std::vector<ArgHolder> arguments_list = std::vector<ArgHolder>();

  if (PyTuple_GET_SIZE(args) != (int) functionMetadata->arguments_list.size())
  {
    return HandleError(fmt::format("Error: In {}.{}() function, expected {} arguments, but got {} "
                                  "arguments instead. The method should be called like this: {}.{}",
                                  class_name, functionMetadata->function_name,
                                  functionMetadata->arguments_list.size(), PyTuple_GET_SIZE(args),
                                  class_name, functionMetadata->example_function_call));
  }

  int argument_index = 0;
  for (auto argument_type : functionMetadata->arguments_list)
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
     return HandleError("Argument type not supported yet!");
    }
    ++argument_index;
  }

  ArgHolder return_value = (*functionMetadata->function_pointer)(this_pointer, arguments_list);

  switch (return_value.argument_type)
  {
  case ArgTypeEnum::VoidType:
    Py_INCREF(Py_None);
    return Py_None;

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

  default:
   return HandleError("Return Type not supported yet!");
  }
  return nullptr;
}

void PythonScriptContext::ImportModule(const std::string& api_name, const std::string& api_version)
{
  PyObject* new_module = nullptr;
  if (api_name == BitApi::class_name)
    new_module = BitModuleImporter::ImportBitModule(api_version);
  else if (api_name == ImportAPI::class_name)
    new_module = ImportModuleImporter::ImportImportModule(api_version);
  else
    return;
  list_of_imported_modules.push_back(new_module);
  PyDict_SetItemString(PyImport_GetModuleDict(), api_name.c_str(), new_module);
  PyRun_SimpleString(std::string("import " + api_name).c_str());
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

PyObject* PythonScriptContext::HandleError(const std::string& error_msg)
{
  PyErr_SetString(PyExc_RuntimeError, error_msg.c_str());
  error_buffer_str = error_msg;
  return nullptr;
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

void PythonScriptContext::AddButtonCallback(long long button_id, void* callbacks)
{
}

void PythonScriptContext::RunButtonCallback(long long button_id)
{
}

bool PythonScriptContext::IsCallbackDefinedForButtonId(long long button_id)
{
  return false;
}

void PythonScriptContext::ShutdownScript()
{
  this->is_script_active = false;
  (*GetScriptEndCallback())(this->unique_script_identifier);
  return;
}

}  // namespace Scripting::Python
