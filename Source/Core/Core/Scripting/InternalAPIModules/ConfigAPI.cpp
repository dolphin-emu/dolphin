#include "Core/Scripting/InternalAPIModules/ConfigAPI.h"

#include <cctype>
#include <optional>
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "common/Logging/Log.h"
#include "AudioCommon/Enums.h"
#include "Core/Config/MainSettings.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/FreeLookConfig.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/HSP/HSP_Device.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"
#include "DiscIO/Enums.h"
#include "VideoCommon/VideoConfig.h"

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

static std::map<Config::ValueType, std::map<std::string, int>> map_of_enum_type_from_string_to_int = {

  {Config::ValueType::CPU_CORE,
    {
      {"INTERPRETER", (int)PowerPC::CPUCore::Interpreter},
      {"JIT64", (int) PowerPC::CPUCore::JIT64},
      {"JITARM64", (int) PowerPC::CPUCore::JITARM64},
      {"CACHEDINTERPRETER", (int) PowerPC::CPUCore::CachedInterpreter}
    }
  },

  {Config::ValueType::DPL2_QUALITY,
    {
      {"LOWEST", (int)AudioCommon::DPL2Quality::Lowest},
      {"LOW", (int)AudioCommon::DPL2Quality::Low},
      {"HIGH", (int)AudioCommon::DPL2Quality::High},
      {"HIGHEST", (int)AudioCommon::DPL2Quality::Highest}
    }
  },

  {Config::ValueType::EXI_DEVICE_TYPE,
      {
        {"DUMMY", (int) ExpansionInterface::EXIDeviceType::Dummy},
        {"MEMORYCARD", (int) ExpansionInterface::EXIDeviceType::MemoryCard},
        {"MASKROM", (int) ExpansionInterface::EXIDeviceType::MaskROM},
        {"AD16", (int) ExpansionInterface::EXIDeviceType::AD16},
        {"MICROPHONE", (int) ExpansionInterface::EXIDeviceType::Microphone},
        {"ETHERNET", (int) ExpansionInterface::EXIDeviceType::Ethernet},
        {"AMBASEBOARD", (int) ExpansionInterface::EXIDeviceType::AMBaseboard},
        {"GECKO", (int) ExpansionInterface::EXIDeviceType::Gecko},
        {"MEMORYCARDFOLDER", (int) ExpansionInterface::EXIDeviceType::MemoryCardFolder},
        {"AGP", (int) ExpansionInterface::EXIDeviceType::AGP},
        {"ETHERNETXLINK", (int) ExpansionInterface::EXIDeviceType::EthernetXLink},
        {"ETHERNETTAPSERVER", (int) ExpansionInterface::EXIDeviceType::EthernetTapServer},
        {"ETHERNETBUILTIN", (int) ExpansionInterface::EXIDeviceType::EthernetBuiltIn},
        {"NONE", (int) ExpansionInterface::EXIDeviceType::None}
      }
  },

  {Config::ValueType::SI_DEVICE_TYPE,
    {
      {"NONE", (int) SerialInterface::SIDevices::SIDEVICE_NONE},
      {"N64_MIC", (int)SerialInterface::SIDevices::SIDEVICE_N64_MIC},
      {"N64_KEYBOARD", (int) SerialInterface::SIDevices::SIDEVICE_N64_KEYBOARD},
      {"N64_MOUSE", (int)SerialInterface::SIDevices::SIDEVICE_N64_MOUSE},
      {"N64_CONTROLLER", (int)SerialInterface::SIDevices::SIDEVICE_N64_CONTROLLER},
      {"GC_GBA", (int)SerialInterface::SIDevices::SIDEVICE_GC_GBA},
      {"GC_CONTROLLER", (int)SerialInterface::SIDevices::SIDEVICE_GC_CONTROLLER},
      {"GC_KEYBOARD", (int)SerialInterface::SIDevices::SIDEVICE_GC_KEYBOARD},
      {"GC_STEERING", (int)SerialInterface::SIDevices::SIDEVICE_GC_STEERING},
      {"DANCEMAT", (int)SerialInterface::SIDevices::SIDEVICE_DANCEMAT},
      {"GC_TARUKONGA", (int) SerialInterface::SIDevices::SIDEVICE_GC_TARUKONGA},
      {"AM_BASEBOARD", (int)SerialInterface::SIDevices::SIDEVICE_AM_BASEBOARD},
      {"WIIU_ADAPTER", (int)SerialInterface::SIDevices::SIDEVICE_WIIU_ADAPTER},
      {"GC_GBA_EMULATED", (int)SerialInterface::SIDevices::SIDEVICE_GC_GBA_EMULATED},
    }
  },

  {Config::ValueType::HSP_DEVICE_TYPE,
    {
      {"NONE", (int)HSP::HSPDeviceType::None},
      {"ARAMEXPANSION", (int)HSP::HSPDeviceType::ARAMExpansion}
    }
  },

  {Config::ValueType::REGION,
    {
      {"NTSC_J", (int)DiscIO::Region::NTSC_J},
      {"NTSC_U", (int)DiscIO::Region::NTSC_U},
      {"PAL", (int)DiscIO::Region::PAL},
      {"NTSC_K", (int)DiscIO::Region::NTSC_K},
      {"UNKNOWN", (int)DiscIO::Region::Unknown}
    }
  },

  {Config::ValueType::SHOW_CURSOR_VALUE_TYPE,
    {
      {"NEVER", (int)Config::ShowCursor::Never},
      {"CONSTANTLY", (int)Config::ShowCursor::Constantly},
      {"ONMOVEMENT", (int)Config::ShowCursor::OnMovement}
    }
  },

  {Config::ValueType::LOG_LEVEL_TYPE,
    {
      {"LNOTICE", (int)Common::Log::LogLevel::LNOTICE},
      {"LERROR", (int)Common::Log::LogLevel::LERROR},
      {"LWARNING", (int)Common::Log::LogLevel::LWARNING},
      {"LINFO", (int)Common::Log::LogLevel::LINFO},
      {"LDEBUG", (int)Common::Log::LogLevel::LDEBUG}
    }
  },

  {Config::ValueType::FREE_LOOK_CONTROL_TYPE,
    {
      {"SIXAXIS", (int)FreeLook::ControlType::SixAxis},
      {"FPS", (int)FreeLook::ControlType::FPS},
      {"ORBITAL", (int)FreeLook::ControlType::Orbital}
    }
  },

  {Config::ValueType::ASPECT_MODE,
    {
      {"AUTO", (int)AspectMode::Auto},
      {"ANALOGWIDE", (int)AspectMode::AnalogWide},
      {"ANALOG", (int)AspectMode::Analog},
      {"STRETCH", (int)AspectMode::Stretch}
    }
  },

  {Config::ValueType::SHADER_COMPILATION_MODE,
    {
      {"SYNCHRONOUS", (int)ShaderCompilationMode::Synchronous},
      {"SYNCHRONOUSUBERSHADERS", (int)ShaderCompilationMode::SynchronousUberShaders},
      {"ASYNCHRONOUSUBERSHADERS", (int)ShaderCompilationMode::AsynchronousUberShaders},
      {"ASYNCHRONOUSSKIPRENDERING", (int)ShaderCompilationMode::AsynchronousSkipRendering}
    }
  },

  {Config::ValueType::TRI_STATE,
    {
      {"OFF", (int)TriState::Off},
      {"ON", (int)TriState::On},
      {"AUTO", (int)TriState::Auto}
    }
  },

  {Config::ValueType::TEXTURE_FILTERING_MODE,
    {
      {"DEFAULT", (int)TextureFilteringMode::Default},
      {"NEAREST", (int)TextureFilteringMode::Nearest},
      {"LINEAR", (int)TextureFilteringMode::Linear}
    }
  },

  {Config::ValueType::STERERO_MODE,
    {
      {"OFF", (int)StereoMode::Off},
      {"SBS", (int)StereoMode::SBS},
      {"TAB", (int)StereoMode::TAB},
      {"ANAGLYPH", (int)StereoMode::Anaglyph},
      {"QUADBUFFER", (int)StereoMode::QuadBuffer},
      {"PASSIVE", (int)StereoMode::Passive}
    }
  },

  {Config::ValueType::WIIMOTE_SOURCE,
    {
      {"NONE", (int)WiimoteSource::None},
      {"EMULATED", (int)WiimoteSource::Emulated},
      {"REAL", (int)WiimoteSource::Real}
    }
  }

};

