#ifndef PYTHON_SCRIPT_CONTEXT
#define PYTHON_SCRIPT_CONTEXT
#include <Python.h>
#include <functional>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <condition_variable>

#include "Core/Scripting/ScriptContext.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"

namespace Scripting::Python
{
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

  bool ShouldCallEndScriptFunction();


  PythonScriptContext(int new_unique_script_identifier, const std::string& new_script_filename,
                        std::vector<ScriptContext*>* new_pointer_to_list_of_all_scripts,
                        const std::string& api_version,
                        std::function<void(const std::string&)>* new_print_callback,
                        std::function<void(int)>* new_script_end_callback);


  static void StartMainScript(const char* script_name)
  {
    FILE* fp = fopen(script_name, "rb");
    PyRun_AnyFile(fp, nullptr);
    fclose(fp);
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
  static PyObject* HandleError(const std::string& error_msg);
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
};

}  // namespace Scripting::Python
#endif
