#include "Core/Scripting/ScriptUtilities.h"
#include "Core/Scripting/LanguageDefinitions/Lua/LuaScriptContext.h"

namespace Scripting::ScriptUtilities {

std::vector<ScriptContext*>* global_pointer_to_list_of_all_scripts = nullptr;
std::mutex initialization_and_destruction_lock;
std::mutex global_code_and_frame_callback_running_lock;
std::mutex gc_controller_polled_callback_running_lock;
std::mutex instruction_hit_callback_running_lock;
std::mutex memory_address_read_from_callback_running_lock;
std::mutex memory_address_written_to_callback_running_lock;
std::mutex wii_input_polled_callback_running_lock;

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

  wii_input_polled_callback_running_lock.unlock();
  memory_address_written_to_callback_running_lock.unlock();
  memory_address_read_from_callback_running_lock.unlock();
  instruction_hit_callback_running_lock.unlock();
  gc_controller_polled_callback_running_lock.unlock();
  global_code_and_frame_callback_running_lock.unlock();
  initialization_and_destruction_lock.unlock();
}

void RunOnFrameStartCallbacks()
{
}
void RunGlobalCode()
{
}

void RunOnGCInputPolledCallbacks()
{

}
}