static std::map<Config::ValueType, std::map<int, std::string>> ConvertMapFromStringToInt(const std::map<Config::ValueType, std::map<std::string, int>>& input_map)
{
  std::map<Config::ValueType, std::map<int, std::string>> return_map = std::map<Config::ValueType, std::map<int, std::string>>();
  for (auto& outer_iterator : input_map)
  {
    return_map[outer_iterator.first] = std::map<int, std::string>();
    for (auto& inner_iterator : outer_iterator.second)
    {
      return_map[outer_iterator.first][inner_iterator.second] = inner_iterator.first;
    }
  }
  return return_map;
}

static std::map<Config::ValueType, std::map<int, std::string>> map_of_enum_type_int_to_string = ConvertMapFromStringToInt(map_of_enum_type_from_string_to_int);


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


std::string ConvertEnumIntToStringForType(Config::ValueType enum_type, int enum_val)
{
  if (map_of_enum_type_int_to_string.count(enum_type) > 0 &&
      map_of_enum_type_int_to_string[enum_type].count(enum_val) > 0)
    return map_of_enum_type_int_to_string[enum_type][enum_val];
  else
    return "";
}

int ConvertEnumStringToIntForType(Config::ValueType enum_type, std::string enum_val)
{
  if (map_of_enum_type_from_string_to_int.count(enum_type) > 0 &&
      map_of_enum_type_from_string_to_int[enum_type].count(enum_val) > 0)
    return map_of_enum_type_from_string_to_int[enum_type][enum_val];
  else
    return -1;
}

