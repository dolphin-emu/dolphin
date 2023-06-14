#include "Core/Scripting/ScriptUtilities.h"


#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ArgHolder_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ClassFunctionsResolver_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ClassMetadata_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/FunctionMetadata_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/GCButton_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/ScriptContext_APIs.h"
#include "Core/Scripting/CoreScriptContextFiles/InternalScriptAPIs/VectorOfArgHolders_APIs.h"

#include "Core/Scripting/EventCallbackRegistrationAPIs//OnInstructionHitCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressReadFromCallbackAPI.h"
#include "Core/Scripting/EventCallbackRegistrationAPIs/OnMemoryAddressWrittenToCallbackAPI.h"
#include "Core/Scripting/InternalAPIModules/GraphicsAPI.h"
#include "Core/Scripting/LanguageDefinitions/Lua/LuaScriptContext.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"
#include "Core/System.h"
#include "Core/Core.h"

namespace Scripting::ScriptUtilities
{

std::vector<ScriptContext*>* global_pointer_to_list_of_all_scripts = nullptr;
std::mutex initialization_and_destruction_lock;
std::mutex global_code_and_frame_callback_running_lock;
std::mutex gc_controller_polled_callback_running_lock;
std::mutex instruction_hit_callback_running_lock;
std::mutex memory_address_read_from_callback_running_lock;
std::mutex memory_address_written_to_callback_running_lock;
std::mutex wii_input_polled_callback_running_lock;
std::mutex graphics_callback_running_lock;
ThreadSafeQueue<ScriptContext*> queue_of_scripts_waiting_to_start = ThreadSafeQueue<ScriptContext*>();

static bool initialized_dolphin_api_structs = false;

static ArgHolder_APIs argHolder_apis_impl = {};
static ClassFunctionsResolver_APIs classFunctionsResolver_apis_impl = {};
static ClassMetadata_APIs classMetadata_apis_impl = {};
static FunctionMetadata_APIs functionMetadata_apis_impl = {};
static GCButton_APIs gcButton_apis_impl = {};
static Dolphin_Defined_ScriptContext_APIs dolphin_defined_scriptContext_apis_impl = {};
static VectorOfArgHolders_APIs vectorOfArgHolders_apis_impl = {};


bool IsScriptingCoreInitialized()
{
  return global_pointer_to_list_of_all_scripts != nullptr;
}

void InitializeScript(int unique_script_identifier, const std::string& script_filename,
                      std::function<void(const std::string&)>* new_print_callback,
                      std::function<void(int)>* new_script_end_callback,
                      DefinedScriptingLanguagesEnum language)
{
  Core::CPUThreadGuard lock(Core::System::GetInstance());
  initialization_and_destruction_lock.lock();
  global_code_and_frame_callback_running_lock.lock();
  if (global_pointer_to_list_of_all_scripts == nullptr)
  {
    global_pointer_to_list_of_all_scripts = new std::vector<ScriptContext*>();
  }
  /*
  if (language == DefinedScriptingLanguagesEnum::LUA)
  {
    global_pointer_to_list_of_all_scripts->push_back(new Scripting::Lua::LuaScriptContext(
        unique_script_identifier, script_filename, global_pointer_to_list_of_all_scripts,
        new_print_callback, new_script_end_callback));
  }
  else if (language == DefinedScriptingLanguagesEnum::PYTHON)
  {
    global_pointer_to_list_of_all_scripts->push_back(new Scripting::Python::PythonScriptContext(
        unique_script_identifier, script_filename, global_pointer_to_list_of_all_scripts,
        new_print_callback, new_script_end_callback));
  }
  */
  global_code_and_frame_callback_running_lock.unlock();
  initialization_and_destruction_lock.unlock();
}

void StopScript(int unique_script_identifier)
{
  Core::CPUThreadGuard lock(Core::System::GetInstance());
  initialization_and_destruction_lock.lock();
  global_code_and_frame_callback_running_lock.lock();
  gc_controller_polled_callback_running_lock.lock();
  instruction_hit_callback_running_lock.lock();
  memory_address_read_from_callback_running_lock.lock();
  memory_address_written_to_callback_running_lock.lock();
  wii_input_polled_callback_running_lock.lock();
  graphics_callback_running_lock.lock();

  ScriptContext* script_to_delete = nullptr;

  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
  (*global_pointer_to_list_of_all_scripts)[i]->script_specific_lock.lock();
    if ((*global_pointer_to_list_of_all_scripts)[i]->unique_script_identifier ==
        unique_script_identifier)
      script_to_delete = (*global_pointer_to_list_of_all_scripts)[i];
   (*global_pointer_to_list_of_all_scripts)[i]->script_specific_lock.unlock();
  }

  if (script_to_delete != nullptr)
  {
    (*global_pointer_to_list_of_all_scripts)
        .erase(std::find((*global_pointer_to_list_of_all_scripts).begin(),
                         (*global_pointer_to_list_of_all_scripts).end(), script_to_delete));
    delete script_to_delete;
  }

