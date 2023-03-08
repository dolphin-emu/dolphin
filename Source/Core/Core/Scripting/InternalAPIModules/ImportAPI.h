#pragma once
#include <string>

#include "Core/Scripting/ScriptContext.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"

namespace Scripting::ImportAPI
{
extern const char* class_name;

ClassMetadata GetImportApiClassData(const std::string& api_version);

ArgHolder ImportCommon(ScriptContext* current_script, std::string api_name,
                        std::string version_number);

ArgHolder ImportModule(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder ImportAlt(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder Shutdown(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

}  // namespace Scripting::ImportAPI