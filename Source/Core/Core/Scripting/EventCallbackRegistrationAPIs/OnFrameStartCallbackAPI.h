#pragma once
#include <string>

#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/ScriptContext.h"

namespace Scripting::OnFrameStartCallbackAPI
{
extern const char* class_name;

ClassMetadata GetOnFrameStartCallbackApiClassData(const std::string& api_version);

ArgHolder Register(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder RegisterWithAutoDeregistration(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder Unregister(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

}