#pragma once
#include <string>
#include <vector>

#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ScriptCallLocations.h"

namespace Scripting::RegistersAPI
{

extern const char* class_name;

ClassMetadata GetRegistersApiClassData(const std::string& api_version);

ArgHolder GetU8FromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetU16FromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetU32FromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetU64FromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetS8FromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetS16FromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetS32FromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetS64FromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetFloatFromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetDoubleFromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetUnsignedBytesFromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder GetSignedBytesFromRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);

ArgHolder WriteU8ToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteU16ToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteU32ToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteU64ToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteS8ToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteS16ToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteS32ToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteS64ToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteFloatToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteDoubleToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);
ArgHolder WriteBytesToRegister(ScriptCallLocations call_location, std::vector<ArgHolder>& args_list);

}  // namespace Scripting::RegistersAPI

