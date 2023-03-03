#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include "Core/Scripting/LanguageDefinitions/DefinedScriptingLanguagesEnum.h"
#include "Core/Scripting/ScriptContext.h"
namespace Scripting::ScriptUtilities
{

extern std::vector<ScriptContext*>* global_pointer_to_list_of_all_scripts;
extern std::mutex initialization_and_destruction_lock;
extern std::mutex global_code_and_frame_callback_running_lock;
extern std::mutex gc_controller_polled_callback_running_lock;
extern std::mutex instruction_hit_callback_running_lock;
extern std::mutex memory_address_read_from_callback_running_lock;
extern std::mutex memory_address_written_to_callback_running_lock;
extern std::mutex wii_input_polled_callback_running_lock;

bool IsScriptingCoreInitialized();

void StartScript(int unique_script_identifier, const std::string& script_location, 
                 std::function<void(const std::string&)>* new_print_callback,
                 std::function<void(int)>* new_script_end_callback,
                 DefinedScriptingLanguagesEnum language);

void StopScript(int unique_script_identifier);

void RunOnFrameStartCallbacks();
void RunGlobalCode();
void RunOnGCInputPolledCallbacks();

}  // namespace Scripting::ScriptUtilities