  graphics_callback_running_lock.unlock();
  wii_input_polled_callback_running_lock.unlock();
  memory_address_written_to_callback_running_lock.unlock();
  memory_address_read_from_callback_running_lock.unlock();
  instruction_hit_callback_running_lock.unlock();
  gc_controller_polled_callback_running_lock.unlock();
  global_code_and_frame_callback_running_lock.unlock();
  initialization_and_destruction_lock.unlock();
}

bool StartScripts()
{
  std::lock_guard<std::mutex> lock(initialization_and_destruction_lock);
  std::lock_guard<std::mutex> second_lock(global_code_and_frame_callback_running_lock);
  bool return_value = false;
  if (global_pointer_to_list_of_all_scripts == nullptr ||
      global_pointer_to_list_of_all_scripts->size() == 0 ||
      queue_of_scripts_waiting_to_start.IsEmpty())
    return false;
  while (!queue_of_scripts_waiting_to_start.IsEmpty())
  {
    ScriptContext* next_script = queue_of_scripts_waiting_to_start.pop();
    if (next_script == nullptr)
      break;
   next_script->script_specific_lock.lock();
    if (next_script->is_script_active)
    {
      next_script->current_script_call_location = ScriptCallLocations::FromScriptStartup;
      next_script->dll_specific_api_definitions.StartScript(next_script);
    }
    return_value = next_script->called_yielding_function_in_last_global_script_resume;
    next_script->script_specific_lock.unlock();
    if (return_value)
      break;
  }
  return return_value;
}

bool RunGlobalCode()
{
  std::lock_guard<std::mutex> lock(global_code_and_frame_callback_running_lock);
  bool return_value = false;
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return return_value;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active && !current_script->finished_with_global_code)
    {
      current_script->current_script_call_location = ScriptCallLocations::FromFrameStartGlobalScope;
      current_script->dll_specific_api_definitions.RunGlobalScopeCode(current_script);
    }
    return_value = current_script->called_yielding_function_in_last_global_script_resume;
    current_script->script_specific_lock.unlock();
    if (return_value)
      break;
  }
  return return_value;
}

bool RunOnFrameStartCallbacks()
{
  std::lock_guard<std::mutex> lock(global_code_and_frame_callback_running_lock);
  bool return_value = false;
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return return_value;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
   ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
   current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location = ScriptCallLocations::FromFrameStartCallback;
      current_script->dll_specific_api_definitions.RunOnFrameStartCallbacks(current_script);
    }
    return_value = current_script->called_yielding_function_in_last_frame_callback_script_resume;
    current_script->script_specific_lock.unlock();
    if (return_value)
      break;
  }
  return return_value;
}

void RunOnGCInputPolledCallbacks()
{
  std::lock_guard<std::mutex> lock(gc_controller_polled_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromGCControllerInputPolled;
      current_script->dll_specific_api_definitions.RunOnGCControllerPolledCallbacks(current_script);
    }
   current_script->script_specific_lock.unlock();
  }
}

void RunOnInstructionHitCallbacks(u32 instruction_address)
{
  std::lock_guard<std::mutex> lock(instruction_hit_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;

  OnInstructionHitCallbackAPI::instruction_address_for_current_callback = instruction_address;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromInstructionHitCallback;
      current_script->dll_specific_api_definitions.RunOnInstructionReachedCallbacks(current_script, instruction_address);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnMemoryAddressReadFromCallbacks(u32 memory_address)
{
  std::lock_guard<std::mutex> lock(memory_address_read_from_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;

  OnMemoryAddressReadFromCallbackAPI::memory_address_read_from_for_current_callback =
      memory_address;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromMemoryAddressReadFromCallback;
      current_script->dll_specific_api_definitions.RunOnMemoryAddressReadFromCallbacks(current_script, memory_address);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnMemoryAddressWrittenToCallbacks(u32 memory_address, s64 new_value)
{
  std::lock_guard<std::mutex> lock(memory_address_written_to_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;

  OnMemoryAddressWrittenToCallbackAPI::memory_address_written_to_for_current_callback =
      memory_address;
  OnMemoryAddressWrittenToCallbackAPI::value_written_to_memory_address_for_current_callback =
      new_value;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromMemoryAddressWrittenToCallback;
      current_script->dll_specific_api_definitions.RunOnMemoryAddressWrittenToCallbacks(current_script, memory_address);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnWiiInputPolledCallbacks()
{
  std::lock_guard<std::mutex> lock(wii_input_polled_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location = ScriptCallLocations::FromWiiInputPolled;
      current_script->dll_specific_api_definitions.RunOnWiiInputPolledCallbacks(current_script);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunButtonCallbacksInQueues()
{
  std::lock_guard<std::mutex> lock(graphics_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location = ScriptCallLocations::FromGraphicsCallback;
      current_script->dll_specific_api_definitions.RunButtonCallbacksInQueue(current_script);
    }
    current_script->script_specific_lock.unlock();
  }
}

void AddScriptToQueueOfScriptsWaitingToStart(ScriptContext* new_script)
{
  queue_of_scripts_waiting_to_start.push(new_script);
}

}  // namespace Scripting::ScriptUtilities
