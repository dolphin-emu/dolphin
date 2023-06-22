#pragma once

#include <string>
#include "Core/Scripting/HelperClasses/ArgHolder.h"
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/HelperClasses/ScriptContext.h"

namespace Scripting::ConfigAPI
{
extern const char* class_name;

ClassMetadata GetClassMetadataForVersion(const std::string& api_version);
ClassMetadata GetAllClassMetadata();
FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name);

ArgHolder* GetLayerNames_MostGlobalFirst(ScriptContext* script_context, std::vector<ArgHolder*>*);
ArgHolder* DoesLayerExist(ScriptContext* script_context, std::vector<ArgHolder*>* args_list);
ArgHolder* GetListOfSystems(ScriptContext* script_context, std::vector<ArgHolder*>* args_list);
ArgHolder* GetConfigEnumTypes(ScriptContext* script_context, std::vector<ArgHolder*>* args_list);
ArgHolder* GetListOfValidValuesForEnumType(ScriptContext* script_context,
                                           std::vector<ArgHolder*>* args_list);

ArgHolder* GetBooleanConfigSettingForLayer(ScriptContext* current_script,
                                           std::vector<ArgHolder*>* args_list);
ArgHolder* GetSignedIntConfigSettingForLayer(ScriptContext* current_script,
                                             std::vector<ArgHolder*>* args_list);
ArgHolder* GetUnsignedIntConfigSettingForLayer(ScriptContext* current_script,
                                               std::vector<ArgHolder*>* args_list);
ArgHolder* GetFloatConfigSettingForLayer(ScriptContext* current_script,
                                         std::vector<ArgHolder*>* args_list);
ArgHolder* GetStringConfigSettingForLayer(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list);
ArgHolder* GetEnumConfigSettingForLayer(ScriptContext* current_script,
                                        std::vector<ArgHolder*>* args_list);

ArgHolder* SetBooleanConfigSettingForLayer(ScriptContext* current_script,
                                           std::vector<ArgHolder*>* args_list);
ArgHolder* SetSignedIntConfigSettingForLayer(ScriptContext* current_script,
                                             std::vector<ArgHolder*>* args_list);
ArgHolder* SetUnsignedIntConfigSettingForLayer(ScriptContext* current_script,
                                               std::vector<ArgHolder*>* args_list);
ArgHolder* SetFloatConfigSettingForLayer(ScriptContext* current_script,
                                         std::vector<ArgHolder*>* args_list);
ArgHolder* SetStringConfigSettingForLayer(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list);
ArgHolder* SetEnumConfigSettingForLayer(ScriptContext* current_script,
                                        std::vector<ArgHolder*>* args_list);

ArgHolder* DeleteBooleanConfigSettingFromLayer(ScriptContext* current_script,
                                               std::vector<ArgHolder*>* args_list);
ArgHolder* DeleteSignedIntConfigSettingFromLayer(ScriptContext* current_script,
                                                 std::vector<ArgHolder*>* args_list);
ArgHolder* DeleteUnsignedIntConfigSettingFromLayer(ScriptContext* current_script,
                                                   std::vector<ArgHolder*>* args_list);
ArgHolder* DeleteFloatConfigSettingFromLayer(ScriptContext* current_script,
                                             std::vector<ArgHolder*>* args_list);
ArgHolder* DeleteStringConfigSettingFromLayer(ScriptContext* current_script,
                                              std::vector<ArgHolder*>* args_list);
ArgHolder* DeleteEnumConfigSettingFromLayer(ScriptContext* current_script,
                                            std::vector<ArgHolder*>* args_list);

ArgHolder* GetBooleanConfigSetting(ScriptContext* current_script,
                                   std::vector<ArgHolder*>* args_list);
ArgHolder* GetSignedIntConfigSetting(ScriptContext* current_script,
                                     std::vector<ArgHolder*>* args_list);
ArgHolder* GetUnsignedIntConfigSetting(ScriptContext* current_script,
                                       std::vector<ArgHolder*>* args_list);
ArgHolder* GetFloatConfigSetting(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
ArgHolder* GetStringConfigSetting(ScriptContext* current_script,
                                  std::vector<ArgHolder*>* args_list);
ArgHolder* GetEnumConfigSetting(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

ArgHolder* SaveConfigSettings(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);

}  // namespace Scripting::ConfigAPI
