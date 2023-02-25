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
ArgHolder BitwiseAnd(std::vector<ArgHolder>& args_list);
ArgHolder BitwiseOr(std::vector<ArgHolder>& args_list);
ArgHolder BitwiseNot(std::vector<ArgHolder>& args_list);
ArgHolder BitwiseXor(std::vector<ArgHolder>& args_list);
ArgHolder LogicalAnd(std::vector<ArgHolder>& args_list);
ArgHolder LogicalOr(std::vector<ArgHolder>& args_list);
ArgHolder LogicalXor(std::vector<ArgHolder>& args_list);
ArgHolder LogicalNot(std::vector<ArgHolder>& args_list);
ArgHolder BitShiftLeft(std::vector<ArgHolder>& args_list);
ArgHolder BitShiftRight(std::vector<ArgHolder>& args_list);

}  // namespace Scripting::BitApi
