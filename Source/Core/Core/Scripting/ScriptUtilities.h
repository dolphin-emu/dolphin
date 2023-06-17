#ifndef SCRIPTING_UTILITIES
#define SCRIPTING_UTILITIES

#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include "Core/Scripting/LanguageDefinitions/DefinedScriptingLanguagesEnum.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

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
extern std::mutex graphics_callback_running_lock;

bool IsScriptingCoreInitialized();

void InitializeScript(int unique_script_identifier, const std::string& script_location,
                      std::function<void(const std::string&)>* new_print_callback,
                      std::function<void(int)>* new_script_end_callback,
                      DefinedScriptingLanguagesEnum language);

void StopScript(int unique_script_identifier);

bool StartScripts();
bool RunGlobalCode();
bool RunOnFrameStartCallbacks();
void RunOnGCInputPolledCallbacks();
void RunOnInstructionHitCallbacks(u32 instruction_address);
void RunOnMemoryAddressReadFromCallbacks(u32 memory_address);
void RunOnMemoryAddressWrittenToCallbacks(u32 memory_address, s64 new_value);
void RunOnWiiInputPolledCallbacks();
void RunButtonCallbacksInQueues();

}  // namespace Scripting::ScriptUtilities
#endif
