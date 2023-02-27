#pragma once
#include <lua.hpp>

#include <string>

#include "Common/CommonTypes.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include <vector>

namespace Scripting::MemoryApi
{
extern const char* class_name;

ClassMetadata GetMemoryApiClassData(const std::string& api_version);

ArgHolder ReadU8(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadU16(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadU32(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadU64(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadS8(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadS16(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadS32(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadS64(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadFloat(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadDouble(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadFixedLengthString(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadNullTerminatedString(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadUnsignedBytes(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder ReadSignedBytes(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);

ArgHolder WriteU8(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteU16(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteU32(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteU64(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteS8(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteS16(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteS32(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteS64(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteFloat(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteDouble(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteString(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteBytes(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);

}  // namespace Scripting::MemoryApi

