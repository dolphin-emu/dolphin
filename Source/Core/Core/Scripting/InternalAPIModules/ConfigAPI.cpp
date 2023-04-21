#include "Core/Scripting/InternalAPIModules/ConfigAPI.h"

#include <cctype>
#include <optional>
#include "Common/Config/Config.h"
#include "common/Config/Layer.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"

namespace Scripting::ConfigAPI
{

const char* class_name = "ConfigAPI";
static std::array all_config_functions_metadata_list = {
    FunctionMetadata("getLayerNames_mostGlobalFirst", "1.0", "getLayerNames_mostGlobalFirst()", GetLayerNames_MostGlobalFirst, ArgTypeEnum::String, {}),
    FunctionMetadata("getListOfSystems", "1.0", "getListOfSystems()", GetListOfSystems, ArgTypeEnum::String, {}),
    FunctionMetadata("getConfigEnumTypes", "1.0", "getConfigEnumTypes()", GetConfigEnumTypes, ArgTypeEnum::String, {}),
    FunctionMetadata("getListOfValidValuesForEnumType", "1.0", "getListOfValidValuesForEnumType", GetListOfValidValuesForEnumType, ArgTypeEnum::String, {ArgTypeEnum::String}),
    FunctionMetadata("getAllSettings", "1.0", "getAllSettings()", GetAllSettings, ArgTypeEnum::String, {}),


    FunctionMetadata("getBooleanConfigSettingForLayer", "1.0", "getBooleanConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Interface\", \"debugModeEnabled\")", GetBooleanConfigSettingForLayer, ArgTypeEnum::Boolean, {ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String}),
    FunctionMetadata("getSignedIntConfigSettingForLayer", "1.0", "getSignedIntConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"TimingVariance\")", GetSignedIntConfigSettingForLayer, ArgTypeEnum::Integer, {ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String}),
    FunctionMetadata("getUnsignedIntConfigSettingForLayer", "1.0", "getUnsignedIntConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"MEM1Size\")", GetUnsignedIntConfigSettingForLayer, ArgTypeEnum::U32, {ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String}),
    FunctionMetadata("getFloatConfigSettingForLayer", "1.0", "getFloatConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"EmulationSpeed\")", GetFloatConfigSettingForLayer, ArgTypeEnum::Float, {ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String}),
    FunctionMetadata("getStringConfigSettingForLayer", "1.0", "getStringConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"MemcardAPath\")", GetStringConfigSettingForLayer, ArgTypeEnum::String, {ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String}),
    FunctionMetadata("getEnumConfigSettingForLayer", "1.0", "getEnumConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"SlotA\", \"ExiDeviceType\")", GetEnumConfigSettingForLayer, ArgTypeEnum::String, {ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String, ArgTypeEnum::String}),





};

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_config_functions_metadata_list, api_version, deprecated_functions_map)};
}
ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_config_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version, const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_config_functions_metadata_list, api_version, function_name, deprecated_functions_map);
}

ArgHolder GetLayerNames_MostGlobalFirst(ScriptContext* current_script,
                                        std::vector<ArgHolder>& args_list);
