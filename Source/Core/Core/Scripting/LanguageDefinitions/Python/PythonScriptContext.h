#ifndef PYTHON_SCRIPT_CONTEXT
#define PYTHON_SCRIPT_CONTEXT
#include <Python.h>
#include <functional>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <condition_variable>

#include "Core/Scripting/ScriptContext.h"

namespace Scripting::Python
{
extern bool python_initialized;
extern const char* THIS_VARIABLE_NAME;  // Making this something unlikely to overlap with a user-defined global.

class PythonScriptContext : public ScriptContext
{
public:
  PyThreadState* python_thread;

  std::vector<PyObject*> frame_callbacks;
  std::vector<PyObject*> gc_controller_input_polled_callbacks;
  std::vector<PyObject*> wii_controller_input_polled_callbacks;

  std::vector<PyObject*> list_of_imported_modules;

  std::unordered_map<size_t, std::vector<PyObject*>> map_of_instruction_address_to_python_callbacks;

  std::unordered_map<size_t, std::vector<PyObject*>>
      map_of_memory_address_read_from_to_python_callbacks;

  std::unordered_map<size_t, std::vector<PyObject*>>
      map_of_memory_address_written_to_to_python_callbacks;

  std::unordered_map<long long, PyObject*> map_of_button_id_to_callback;

  std::atomic<size_t> number_of_frame_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_gc_controller_input_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_wii_input_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_instruction_address_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_memory_address_read_callbacks_to_auto_deregister;
  std::atomic<size_t> number_of_memory_address_write_callbacks_to_auto_deregister;



  int getNumberOfCallbacksInMap(std::unordered_map<size_t, std::vector<PyObject*>>& input_map)
  {
    int return_val = 0;
    for (auto& element : input_map)
    {
      return_val += (int)element.second.size();
    }
    return return_val;
  }

  bool ShouldCallEndScriptFunction()
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

  const char* THIS_MODULE_NAME = "ThisPointerModule";

    PyModuleDef ThisModule = {
      PyModuleDef_HEAD_INIT, THIS_MODULE_NAME, /* name of module */
      nullptr,                                    /* module documentation, may be NULL */
      sizeof(long long),                          /* size of per-interpreter state of the module,
                                                                  or -1 if the module keeps state in global variables. */
      nullptr};

  PythonScriptContext(int new_unique_script_identifier, const std::string& new_script_filename,
                   std::vector<ScriptContext*>* new_pointer_to_list_of_all_scripts,
                   const std::string& api_version,
                   std::function<void(const std::string&)>* new_print_callback,
                   std::function<void(int)>* new_script_end_callback)
      : ScriptContext(new_unique_script_identifier, new_script_filename,
                      new_pointer_to_list_of_all_scripts)
  {
  const std::lock_guard<std::mutex> lock(script_specific_lock);
    if (!python_initialized)
    {
      Py_Initialize();
      python_initialized = true;
    }

  PyEval_AcquireLock();
  python_thread = Py_NewInterpreter();
  long long this_address = (long long)this;
  *((long long*)(PyModule_GetState(PyModule_Create(&ThisModule)))) = this_address;

  current_script_call_location = ScriptCallLocations::FromScriptStartup;
  this->ImportModule("dolphin", api_version);
  this->ImportModule("OnFrameStart", api_version);
  this->ImportModule("OnGCControllerPolled", api_version);
  this->ImportModule("OnInstructionHit", api_version);
  this->ImportModule("OnMemoryAddressReadFrom", api_version);
  this->ImportModule("OnMemoryAddressWrittenTo", api_version);
  this->ImportModule("OnWiiInputPolled", api_version);
  StartMainScript(new_script_filename.c_str());
  PyEval_ReleaseLock();
  }



  static void StartMainScript(const char* script_name)
  {
  PyRun_AnyFileExFlags(nullptr, script_name, true, nullptr);
  }

  static PyObject* RunFunction(PyObject* self, PyObject* args, std::string class_name,
                               FunctionMetadata* functionMetadata);

