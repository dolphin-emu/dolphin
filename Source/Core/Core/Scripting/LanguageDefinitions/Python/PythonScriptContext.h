#ifndef PYTHON_SCRIPT_CONTEXT
#define PYTHON_SCRIPT_CONTEXT
#include <Python.h>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "Core/Core.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/ScriptContext.h"

namespace Scripting::Python
{
extern const char*
    THIS_VARIABLE_NAME;  // Making this something unlikely to overlap with a user-defined global.

class PythonScriptContext : public ScriptContext
{
public:
  PyThreadState* main_python_thread;

  struct IdentifierToCallback
  {
    size_t identifier;
    PyObject* callback;
  };
  std::vector<IdentifierToCallback> frame_callbacks;
  std::vector<IdentifierToCallback> gc_controller_input_polled_callbacks;
  std::vector<IdentifierToCallback> wii_controller_input_polled_callbacks;

  std::unordered_map<size_t, std::vector<IdentifierToCallback>> map_of_instruction_address_to_python_callbacks;
  std::unordered_map<size_t, std::vector<IdentifierToCallback>>
      map_of_memory_address_read_from_to_python_callbacks;
  std::unordered_map<size_t, std::vector<IdentifierToCallback>>
      map_of_memory_address_written_to_to_python_callbacks;

  std::unordered_map<long long, IdentifierToCallback> map_of_button_id_to_callback;

  ThreadSafeQueue<IdentifierToCallback*> button_callbacks_to_run;


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

  bool ShouldCallEndScriptFunction();

  PythonScriptContext(int new_unique_script_identifier, const std::string& new_script_filename,
                      std::vector<ScriptContext*>* new_pointer_to_list_of_all_scripts,
                      std::function<void(const std::string&)>* new_print_callback,
                      std::function<void(int)>* new_script_end_callback);

  static PyObject* RunFunction(PyObject* self, PyObject* args, std::string class_name, std::string function_name);

  virtual ~PythonScriptContext() {}
  virtual void ImportModule(const std::string& api_name, const std::string& api_version);
  virtual void StartScript();
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

  virtual void RegisterButtonCallback(long long button_id, void* callbacks);
  virtual bool IsButtonRegistered(long long button_id);
  virtual void GetButtonCallbackAndAddToQueue(long long button_id);
  virtual void RunButtonCallbacksInQueue();

  static PyObject* HandleError(const char* class_name, const FunctionMetadata* function_metadata,
                               bool include_example, const std::string& base_error_msg);

private:
  void RunEndOfIteraionTasks();
  void RunCallbacksForVector(std::vector<IdentifierToCallback>& callback_list);
  void RunCallbacksForMap(std::unordered_map<size_t, std::vector<IdentifierToCallback>>& map_of_callbacks,
                         size_t current_address);
  void* RegisterForVectorHelper(std::vector<IdentifierToCallback>& callback_list, void* callback);
  void RegisterForVectorWithAutoDeregistrationHelper(
      std::vector<IdentifierToCallback>& callback_list, void* callback,
      std::atomic<size_t>& number_of_callbacks_to_auto_deregister);
  bool UnregisterForVectorHelper(std::vector<IdentifierToCallback>& callback_list, void* callback);



void* RegisterForMapHelper(
      size_t address, std::unordered_map<size_t, std::vector<IdentifierToCallback>>& map_of_callbacks,
      void* callbacks);
  void RegisterForMapWithAutoDeregistrationHelper(
      size_t address, std::unordered_map<size_t, std::vector<IdentifierToCallback>>& map_of_callbacks,
      void* callbacks, std::atomic<size_t>& number_of_auto_deregistration_callbacks);

  bool UnregisterForMapHelper(size_t address,
                              std::unordered_map<size_t, std::vector<IdentifierToCallback>>& map_of_callbacks,
                              void* callbacks);
};

}  // namespace Scripting::Python
#endif