std::optional<Config::ValueType> ParseEnumType(const std::string& enum_name)
{
  std::string uppercase_enum_name = std::string(enum_name.c_str());
  size_t str_length = uppercase_enum_name.length();
  for (size_t i = 0; i < str_length; ++i)
    uppercase_enum_name[i] = toupper(uppercase_enum_name[i]);

  if (uppercase_enum_name == "CPUCORE")
    return Config::ValueType::CPU_CORE;
  else if (uppercase_enum_name == "DPL2QUALITY")
    return Config::ValueType::DPL2_QUALITY;
  else if (uppercase_enum_name == "EXIDEVICETYPE")
    return Config::ValueType::EXI_DEVICE_TYPE;
  else if (uppercase_enum_name == "SIDEVICETYPE")
    return Config::ValueType::SI_DEVICE_TYPE;
  else if (uppercase_enum_name == "HSPDEVICETYPE")
    return Config::ValueType::HSP_DEVICE_TYPE;
  else if (uppercase_enum_name == "REGION")
    return Config::ValueType::REGION;
  else if (uppercase_enum_name == "SHOWCURSOR")
    return Config::ValueType::SHOW_CURSOR_VALUE_TYPE;
  else if (uppercase_enum_name == "LOGLEVEL")
    return Config::ValueType::LOG_LEVEL_TYPE;
  else if (uppercase_enum_name == "FREELOOKCONTROL")
    return Config::ValueType::FREE_LOOK_CONTROL_TYPE;
  else if (uppercase_enum_name == "ASPECTMODE")
    return Config::ValueType::ASPECT_MODE;
  else if (uppercase_enum_name == "SHADERCOMPILATIONMODE")
    return Config::ValueType::SHADER_COMPILATION_MODE;
  else if (uppercase_enum_name == "TRISTATE")
    return Config::ValueType::TRI_STATE;
  else if (uppercase_enum_name == "TEXTUREFILTERINGMODE")
    return Config::ValueType::TEXTURE_FILTERING_MODE;
  else if (uppercase_enum_name == "STEREOMODE")
    return Config::ValueType::STERERO_MODE;
  else if (uppercase_enum_name == "WIIMOTESOURCE")
    return Config::ValueType::WIIMOTE_SOURCE;
  else
    return {};
}

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