  virtual ~PythonScriptContext() {}
  virtual void ImportModule(const std::string& api_name, const std::string& api_version);
  virtual void RunGlobalScopeCode();
  virtual void RunOnFrameStartCallbacks();
  virtual void RunOnGCControllerPolledCallbacks();
  virtual void RunOnInstructionReachedCallbacks(size_t current_address);
  virtual void RunOnMemoryAddressReadFromCallbacks(size_t current_memory_address);
  virtual void RunOnMemoryAddressWrittenToCallbacks(size_t current_memory_address);
  virtual void RunOnWiiInputPolledCallbacks();

  virtual void* RegisterOnFrameStartCallbacks(void* callbacks);
  virtual void RegisterOnFrameStartWithAutoDeregistrationCallbacks(void* callbacks);
  virtual bool UnregisterOnFrameStartCallbacks(void* callbacks);

  virtual void* RegisterOnGCCControllerPolledCallbacks(void* callbacks);
  virtual void RegisterOnGCControllerPolledWithAutoDeregistrationCallbacks(void* callbacks);
  virtual bool UnregisterOnGCControllerPolledCallbacks(void* callbacks);

  virtual void* RegisterOnInstructionReachedCallbacks(size_t address, void* callbacks);
  virtual void RegisterOnInstructionReachedWithAutoDeregistrationCallbacks(size_t address,
                                                                           void* callbacks);
  virtual bool UnregisterOnInstructionReachedCallbacks(size_t address, void* callbacks);

  virtual void* RegisterOnMemoryAddressReadFromCallbacks(size_t memory_address, void* callbacks);
  virtual void RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallbacks(size_t memory_address,
                                                                              void* callbacks);
  virtual bool UnregisterOnMemoryAddressReadFromCallbacks(size_t memory_address, void* callbacks);

  virtual void* RegisterOnMemoryAddressWrittenToCallbacks(size_t memory_address, void* callbacks);
  virtual void
  RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallbacks(size_t memory_address,
                                                                  void* callbacks);
  virtual bool UnregisterOnMemoryAddressWrittenToCallbacks(size_t memory_address, void* callbacks);

  virtual void* RegisterOnWiiInputPolledCallbacks(void* callbacks);
  virtual void RegisterOnWiiInputPolledWithAutoDeregistrationCallbacks(void* callbacks);
  virtual bool UnregisterOnWiiInputPolledCallbacks(void* callbacks);

private:
  void GenericRunCallbacksHelperFunction(PyObject* curr_state,
                                         std::vector<PyObject*>& vector_of_callbacks,
                                         int& index_of_next_callback_to_run,
                                         bool& yielded_on_last_callback_call,
                                         bool yields_are_allowed);

  void* RegisterForVectorHelper(std::vector<PyObject*>& input_vector, void* callbacks);
  void RegisterForVectorWithAutoDeregistrationHelper(
      std::vector<PyObject*>& input_vector, void* callbacks,
      std::atomic<size_t>& number_of_auto_deregister_callbacks);
  bool UnregisterForVectorHelper(std::vector<PyObject*>& input_vector, void* callbacks);

  void* RegisterForMapHelper(size_t address,
                             std::unordered_map<size_t, std::vector<PyObject*>>& input_map,
                             void* callbacks);
  void RegisterForMapWithAutoDeregistrationHelper(
      size_t address, std::unordered_map<size_t, std::vector<PyObject*>>& input_map, void* callbacks,
      std::atomic<size_t>& number_of_auto_deregistration_callbacks);
  bool UnregisterForMapHelper(size_t address,
                              std::unordered_map<size_t, std::vector<PyObject*>>& input_map,
                              void* callbacks);
  virtual void AddButtonCallback(long long button_id, void* callbacks);
  virtual void RunButtonCallback(long long button_id);
  virtual bool IsCallbackDefinedForButtonId(long long button_id);

  virtual void ShutdownScript();
};

}  // namespace Scripting::Python
#endif
