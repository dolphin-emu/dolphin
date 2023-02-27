#pragma once
#include <lua.hpp>
#include <string>
#include "Core/Lua/LuaHelperClasses/LuaColonCheck.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include <vector>

namespace Scripting::BitApi
{
extern const char* class_name;

ClassMetadata GetBitApiClassData(const std::string& api_version);
ArgHolder BitwiseAnd(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder BitwiseOr(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder BitwiseNot(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder BitwiseXor(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder LogicalAnd(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder LogicalOr(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder LogicalXor(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder LogicalNot(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder BitShiftLeft(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder BitShiftRight(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);

}  // namespace Scripting::BitApi
