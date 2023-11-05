#pragma once

#include <mutex>
#include <string>
#include <vector>
#include "Common/CommonTypes.h"
#include "Core/Scripting/CoreScriptInterface/InternalScriptAPIs/ScriptContext_APIs.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

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

void InitializeScript(
    int unique_script_identifier, const std::string& script_location,
    Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE& new_print_callback,
    Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE& new_script_end_callback);

bool IsScriptingCoreInitialized();

bool StartScripts();
void StopScript(int unique_script_identifier);
bool RunGlobalCode();
bool RunOnFrameStartCallbacks();
void RunOnGCInputPolledCallbacks();
void RunOnInstructionHitCallbacks(u32 instruction_address);
void RunOnMemoryAddressReadFromCallbacks(u32 memory_address);
void RunOnMemoryAddressWrittenToCallbacks(u32 memory_address);
void RunOnWiiInputPolledCallbacks();
void RunButtonCallbacksInQueues();

}  // namespace Scripting::ScriptUtilities
