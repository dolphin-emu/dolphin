#ifndef SCRIPTING_UTILITIES
#define SCRIPTING_UTILITIES

#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include "Common/CommonTypes.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"
#include "Core/Scripting/HelperClasses/ScriptQueueEventTypes.h"

namespace Scripting::ScriptUtilities
{

extern std::vector<ScriptContext*> list_of_all_scripts;
extern std::mutex initialization_and_destruction_lock;
extern std::mutex global_code_and_frame_callback_running_lock;
extern std::mutex gc_controller_polled_callback_running_lock;
extern std::mutex instruction_hit_callback_running_lock;
extern std::mutex memory_address_read_from_callback_running_lock;
extern std::mutex memory_address_written_to_callback_running_lock;
extern std::mutex wii_input_polled_callback_running_lock;
extern std::mutex graphics_callback_running_lock;

bool IsScriptingCoreInitialized();

void InitializeScript(
    int unique_script_identifier, const std::string& script_location,
    Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE new_print_callback,
    Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE new_script_end_callback);

void SetIsScriptActiveToFalse(void* base_script_context_ptr);  // Only used by Main.cpp

void StopScript(int unique_script_identifier);

void PushScriptCreateQueueEvent(
    const int new_unique_script_identifier, const char* new_script_filename,
    const Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE& new_print_callback_func,
    const Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE&
        new_script_end_callback_func);

void PushScriptStopQueueEvent(const ScriptQueueEventTypes event_type, const int script_identifier);

void ProcessScriptQueueEvents();  // This function executes
                                  // the events in the queue
                                  // of script events in the
                                  // order that they were
                                  // triggered.
// This includes the events to start and stop scripts from the UI, as well as the
// script-end-callback function being called from the script (which is another event)

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
