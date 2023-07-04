#include "Core/Scripting/InternalAPIModules/StatisticsAPI.h"

#include <fmt/format.h>
#include <memory>
#include "Core/HW/Memmap.h"
#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"
#include "Core/System.h"

namespace Scripting::StatisticsApi
{
const char* class_name = "StatisticsAPI";

static std::array all_statistics_functions_metadata_list = {
    FunctionMetadata("isRecordingInput", "1.0", "isRecordingInput()", IsRecordingInput,
                     ScriptingEnums::ArgTypeEnum::Boolean, {}),
    FunctionMetadata("isRecordingInputFromSaveState", "1.0", "isRecordingInputFromSaveState()",
                     IsRecordingInputFromSaveState, ScriptingEnums::ArgTypeEnum::Boolean, {}),
    FunctionMetadata("isPlayingInput", "1.0", "isPlayingInput()", IsPlayingInput,
                     ScriptingEnums::ArgTypeEnum::Boolean, {}),
    FunctionMetadata("isMovieActive", "1.0", "isMovieActive()", IsMovieActive,
                     ScriptingEnums::ArgTypeEnum::Boolean, {}),
    FunctionMetadata("getCurrentFrame", "1.0", "getCurrentFrame()", GetCurrentFrame,
                     ScriptingEnums::ArgTypeEnum::S64, {}),
    FunctionMetadata("getMovieLength", "1.0", "getMovieLength()", GetMovieLength,
                     ScriptingEnums::ArgTypeEnum::S64, {}),
    FunctionMetadata("getRerecordCount", "1.0", "getRerecordCount()", GetRerecordCount,
                     ScriptingEnums::ArgTypeEnum::S64, {}),
    FunctionMetadata("getCurrentInputCount", "1.0", "getCurrentInputCount()", GetCurrentInputCount,
                     ScriptingEnums::ArgTypeEnum::S64, {}),
    FunctionMetadata("getTotalInputCount", "1.0", "getTotalInputCount()", GetTotalInputCount,
                     ScriptingEnums::ArgTypeEnum::S64, {}),
    FunctionMetadata("getCurrentLagCount", "1.0", "getCurrentlagCount()", GetCurrentLagCount,
                     ScriptingEnums::ArgTypeEnum::S64, {}),
    FunctionMetadata("getTotalLagCount", "1.0", "getTotalLagCount()", GetTotalLagCount,
                     ScriptingEnums::ArgTypeEnum::S64, {}),
    FunctionMetadata("getRAMSize", "1.0", "getRAMSize()", GetRAMSize,
                     ScriptingEnums::ArgTypeEnum::U32, {}),
    FunctionMetadata("getL1CacheSize", "1.0", "getL1CacheSize()", GetL1CacheSize,
                     ScriptingEnums::ArgTypeEnum::U32, {}),
    FunctionMetadata("getFakeVMemSize", "1.0", "getFakeVMemSize()", GetFakeVMemSize,
                     ScriptingEnums::ArgTypeEnum::U32, {}),
    FunctionMetadata("getExRAMSize", "1.0", "getExRAMSize()", GetExRAMSize,
                     ScriptingEnums::ArgTypeEnum::U32, {})};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_statistics_functions_metadata_list,
                                                   api_version, deprecated_functions_map)};
}

ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_statistics_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_statistics_functions_metadata_list, api_version, function_name,
                               deprecated_functions_map);
}

ArgHolder* IsRecordingInput(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateBoolArgHolder(Movie::IsRecordingInput());
}

ArgHolder* IsRecordingInputFromSaveState(ScriptContext* current_script,
                                         std::vector<ArgHolder*>* args_list)
{
  return CreateBoolArgHolder(Movie::IsRecordingInputFromSaveState());
}

ArgHolder* IsPlayingInput(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateBoolArgHolder(Movie::IsPlayingInput());
}

ArgHolder* IsMovieActive(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateBoolArgHolder(Movie::IsMovieActive());
}

ArgHolder* GetCurrentFrame(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateS64ArgHolder(Movie::GetCurrentFrame());
}

ArgHolder* GetMovieLength(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateS64ArgHolder(Movie::GetTotalFrames());
}

ArgHolder* GetRerecordCount(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateS64ArgHolder(Movie::GetRerecordCount());
}

ArgHolder* GetCurrentInputCount(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateS64ArgHolder(Movie::GetCurrentInputCount());
}

ArgHolder* GetTotalInputCount(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateS64ArgHolder(Movie::GetTotalInputCount());
}

ArgHolder* GetCurrentLagCount(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateS64ArgHolder(Movie::GetCurrentLagCount());
}

ArgHolder* GetTotalLagCount(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateS64ArgHolder(Movie::GetTotalLagCount());
}

ArgHolder* GetRAMSize(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateU32ArgHolder(Core::System::GetInstance().GetMemory().getRAM_scriptHelper() !=
                                    nullptr ?
                                Core::System::GetInstance().GetMemory().GetRamSizeReal() :
                                0);
}

ArgHolder* GetL1CacheSize(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateU32ArgHolder(Core::System::GetInstance().GetMemory().getL1Cache_scriptHelper() !=
                                    nullptr ?
                                Core::System::GetInstance().GetMemory().GetL1CacheSize() :
                                0);
}

ArgHolder* GetFakeVMemSize(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateU32ArgHolder(Core::System::GetInstance().GetMemory().getFakeVMEM_scriptHelper() !=
                                    nullptr ?
                                Core::System::GetInstance().GetMemory().GetFakeVMemSize() :
                                0);
}

ArgHolder* GetExRAMSize(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return CreateU32ArgHolder(Core::System::GetInstance().GetMemory().getEXRAM_scriptHelper() !=
                                    nullptr ?
                                Core::System::GetInstance().GetMemory().GetExRamSizeReal() :
                                0);
}

}  // namespace Scripting::StatisticsApi
