#pragma once
#include <Python.h>
#include <functional>
#include <unordered_map>

#include "Core/Scripting/ScriptContext.h"

namespace Scripting::Python
{
extern bool python_initialized;

class PythonScriptContext : public ScriptContext
{
public:
  PyThreadState* main_python_thread;
  PyThreadState* frame_callback_python_thread;
  PyThreadState* instruction_address_hit_callback_python_thread;
  PyThreadState* memory_address_read_from_callback_python_thread;
  PyThreadState* memory_address_written_to_callback_python_thread;
  PyThreadState* gc_controller_input_polled_callback_python_thread;
  PyThreadState* wii_input_polled_callback_python_thread;
  PyThreadState* button_callback_thread;

  PyThreadState* current_thread;

  std::vector<PyObject*> frame_callbacks;
  std::vector<PyObject*> gc_controller_input_polled_callbacks;
  std::vector<PyObject*> wii_controller_input_polled_callbacks;

  int index_of_next_frame_callback_to_execute;


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
  PyInterpreterState* interpretter = PyInterpreterState_New();
  main_python_thread = PyThreadState_New(interpretter);
  frame_callback_python_thread = PyThreadState_New(interpretter);
  instruction_address_hit_callback_python_thread = PyThreadState_New(interpretter);
  memory_address_read_from_callback_python_thread = PyThreadState_New(interpretter);
  memory_address_written_to_callback_python_thread = PyThreadState_New(interpretter);
  gc_controller_input_polled_callback_python_thread = PyThreadState_New(interpretter);
  wii_input_polled_callback_python_thread = PyThreadState_New(interpretter);
  button_callback_thread = PyThreadState_New(interpretter);

  current_thread = main_python_thread;
  current_script_call_location = ScriptCallLocations::FromScriptStartup;

   
  
  }

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
