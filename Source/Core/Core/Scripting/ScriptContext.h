#ifndef SCRIPT_CONTEXT
#define SCRIPT_CONTEXT
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include "Common/ThreadSafeQueue.h"
#include "Core/Scripting/HelperClasses/ScriptCallLocations.h"

namespace Scripting
{
extern std::string most_recent_script_version;
extern bool set_print_callback;
extern bool set_script_end_callback;
extern std::function<void(const std::string&)>* print_callback;
extern std::function<void(int)>* script_end_callback;

extern bool IsPrintCallbackSet();
extern bool IsScriptEndCallbackSet();
extern void SetPrintCallback(std::function<void(const std::string&)>* new_print_callback);
extern void SetScriptEndCallback(std::function<void(int)>* new_script_end_callback);
extern std::function<void(const std::string&)>* GetPrintCallback();
extern std::function<void(int)>* GetScriptEndCallback();

  class ScriptContext
  {
public:

  int unique_script_identifier;
  std::string script_filename;
  ScriptCallLocations current_script_call_location;
  std::vector<ScriptContext*>* pointer_to_vector_containing_all_scripts;
  bool is_script_active;
  bool finished_with_global_code;
  bool called_yielding_function_in_last_global_script_resume;
  bool called_yielding_function_in_last_frame_callback_script_resume;
  std::mutex script_specific_lock;

  ScriptContext(int new_unique_script_identifier, std::string new_script_filename,
                std::vector<ScriptContext*>* new_pointer_to_vector_containing_all_scripts,
                std::function<void(const std::string&)>* new_print_callback_for_ui,
                std::function<void(int)>* new_script_end_callback_for_ui)
  {
    unique_script_identifier = new_unique_script_identifier;
    script_filename = new_script_filename;
    current_script_call_location = ScriptCallLocations::FromScriptStartup;
    pointer_to_vector_containing_all_scripts = new_pointer_to_vector_containing_all_scripts;
    is_script_active = true;
    finished_with_global_code = false;
    called_yielding_function_in_last_global_script_resume = false;
    called_yielding_function_in_last_frame_callback_script_resume = false;
    SetPrintCallback(new_print_callback_for_ui);
    SetScriptEndCallback(new_script_end_callback_for_ui);
  }

  virtual ~ScriptContext() {}
  virtual void ImportModule(const std::string& api_name, const std::string& api_version) = 0;

  virtual void StartScript() = 0;
  virtual void RunGlobalScopeCode() = 0;
  virtual void RunOnFrameStartCallbacks() = 0;
  virtual void RunOnGCControllerPolledCallbacks() = 0;
  virtual void RunOnInstructionReachedCallbacks(size_t current_address) = 0;
  virtual void RunOnMemoryAddressReadFromCallbacks(size_t current_memory_address) = 0;
  virtual void RunOnMemoryAddressWrittenToCallbacks(size_t current_memory_address) = 0;
  virtual void RunOnWiiInputPolledCallbacks() = 0;

  virtual void* RegisterOnFrameStartCallbacks(void* callbacks) = 0;
  virtual void RegisterOnFrameStartWithAutoDeregistrationCallbacks(void* callbacks) = 0;
  virtual bool UnregisterOnFrameStartCallbacks(void* callbacks) = 0;

  virtual void* RegisterOnGCCControllerPolledCallbacks(void* callbacks) = 0;
  virtual void RegisterOnGCControllerPolledWithAutoDeregistrationCallbacks(void* callbacks) = 0;
  virtual bool UnregisterOnGCControllerPolledCallbacks(void* callbacks) = 0;

  virtual void* RegisterOnInstructionReachedCallbacks(size_t address, void* callbacks) = 0;
  virtual void RegisterOnInstructionReachedWithAutoDeregistrationCallbacks(size_t address, void* callbacks) = 0;
  virtual bool UnregisterOnInstructionReachedCallbacks(size_t address, void* callbacks) = 0;

  virtual void* RegisterOnMemoryAddressReadFromCallbacks(size_t memory_address, void* callbacks) = 0;
  virtual void RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallbacks(size_t memory_address, void* callbacks) = 0;
  virtual bool UnregisterOnMemoryAddressReadFromCallbacks(size_t memory_address, void* callbacks) = 0;

  virtual void* RegisterOnMemoryAddressWrittenToCallbacks(size_t memory_address, void* callbacks) = 0;
  virtual void RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallbacks(size_t memory_address, void* callbacks) = 0;
  virtual bool UnregisterOnMemoryAddressWrittenToCallbacks(size_t memory_address, void* callbacks) = 0;

  virtual void* RegisterOnWiiInputPolledCallbacks(void* callbacks) = 0;
  virtual void RegisterOnWiiInputPolledWithAutoDeregistrationCallbacks(void* callbacks) = 0;
  virtual bool UnregisterOnWiiInputPolledCallbacks(void* callbacks) = 0;

  virtual void RegisterButtonCallback(long long button_id, void* callbacks) = 0;
  virtual void AddButtonCallbackToQueue(void* callbacks) = 0;
  virtual bool IsButtonRegistered(long long button_id) = 0;
  virtual void GetButtonCallbackAndAddToQueue(long long button_id) = 0;
  virtual void RunButtonCallbacksInQueue() = 0;

  inline void ShutdownScript()
  {
    this->is_script_active = false;
    (*GetScriptEndCallback())(this->unique_script_identifier);
  }
  };

  extern ThreadSafeQueue<ScriptContext*> queue_of_scripts_waiting_to_start;
  extern void AddScriptToQueueOfScriptsWaitingToStart(ScriptContext*);
  extern ScriptContext* RemoveNextScriptToStartFromQueue();

  }  // namespace Scripting
#endif