ArgHolder GetListOfSystems(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetConfigEnumTypes(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetListOfValidValuesForEnumType(ScriptContext* current_script,
                                          std::vector<ArgHolder>& args_list);
ArgHolder GetAllSettings(ScriptContext* current_script, std::vector<ArgHolder>& args_list);


std::optional<Config::LayerType> ParseLayer(const std::string& layer_name)
{
  std::string uppercase_layer_name = std::string(layer_name.c_str());
  size_t str_length = uppercase_layer_name.length();
  for (size_t i = 0; i < str_length; ++i)
    uppercase_layer_name[i] = toupper(uppercase_layer_name[i]);

  if (uppercase_layer_name == "BASE")
    return Config::LayerType::Base;
  else if (uppercase_layer_name == "COMMANDLINE")
    return Config::LayerType::CommandLine;
  else if (uppercase_layer_name == "GLOBALGAME")
    return Config::LayerType::GlobalGame;
  else if (uppercase_layer_name == "LOCALGAME")
    return Config::LayerType::LocalGame;
  else if (uppercase_layer_name == "MOVIE")
    return Config::LayerType::Movie;
  else if (uppercase_layer_name == "NETPLAY")
    return Config::LayerType::Netplay;
  else if (uppercase_layer_name == "CURRENTRUN")
    return Config::LayerType::CurrentRun;
  else if (uppercase_layer_name == "META")
    return Config::LayerType::Meta;
  else
    return {};
}

std::optional<Config::System> ParseSystem(const std::string& system_name)
{
  std::string uppercase_system_name = std::string(system_name.c_str());
  size_t str_length = uppercase_system_name.length();
  for (size_t i = 0; i < str_length; ++i)
    uppercase_system_name[i] = toupper(uppercase_system_name[i]);

  if (uppercase_system_name == "MAIN" || uppercase_system_name == "DOLPHIN")
    return Config::System::Main;
  else if (uppercase_system_name == "SYSCONF")
    return Config::System::SYSCONF;
  else if (uppercase_system_name == "GCPAD")
    return Config::System::GCPad;
  else if (uppercase_system_name == "WIIPAD")
    return Config::System::WiiPad;
  else if (uppercase_system_name == "GCKEYBOARD")
    return Config::System::GCKeyboard;
  else if (uppercase_system_name == "GFX")
    return Config::System::GFX;
  else if (uppercase_system_name == "LOGGER")
    return Config::System::Logger;
  else if (uppercase_system_name == "DEBUGGER")
    return Config::System::Debugger;
  else if (uppercase_system_name == "DUALSHOCKUDPCLIENT")
    return Config::System::DualShockUDPClient;
  else if (uppercase_system_name == "FREELOOK")
    return Config::System::FreeLook;
  else if (uppercase_system_name == "SESSION")
    return Config::System::Session;
  else if (uppercase_system_name == "GAMESETTINGSONLY")
    return Config::System::GameSettingsOnly;
  else if (uppercase_system_name == "ACHIEVEMENTS")
    return Config::System::Achievements;
  else
    return {};
}

template <typename T>
ArgHolder GetConfigSettingForLayer(std::vector<ArgHolder>& args_list, T default_value)
{
  std::optional<Config::LayerType> layer_name = ParseLayer(args_list[0].string_val);
  std::optional<Config::System> system_name = ParseSystem(args_list[1].string_val);
  std::string section_name = args_list[2].string_val;
  std::string setting_name = args_list[3].string_val;

  if (!layer_name.has_value())
    return CreateErrorStringArgHolder("Invalid layer name of " + args_list[0].string_val +
                                      " was used.");
  else if (!system_name.has_value())
    return CreateErrorStringArgHolder("Invalid system name of " + args_list[1].string_val +
                                      " was used.");

  std::optional<T> returned_config_val;

  if (layer_name.value() == Config::LayerType::Meta)
    returned_config_val = Config::Get(
        Config::Info<T>({system_name.value(), section_name, setting_name}, default_value));
  else
  {
    Config::Location location = {system_name.value(), section_name, setting_name};
    returned_config_val = Config::GetLayer(layer_name.value())->Get<T>(location);
  }

  if (!returned_config_val.has_value())
    return CreateEmptyOptionalArgument();
  else
  {
    if (std::is_same<T, bool>::value)
      return CreateBoolArgHolder((*((std::optional<bool>*) &returned_config_val)).value());
    else if (std::is_same<T, int>::value)
      return CreateIntArgHolder((*((std::optional<int>*) &returned_config_val)).value());
    else if (std::is_same<T, u32>::value)
      return CreateU32ArgHolder((*((std::optional<u32>*) &returned_config_val)).value());
    else if (std::is_same<T, float>::value)
      return CreateFloatArgHolder((*((std::optional<float>*) &returned_config_val)).value());
    else
      return CreateStringArgHolder((*((std::optional<std::string>*) &returned_config_val)).value());
  }

}

ArgHolder GetBooleanConfigSettingForLayer(ScriptContext* current_script,
                                          std::vector<ArgHolder>& args_list)
{
  return GetConfigSettingForLayer(args_list, (bool) false);
}

ArgHolder GetSignedIntConfigSettingForLayer(ScriptContext* current_script,
                                            std::vector<ArgHolder>& args_list)
{
  return GetConfigSettingForLayer(args_list, (int)0);
}

ArgHolder GetUnsignedIntConfigSettingForLayer(ScriptContext* current_script,
                                              std::vector<ArgHolder>& args_list)
{
  return GetConfigSettingForLayer(args_list, (u32)0);
}

ArgHolder GetFloatConfigSettingForLayer(ScriptContext* current_script,
                                        std::vector<ArgHolder>& args_list)
{
  return GetConfigSettingForLayer(args_list, (float)0.0);
}

ArgHolder GetStringConfigSettingForLayer(ScriptContext* current_script,
                                         std::vector<ArgHolder>& args_list)
{
 return GetConfigSettingForLayer(args_list, std::string());
}

ArgHolder GetEnumConfigSettingForLayer(ScriptContext* current_script,
                                       std::vector<ArgHolder>& args_list);



ArgHolder SetBooleanConfigSettingForLayer(ScriptContext* current_script,
                                          std::vector<ArgHolder>& args_list);
ArgHolder SetSignedIntConfigSettingForLayer(ScriptContext* current_script,
                                            std::vector<ArgHolder>& args_list);
ArgHolder SetUnsignedIntConfigSettingForLayer(ScriptContext* current_script,
                                              std::vector<ArgHolder>& args_list);
ArgHolder SetFloatConfigSettingForLayer(ScriptContext* current_script,
                                        std::vector<ArgHolder>& args_list);
ArgHolder SetStringConfigSettingForLayer(ScriptContext* current_script,
                                         std::vector<ArgHolder>& args_list);
ArgHolder SetEnumConfigSettingForLayer(ScriptContext* current_script,
                                       std::vector<ArgHolder>& args_list);

ArgHolder GetBooleanConfigSetting(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetSignedIntConfigSetting(ScriptContext* current_script,
                                    std::vector<ArgHolder>& args_list);
ArgHolder GetUnsignedIntConfigSetting(ScriptContext* current_script,
                                      std::vector<ArgHolder>& args_list);
ArgHolder GetFloatConfigSetting(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetStringConfigSetting(ScriptContext* current_script, std::vector<ArgHolder>& args_list);
ArgHolder GetEnumConfigSetting(ScriptContext* current_script, std::vector<ArgHolder>& args_list);

ArgHolder SaveConfigSettings(ScriptContext* current_script, std::vector<ArgHolder>& args_list);


}  // namespace Scripting::ConfigAPI
