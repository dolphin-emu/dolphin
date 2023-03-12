#pragma once
#include "Core/Scripting/ScriptContext.h"
#include <string>
#include <vector>
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
namespace Scripting::GraphicsAPI
{

extern const char* class_name;
ClassMetadata GetGraphicsApiClassData(const std::string& api_version);
ArgHolder DrawLine(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder DrawEmptyRectangle(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder DrawFilledRectangle(ScriptContext* current_script, std::vector<ArgHolder>& args_list);


}
