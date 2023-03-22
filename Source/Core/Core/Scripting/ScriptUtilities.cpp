#include "Core/Scripting/ScriptUtilities.h"
#include "Core/Scripting/InternalAPIModules/GraphicsAPI.h"
#include "Core/Scripting/LanguageDefinitions/Lua/LuaScriptContext.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::ScriptUtilities {

std::vector<ScriptContext*>* global_pointer_to_list_of_all_scripts = nullptr;
std::mutex initialization_and_destruction_lock;
std::mutex global_code_and_frame_callback_running_lock;
std::mutex gc_controller_polled_callback_running_lock;
std::mutex instruction_hit_callback_running_lock;
std::mutex memory_address_read_from_callback_running_lock;
std::mutex memory_address_written_to_callback_running_lock;
std::mutex wii_input_polled_callback_running_lock;
std::mutex graphics_callback_running_lock;

const std::string newest_api_version = "1.0.0";

bool IsScriptingCoreInitialized()
{
  return global_pointer_to_list_of_all_scripts != nullptr;
}

void StartScript(int unique_script_identifier, const std::string& script_filename, 
                 std::function<void(const std::string&)>* new_print_callback,
                 std::function<void(int)>* new_script_end_callback,
                 DefinedScriptingLanguagesEnum language)
{
  initialization_and_destruction_lock.lock();
  global_code_and_frame_callback_running_lock.lock();
  if (global_pointer_to_list_of_all_scripts == nullptr)
    global_pointer_to_list_of_all_scripts = new std::vector<ScriptContext*>();

  if (language == DefinedScriptingLanguagesEnum::LUA)
  {
    global_pointer_to_list_of_all_scripts->push_back(new Scripting::Lua::LuaScriptContext(unique_script_identifier, script_filename, global_pointer_to_list_of_all_scripts, newest_api_version, new_print_callback, new_script_end_callback));
  }
  else if (language == DefinedScriptingLanguagesEnum::PYTHON)
  {
    global_pointer_to_list_of_all_scripts->push_back(new Scripting::Python::PythonScriptContext(
        unique_script_identifier, script_filename, global_pointer_to_list_of_all_scripts,
        newest_api_version, new_print_callback, new_script_end_callback));
  }
  global_code_and_frame_callback_running_lock.unlock();
  initialization_and_destruction_lock.unlock();
}

void StopScript(int unique_script_identifier)
{
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

bool RunGlobalCode()
{
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
      current_script->RunGlobalScopeCode();
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
      current_script->RunOnFrameStartCallbacks();
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
      current_script->current_script_call_location = ScriptCallLocations::FromGCControllerInputPolled;
      current_script->RunOnGCControllerPolledCallbacks();
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnInstructionHitCallbacks(size_t instruction_address)
{
  std::lock_guard<std::mutex> lock(instruction_hit_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromInstructionBreakpointCallback;
      current_script->RunOnInstructionReachedCallbacks(instruction_address);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnMemoryAddressReadFromCallbacks(size_t memory_address)
{
  std::lock_guard<std::mutex> lock(memory_address_read_from_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromMemoryAddressReadFromCallback;
      current_script->RunOnMemoryAddressReadFromCallbacks(memory_address);
    }
    current_script->script_specific_lock.unlock();
  }
}

void RunOnMemoryAddressWrittenToCallbacks(size_t memory_address)
{
  std::lock_guard<std::mutex> lock(memory_address_written_to_callback_running_lock);
  if (global_pointer_to_list_of_all_scripts == nullptr)
    return;
  for (size_t i = 0; i < global_pointer_to_list_of_all_scripts->size(); ++i)
  {
    ScriptContext* current_script = (*global_pointer_to_list_of_all_scripts)[i];
    current_script->script_specific_lock.lock();
    if (current_script->is_script_active)
    {
      current_script->current_script_call_location =
          ScriptCallLocations::FromMemoryAddressWrittenToCallback;
      current_script->RunOnMemoryAddressWrittenToCallbacks(memory_address);
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
      current_script->RunOnWiiInputPolledCallbacks();
    }
    current_script->script_specific_lock.unlock();
  }
}
}
