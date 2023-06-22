#pragma once
#include <string>

#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::OnGCControllerPolledCallbackAPI
{
extern const char* class_name;

extern std::array<bool, 4> overwrite_controller_at_specified_port;
extern std::array<Movie::ControllerState, 4> new_controller_inputs;
extern std::array<Movie::ControllerState, 4> controller_inputs_on_last_frame;
extern int current_controller_number_polled;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* Register(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* RegisterWithAutoDeregistration(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list);
ArgHolder* Unregister(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

ArgHolder* IsInGCControllerPolledCallback(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list);
ArgHolder* GetCurrentPortNumberOfPoll(ScriptContext* current_script,
                                      std::vector<ArgHolder*>* args_list);
ArgHolder* SetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetInputsForPoll(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::OnGCControllerPolledCallbackAPI
