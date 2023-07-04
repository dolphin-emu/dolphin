#include "Core/Scripting/InternalAPIModules/ConfigAPI.h"

#include <cctype>
#include <optional>
#include "AudioCommon/Enums.h"
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "Common/Logging/Log.h"
#include "Core/Config/MainSettings.h"
#include "Core/FreeLookConfig.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/HSP/HSP_Device.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/Scripting/CoreScriptContextFiles/Enums/ArgTypeEnum.h"
#include "Core/Scripting/HelperClasses/VersionResolver.h"
#include "DiscIO/Enums.h"
#include "VideoCommon/VideoConfig.h"

namespace Scripting::ConfigAPI
{

const char* class_name = "ConfigAPI";
static std::array all_config_functions_metadata_list = {

    FunctionMetadata("getLayerNames_mostGlobalFirst", "1.0", "getLayerNames_mostGlobalFirst()",
                     GetLayerNames_MostGlobalFirst, ScriptingEnums::ArgTypeEnum::String, {}),
    FunctionMetadata("doesLayerExist", "1.0", "doesLayerExist(\"Movie\")", DoesLayerExist,
                     ScriptingEnums::ArgTypeEnum::Boolean, {ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("getListOfSystems", "1.0", "getListOfSystems()", GetListOfSystems,
                     ScriptingEnums::ArgTypeEnum::String, {}),
    FunctionMetadata("getConfigEnumTypes", "1.0", "getConfigEnumTypes()", GetConfigEnumTypes,
                     ScriptingEnums::ArgTypeEnum::String, {}),
    FunctionMetadata("getListOfValidValuesForEnumType", "1.0",
                     "getListOfValidValuesForEnumType(\"EXIDeviceType\")",
                     GetListOfValidValuesForEnumType, ScriptingEnums::ArgTypeEnum::String,
                     {ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("getBooleanConfigSettingForLayer", "1.0",
                     "getBooleanConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Interface\", "
                     "\"debugModeEnabled\")",
                     GetBooleanConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata(
        "getSignedIntConfigSettingForLayer", "1.0",
        "getSignedIntConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"TimingVariance\")",
        GetSignedIntConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::S32,
        {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
         ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata(
        "getUnsignedIntConfigSettingForLayer", "1.0",
        "getUnsignedIntConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"MEM1Size\")",
        GetUnsignedIntConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::U32,
        {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
         ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata(
        "getFloatConfigSettingForLayer", "1.0",
        "getFloatConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"EmulationSpeed\")",
        GetFloatConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::Float,
        {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
         ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata(
        "getStringConfigSettingForLayer", "1.0",
        "getStringConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"MemcardAPath\")",
        GetStringConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::String,
        {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
         ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("getEnumConfigSettingForLayer", "1.0",
                     "getEnumConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"SlotA\", "
                     "\"EXIDeviceType\")",
                     GetEnumConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::String,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("setBooleanConfigSettingForLayer", "1.0",
                     "setBooleanConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Interface\", "
                     "\"debugModeEnabled\", true)",
                     SetBooleanConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::Boolean}),
    FunctionMetadata("setSignedIntConfigSettingForLayer", "1.0",
                     "setSignedIntConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", "
                     "\"TimingVariance\", 30)",
                     SetSignedIntConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::S32}),
    FunctionMetadata("setUnsignedIntConfigSettingForLayer", "1.0",
                     "setUnsignedIntConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", "
                     "\"MEM1Size\", 500)",
                     SetUnsignedIntConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::U32}),
    FunctionMetadata("setFloatConfigSettingForLayer", "1.0",
                     "setFloatConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", "
                     "\"EmulationSpeed\", 3.4)",
                     SetFloatConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::Float}),
    FunctionMetadata("setStringConfigSettingForLayer", "1.0",
                     "setStringConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", "
                     "\"MemcardAPath\", \"MyFolder/subDirectory/memcardFolder\")",
                     SetStringConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("setEnumConfigSettingForLayer", "1.0",
                     "setEnumConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"SlotA\", "
                     "\"EXIDeviceType\", \"MemoryCard\")",
                     SetEnumConfigSettingForLayer, ScriptingEnums::ArgTypeEnum::VoidType,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("deleteBooleanConfigSettingFromLayer", "1.0",
                     "deleteBooleanConfigSettingFromLayer(\"GlobalGame\", \"Main\", \"Interface\", "
                     "\"debugModeEnabled\")",
                     DeleteBooleanConfigSettingFromLayer, ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("deleteSignedIntConfigSettingFromLayer", "1.0",
                     "deleteSignedIntConfigSettingFromLayer(\"GlobalGame\", \"Main\", \"Core\", "
                     "\"TimingVariance\")",
                     DeleteSignedIntConfigSettingFromLayer, ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("deleteUnsignedIntConfigSettingFromLayer", "1.0",
                     "deleteUnsignedIntConfigSettingFromLayer(\"GlobalGame\", \"Main\", \"Core\", "
                     "\"MEM1Size\")",
                     DeleteUnsignedIntConfigSettingFromLayer, ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("deleteFloatConfigSettingFromLayer", "1.0",
                     "deleteFloatConfigSettingFromLayer(\"GlobalGame\", \"Main\", \"Core\", "
                     "\"EmulationSpeed\")",
                     DeleteFloatConfigSettingFromLayer, ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("deleteStringConfigSettingFromLayer", "1.0",
                     "deleteStringConfigSettingFromLayer(\"GlobalGame\", \"Main\", \"Core\", "
                     "\"MemcardAPath\")",
                     DeleteStringConfigSettingFromLayer, ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata(
        "deleteEnumConfigSettingFromLayer", "1.0",
        "deleteEnumConfigSettingForLayer(\"GlobalGame\", \"Main\", \"Core\", \"SlotA\", "
        "\"EXIDeviceType\")",
        DeleteEnumConfigSettingFromLayer, ScriptingEnums::ArgTypeEnum::Boolean,
        {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
         ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
         ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("getBooleanConfigSetting", "1.0",
                     "getBooleanConfigSetting(\"Main\", \"Interface\", "
                     "\"debugModeEnabled\")",
                     GetBooleanConfigSetting, ScriptingEnums::ArgTypeEnum::Boolean,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("getSignedIntConfigSetting", "1.0",
                     "getSignedIntConfigSetting(\"Main\", \"Core\", \"TimingVariance\")",
                     GetSignedIntConfigSetting, ScriptingEnums::ArgTypeEnum::S32,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("getUnsignedIntConfigSetting", "1.0",
                     "getUnsignedIntConfigSetting(\"Main\", \"Core\", \"MEM1Size\")",
                     GetUnsignedIntConfigSetting, ScriptingEnums::ArgTypeEnum::U32,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("getFloatConfigSetting", "1.0",
                     "getFloatConfigSetting(\"Main\", \"Core\", \"EmulationSpeed\")",
                     GetFloatConfigSetting, ScriptingEnums::ArgTypeEnum::Float,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("getStringConfigSetting", "1.0",
                     "getStringConfigSetting(\"Main\", \"Core\", \"MemcardAPath\")",
                     GetStringConfigSetting, ScriptingEnums::ArgTypeEnum::String,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String}),
    FunctionMetadata("getEnumConfigSetting", "1.0",
                     "getEnumConfigSetting(\"Main\", \"Core\", \"SlotA\", "
                     "\"EXIDeviceType\")",
                     GetEnumConfigSetting, ScriptingEnums::ArgTypeEnum::String,
                     {ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String,
                      ScriptingEnums::ArgTypeEnum::String, ScriptingEnums::ArgTypeEnum::String}),

    FunctionMetadata("saveSettings", "1.0", "saveSettings()", SaveConfigSettings,
                     ScriptingEnums::ArgTypeEnum::VoidType, {})};

static std::string ConvertToUpperCase(const std::string& input_string)
{
  std::string return_string = std::string(input_string.c_str());
  size_t str_length = input_string.length();
  for (size_t i = 0; i < str_length; ++i)
    return_string[i] = toupper(return_string[i]);
  return return_string;
}

static std::map<Config::ValueType, std::map<std::string, int>> map_of_enum_type_from_string_to_int =
    {

        {Config::ValueType::CPU_CORE,
         {{"INTERPRETER", (int)PowerPC::CPUCore::Interpreter},
          {"JIT64", (int)PowerPC::CPUCore::JIT64},
          {"JITARM64", (int)PowerPC::CPUCore::JITARM64},
          {"CACHEDINTERPRETER", (int)PowerPC::CPUCore::CachedInterpreter}}},

        {Config::ValueType::DPL2_QUALITY,
         {{"LOWEST", (int)AudioCommon::DPL2Quality::Lowest},
          {"LOW", (int)AudioCommon::DPL2Quality::Low},
          {"HIGH", (int)AudioCommon::DPL2Quality::High},
          {"HIGHEST", (int)AudioCommon::DPL2Quality::Highest}}},

        {Config::ValueType::EXI_DEVICE_TYPE,
         {{"DUMMY", (int)ExpansionInterface::EXIDeviceType::Dummy},
          {"MEMORYCARD", (int)ExpansionInterface::EXIDeviceType::MemoryCard},
          {"MASKROM", (int)ExpansionInterface::EXIDeviceType::MaskROM},
          {"AD16", (int)ExpansionInterface::EXIDeviceType::AD16},
          {"MICROPHONE", (int)ExpansionInterface::EXIDeviceType::Microphone},
          {"ETHERNET", (int)ExpansionInterface::EXIDeviceType::Ethernet},
          {"AMBASEBOARD", (int)ExpansionInterface::EXIDeviceType::AMBaseboard},
          {"GECKO", (int)ExpansionInterface::EXIDeviceType::Gecko},
          {"MEMORYCARDFOLDER", (int)ExpansionInterface::EXIDeviceType::MemoryCardFolder},
          {"AGP", (int)ExpansionInterface::EXIDeviceType::AGP},
          {"ETHERNETXLINK", (int)ExpansionInterface::EXIDeviceType::EthernetXLink},
          {"ETHERNETTAPSERVER", (int)ExpansionInterface::EXIDeviceType::EthernetTapServer},
          {"ETHERNETBUILTIN", (int)ExpansionInterface::EXIDeviceType::EthernetBuiltIn},
          {"NONE", (int)ExpansionInterface::EXIDeviceType::None}}},

        {Config::ValueType::SI_DEVICE_TYPE,
         {
             {"NONE", (int)SerialInterface::SIDevices::SIDEVICE_NONE},
             {"N64_MIC", (int)SerialInterface::SIDevices::SIDEVICE_N64_MIC},
             {"N64_KEYBOARD", (int)SerialInterface::SIDevices::SIDEVICE_N64_KEYBOARD},
             {"N64_MOUSE", (int)SerialInterface::SIDevices::SIDEVICE_N64_MOUSE},
             {"N64_CONTROLLER", (int)SerialInterface::SIDevices::SIDEVICE_N64_CONTROLLER},
             {"GC_GBA", (int)SerialInterface::SIDevices::SIDEVICE_GC_GBA},
             {"GC_CONTROLLER", (int)SerialInterface::SIDevices::SIDEVICE_GC_CONTROLLER},
             {"GC_KEYBOARD", (int)SerialInterface::SIDevices::SIDEVICE_GC_KEYBOARD},
             {"GC_STEERING", (int)SerialInterface::SIDevices::SIDEVICE_GC_STEERING},
             {"DANCEMAT", (int)SerialInterface::SIDevices::SIDEVICE_DANCEMAT},
             {"GC_TARUKONGA", (int)SerialInterface::SIDevices::SIDEVICE_GC_TARUKONGA},
             {"AM_BASEBOARD", (int)SerialInterface::SIDevices::SIDEVICE_AM_BASEBOARD},
             {"WIIU_ADAPTER", (int)SerialInterface::SIDevices::SIDEVICE_WIIU_ADAPTER},
             {"GC_GBA_EMULATED", (int)SerialInterface::SIDevices::SIDEVICE_GC_GBA_EMULATED},
         }},

        {Config::ValueType::HSP_DEVICE_TYPE,
         {{"NONE", (int)HSP::HSPDeviceType::None},
          {"ARAMEXPANSION", (int)HSP::HSPDeviceType::ARAMExpansion}}},

        {Config::ValueType::REGION,
         {{"NTSC_J", (int)DiscIO::Region::NTSC_J},
          {"NTSC_U", (int)DiscIO::Region::NTSC_U},
          {"PAL", (int)DiscIO::Region::PAL},
          {"NTSC_K", (int)DiscIO::Region::NTSC_K},
          {"UNKNOWN", (int)DiscIO::Region::Unknown}}},

        {Config::ValueType::SHOW_CURSOR_VALUE_TYPE,
         {{"NEVER", (int)Config::ShowCursor::Never},
          {"CONSTANTLY", (int)Config::ShowCursor::Constantly},
          {"ONMOVEMENT", (int)Config::ShowCursor::OnMovement}}},

        {Config::ValueType::LOG_LEVEL_TYPE,
         {{"LNOTICE", (int)Common::Log::LogLevel::LNOTICE},
          {"LERROR", (int)Common::Log::LogLevel::LERROR},
          {"LWARNING", (int)Common::Log::LogLevel::LWARNING},
          {"LINFO", (int)Common::Log::LogLevel::LINFO},
          {"LDEBUG", (int)Common::Log::LogLevel::LDEBUG}}},

        {Config::ValueType::FREE_LOOK_CONTROL_TYPE,
         {{"SIXAXIS", (int)FreeLook::ControlType::SixAxis},
          {"FPS", (int)FreeLook::ControlType::FPS},
          {"ORBITAL", (int)FreeLook::ControlType::Orbital}}},

        {Config::ValueType::ASPECT_MODE,
         {{"AUTO", (int)AspectMode::Auto},
          {"ANALOGWIDE", (int)AspectMode::AnalogWide},
          {"ANALOG", (int)AspectMode::Analog},
          {"STRETCH", (int)AspectMode::Stretch}}},

        {Config::ValueType::SHADER_COMPILATION_MODE,
         {{"SYNCHRONOUS", (int)ShaderCompilationMode::Synchronous},
          {"SYNCHRONOUSUBERSHADERS", (int)ShaderCompilationMode::SynchronousUberShaders},
          {"ASYNCHRONOUSUBERSHADERS", (int)ShaderCompilationMode::AsynchronousUberShaders},
          {"ASYNCHRONOUSSKIPRENDERING", (int)ShaderCompilationMode::AsynchronousSkipRendering}}},

        {Config::ValueType::TRI_STATE,
         {{"OFF", (int)TriState::Off}, {"ON", (int)TriState::On}, {"AUTO", (int)TriState::Auto}}},

        {Config::ValueType::TEXTURE_FILTERING_MODE,
         {{"DEFAULT", (int)TextureFilteringMode::Default},
          {"NEAREST", (int)TextureFilteringMode::Nearest},
          {"LINEAR", (int)TextureFilteringMode::Linear}}},

        {Config::ValueType::STERERO_MODE,
         {{"OFF", (int)StereoMode::Off},
          {"SBS", (int)StereoMode::SBS},
          {"TAB", (int)StereoMode::TAB},
          {"ANAGLYPH", (int)StereoMode::Anaglyph},
          {"QUADBUFFER", (int)StereoMode::QuadBuffer},
          {"PASSIVE", (int)StereoMode::Passive}}},

        {Config::ValueType::WIIMOTE_SOURCE,
         {{"NONE", (int)WiimoteSource::None},
          {"EMULATED", (int)WiimoteSource::Emulated},
          {"REAL", (int)WiimoteSource::Real}}}

};

static std::map<Config::ValueType, std::map<int, std::string>>
ConvertMapFromStringToInt(const std::map<Config::ValueType, std::map<std::string, int>>& input_map)
{
  std::map<Config::ValueType, std::map<int, std::string>> return_map =
      std::map<Config::ValueType, std::map<int, std::string>>();
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

static std::map<Config::ValueType, std::map<int, std::string>> map_of_enum_type_int_to_string =
    ConvertMapFromStringToInt(map_of_enum_type_from_string_to_int);

ClassMetadata GetClassMetadataForVersion(const std::string& api_version)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return {class_name, GetLatestFunctionsForVersion(all_config_functions_metadata_list, api_version,
                                                   deprecated_functions_map)};
}
ClassMetadata GetAllClassMetadata()
{
  return {class_name, GetAllFunctions(all_config_functions_metadata_list)};
}

FunctionMetadata GetFunctionMetadataForVersion(const std::string& api_version,
                                               const std::string& function_name)
{
  std::unordered_map<std::string, std::string> deprecated_functions_map;
  return GetFunctionForVersion(all_config_functions_metadata_list, api_version, function_name,
                               deprecated_functions_map);
}

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
  std::string uppercase_enum_name = ConvertToUpperCase(enum_name);

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
  std::string uppercase_layer_name = ConvertToUpperCase(layer_name);

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
  else
    return {};
}

std::optional<Config::System> ParseSystem(const std::string& system_name)
{
  std::string uppercase_system_name = ConvertToUpperCase(system_name);

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

ArgHolder* GetLayerNames_MostGlobalFirst(ScriptContext* script_context,
                                         std::vector<ArgHolder*>* args_list)
{
  return CreateStringArgHolder(
      "Base, CommandLine, GlobalGame, LocalGame, Movie, Netplay, CurrentRun");
}

ArgHolder* DoesLayerExist(ScriptContext* script_context, std::vector<ArgHolder*>* args_list)
{
  std::optional<Config::LayerType> layer_name = ParseLayer((*args_list)[0]->string_val);
  if (!layer_name.has_value())
    return CreateErrorStringArgHolder("Invalid layer name passed into function.");
  return CreateBoolArgHolder(Config::GetLayer(layer_name.value()) != nullptr);
}

ArgHolder* GetListOfSystems(ScriptContext* script_context, std::vector<ArgHolder*>* args_list)
{
  return CreateStringArgHolder(
      "Main, Sysconf, GCPad, WiiPad, GCKeyboard, GFX, Logger, Debugger, DualShockUDPClient, "
      "FreeLook, Session, GameSettingsOnly, Achievements");
}

ArgHolder* GetConfigEnumTypes(ScriptContext* script_context, std::vector<ArgHolder*>* args_list)
{
  return CreateStringArgHolder(
      "CPUCore, DPL2Quality, EXIDeviceType, SIDeviceType, HSPDeviceType, Region, ShowCursor, "
      "LogLevel, FreeLookControl, AspectMode, ShaderCompilationMode, TriState, "
      "TextureFilteringMode, StereoMode, WiimoteSource");
}

ArgHolder* GetListOfValidValuesForEnumType(ScriptContext* script_context,
                                           std::vector<ArgHolder*>* args_list)
{
  std::optional<Config::ValueType> enum_type = ParseEnumType((*args_list)[0]->string_val);
  if (!enum_type.has_value())
    return CreateErrorStringArgHolder("Unknown enum type passed into function.");
  std::string result_string = "";
  bool first_time_in_loop = true;
  for (auto& it : map_of_enum_type_from_string_to_int[enum_type.value()])
  {
    if (!first_time_in_loop)
      result_string += ", ";
    else
      first_time_in_loop = false;
    result_string += it.first;
  }
  return CreateStringArgHolder(result_string);
}

template <typename T>
ArgHolder* GetConfigSettingForLayer(std::vector<ArgHolder*>* args_list, T default_value)
{
  std::optional<Config::LayerType> layer_name = ParseLayer((*args_list)[0]->string_val);
  std::optional<Config::System> system_name = ParseSystem((*args_list)[1]->string_val);
  std::string section_name = (*args_list)[2]->string_val;
  std::string setting_name = (*args_list)[3]->string_val;

  if (!layer_name.has_value())
    return CreateErrorStringArgHolder("Invalid layer name of " + (*args_list)[0]->string_val +
                                      " was used.");

  else if (Config::GetLayer(layer_name.value()) == nullptr)
    return CreateErrorStringArgHolder("Attempted to get a layer which was not created/active of " +
                                      (*args_list)[0]->string_val);

  else if (!system_name.has_value())
    return CreateErrorStringArgHolder("Invalid system name of " + (*args_list)[1]->string_val +
                                      " was used.");

  std::optional<T> returned_config_val;

  Config::Location location = {system_name.value(), section_name, setting_name};
  returned_config_val = Config::GetLayer(layer_name.value())->Get<T>(location);

  if (!returned_config_val.has_value())
    return CreateEmptyOptionalArgument();
  else
  {
    if (std::is_same<T, bool>::value)
      return CreateBoolArgHolder((*((std::optional<bool>*)&returned_config_val)).value());
    else if (std::is_same<T, int>::value)
      return CreateS32ArgHolder((*((std::optional<int>*)&returned_config_val)).value());
    else if (std::is_same<T, u32>::value)
      return CreateU32ArgHolder((*((std::optional<u32>*)&returned_config_val)).value());
    else if (std::is_same<T, float>::value)
      return CreateFloatArgHolder((*((std::optional<float>*)&returned_config_val)).value());
    else if (std::is_same<T, std::string>::value)
      return CreateStringArgHolder((*((std::optional<std::string>*)&returned_config_val)).value());
    else
      return CreateErrorStringArgHolder("An unknown implementation error occured.");
  }
}

template <typename T>
ArgHolder* GetConfigSettingForLayer_enum(
    std::vector<ArgHolder*>* args_list, T default_value,
    Config::ValueType enum_type)  // unfortunately, this needs to be in a seperate function to
                                  // prevent annoying template errors in the function above.
{
  std::optional<Config::LayerType> layer_name = ParseLayer((*args_list)[0]->string_val);
  std::optional<Config::System> system_name = ParseSystem((*args_list)[1]->string_val);
  std::string section_name = (*args_list)[2]->string_val;
  std::string setting_name = (*args_list)[3]->string_val;

  if (!layer_name.has_value())
    return CreateErrorStringArgHolder("Invalid layer name of " + (*args_list)[0]->string_val +
                                      " was used.");
  else if (Config::GetLayer(layer_name.value()) == nullptr)
    return CreateErrorStringArgHolder("Attempted to get a layer which was not created/active of " +
                                      (*args_list)[0]->string_val);

  else if (!system_name.has_value())
    return CreateErrorStringArgHolder("Invalid system name of " + (*args_list)[1]->string_val +
                                      " was used.");

  std::optional<T> returned_config_val;

  Config::Location location = {system_name.value(), section_name, setting_name};
  returned_config_val = Config::GetLayer(layer_name.value())->Get<T>(location);

  if (!returned_config_val.has_value())
    return CreateEmptyOptionalArgument();
  else
  {
    std::string resulting_enum_string =
        ConvertEnumIntToStringForType(enum_type, (int)returned_config_val.value());
    if (resulting_enum_string.empty())
      return CreateEmptyOptionalArgument();
    else
      return CreateStringArgHolder(resulting_enum_string);
  }
}

ArgHolder* GetBooleanConfigSettingForLayer(ScriptContext* current_script,
                                           std::vector<ArgHolder*>* args_list)
{
  return GetConfigSettingForLayer(args_list, (bool)false);
}

ArgHolder* GetSignedIntConfigSettingForLayer(ScriptContext* current_script,
                                             std::vector<ArgHolder*>* args_list)
{
  return GetConfigSettingForLayer(args_list, (int)0);
}

ArgHolder* GetUnsignedIntConfigSettingForLayer(ScriptContext* current_script,
                                               std::vector<ArgHolder*>* args_list)
{
  return GetConfigSettingForLayer(args_list, (u32)0);
}

ArgHolder* GetFloatConfigSettingForLayer(ScriptContext* current_script,
                                         std::vector<ArgHolder*>* args_list)
{
  return GetConfigSettingForLayer(args_list, (float)0.0);
}

ArgHolder* GetStringConfigSettingForLayer(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list)
{
  return GetConfigSettingForLayer(args_list, std::string());
}

ArgHolder* GetEnumConfigSettingForLayer(ScriptContext* current_script,
                                        std::vector<ArgHolder*>* args_list)
{
  std::string raw_enum_name = (*args_list)[4]->string_val;
  std::optional<Config::ValueType> optional_parsed_enum = ParseEnumType(raw_enum_name);
  if (!optional_parsed_enum.has_value())
    return CreateErrorStringArgHolder("Invalid enum type name passed into function");
  Config::ValueType parsed_enum = optional_parsed_enum.value();
  switch (parsed_enum)
  {
  case Config::ValueType::ASPECT_MODE:
    return GetConfigSettingForLayer_enum(args_list, (AspectMode)AspectMode::Auto, parsed_enum);
  case Config::ValueType::CPU_CORE:
    return GetConfigSettingForLayer_enum(args_list, (PowerPC::CPUCore)PowerPC::CPUCore::JIT64,
                                         parsed_enum);
  case Config::ValueType::DPL2_QUALITY:
    return GetConfigSettingForLayer_enum(
        args_list, (AudioCommon::DPL2Quality)AudioCommon::DPL2Quality::High, parsed_enum);
  case Config::ValueType::EXI_DEVICE_TYPE:
    return GetConfigSettingForLayer_enum(
        args_list, (ExpansionInterface::EXIDeviceType)ExpansionInterface::EXIDeviceType::MemoryCard,
        parsed_enum);
  case Config::ValueType::FREE_LOOK_CONTROL_TYPE:
    return GetConfigSettingForLayer_enum(
        args_list, (FreeLook::ControlType)FreeLook::ControlType::FPS, parsed_enum);
  case Config::ValueType::HSP_DEVICE_TYPE:
    return GetConfigSettingForLayer_enum(args_list, (HSP::HSPDeviceType)HSP::HSPDeviceType::None,
                                         parsed_enum);
  case Config::ValueType::LOG_LEVEL_TYPE:
    return GetConfigSettingForLayer_enum(
        args_list, (Common::Log::LogLevel)Common::Log::LogLevel::LINFO, parsed_enum);
  case Config::ValueType::REGION:
    return GetConfigSettingForLayer_enum(args_list, (DiscIO::Region)DiscIO::Region::NTSC_U,
                                         parsed_enum);
  case Config::ValueType::SHADER_COMPILATION_MODE:
    return GetConfigSettingForLayer_enum(
        args_list, (ShaderCompilationMode)ShaderCompilationMode::Synchronous, parsed_enum);
  case Config::ValueType::SHOW_CURSOR_VALUE_TYPE:
    return GetConfigSettingForLayer_enum(
        args_list, (Config::ShowCursor)Config::ShowCursor::OnMovement, parsed_enum);
  case Config::ValueType::SI_DEVICE_TYPE:
    return GetConfigSettingForLayer_enum(
        args_list, (SerialInterface::SIDevices)SerialInterface::SIDevices::SIDEVICE_GC_CONTROLLER,
        parsed_enum);
  case Config::ValueType::STERERO_MODE:
    return GetConfigSettingForLayer_enum(args_list, (StereoMode)StereoMode::Off, parsed_enum);
  case Config::ValueType::TEXTURE_FILTERING_MODE:
    return GetConfigSettingForLayer_enum(
        args_list, (TextureFilteringMode)TextureFilteringMode::Default, parsed_enum);
  case Config::ValueType::TRI_STATE:
    return GetConfigSettingForLayer_enum(args_list, (TriState)TriState::Auto, parsed_enum);
  case Config::ValueType::WIIMOTE_SOURCE:
    return GetConfigSettingForLayer_enum(args_list, (WiimoteSource)WiimoteSource::None,
                                         parsed_enum);
  default:
    return CreateErrorStringArgHolder(
        "An unknown implementation error occured. Did you add a new enum type to the ConfigAPI?");
  }
}

template <typename T>
ArgHolder* SetConfigSettingForLayer(std::vector<ArgHolder*>* args_list, T new_value)
{
  std::optional<Config::LayerType> layer_name = ParseLayer((*args_list)[0]->string_val);
  std::optional<Config::System> system_name = ParseSystem((*args_list)[1]->string_val);
  std::string section_name = (*args_list)[2]->string_val;
  std::string setting_name = (*args_list)[3]->string_val;

  if (!layer_name.has_value())
    return CreateErrorStringArgHolder("Invalid layer name of " + (*args_list)[0]->string_val +
                                      " was used.");

  else if (Config::GetLayer(layer_name.value()) == nullptr)
    return CreateErrorStringArgHolder("Attempted to get a layer which was not created/active of " +
                                      (*args_list)[0]->string_val);
  else if (!system_name.has_value())
    return CreateErrorStringArgHolder("Invalid system name of " + (*args_list)[1]->string_val +
                                      " was used.");

  else
  {
    Config::Location location = {system_name.value(), section_name, setting_name};
    Config::Set<T>(layer_name.value(), {location, new_value}, new_value);
  }

  return CreateVoidTypeArgHolder();
}

ArgHolder* SetBooleanConfigSettingForLayer(ScriptContext* current_script,
                                           std::vector<ArgHolder*>* args_list)
{
  bool new_bool = (*args_list)[4]->bool_val;
  return SetConfigSettingForLayer(args_list, new_bool);
}

ArgHolder* SetSignedIntConfigSettingForLayer(ScriptContext* current_script,
                                             std::vector<ArgHolder*>* args_list)
{
  int new_int = (*args_list)[4]->s32_val;
  return SetConfigSettingForLayer(args_list, new_int);
}

ArgHolder* SetUnsignedIntConfigSettingForLayer(ScriptContext* current_script,
                                               std::vector<ArgHolder*>* args_list)
{
  u32 new_u32 = (*args_list)[4]->u32_val;
  return SetConfigSettingForLayer(args_list, new_u32);
}

ArgHolder* SetFloatConfigSettingForLayer(ScriptContext* current_script,
                                         std::vector<ArgHolder*>* args_list)
{
  float new_float = (*args_list)[4]->float_val;
  return SetConfigSettingForLayer(args_list, new_float);
}

ArgHolder* SetStringConfigSettingForLayer(ScriptContext* current_script,
                                          std::vector<ArgHolder*>* args_list)
{
  std::string new_string = (*args_list)[4]->string_val;
  return SetConfigSettingForLayer(args_list, new_string);
}

ArgHolder* SetEnumConfigSettingForLayer(ScriptContext* current_script,
                                        std::vector<ArgHolder*>* args_list)
{
  std::optional<Config::ValueType> optional_enum_type = ParseEnumType((*args_list)[4]->string_val);
  if (!optional_enum_type.has_value())
    return CreateErrorStringArgHolder("Invalid enum type passed into function.");
  Config::ValueType enum_type = optional_enum_type.value();
  int new_value =
      ConvertEnumStringToIntForType(enum_type, ConvertToUpperCase((*args_list)[5]->string_val));
  if (new_value == -1)
    return CreateErrorStringArgHolder("Undefined enum value passed in for type " +
                                      (*args_list)[4]->string_val);

  switch (enum_type)
  {
  case Config::ValueType::ASPECT_MODE:
    return SetConfigSettingForLayer(args_list, ((AspectMode)new_value));
  case Config::ValueType::CPU_CORE:
    return SetConfigSettingForLayer(args_list, ((PowerPC::CPUCore)new_value));
  case Config::ValueType::DPL2_QUALITY:
    return SetConfigSettingForLayer(args_list, ((AudioCommon::DPL2Quality)new_value));
  case Config::ValueType::EXI_DEVICE_TYPE:
    return SetConfigSettingForLayer(args_list, ((ExpansionInterface::EXIDeviceType)new_value));
  case Config::ValueType::FREE_LOOK_CONTROL_TYPE:
    return SetConfigSettingForLayer(args_list, ((FreeLook::ControlType)new_value));
  case Config::ValueType::HSP_DEVICE_TYPE:
    return SetConfigSettingForLayer(args_list, ((HSP::HSPDeviceType)new_value));
  case Config::ValueType::LOG_LEVEL_TYPE:
    return SetConfigSettingForLayer(args_list, ((Common::Log::LogLevel)new_value));
  case Config::ValueType::REGION:
    return SetConfigSettingForLayer(args_list, ((DiscIO::Region)new_value));
  case Config::ValueType::SHADER_COMPILATION_MODE:
    return SetConfigSettingForLayer(args_list, ((ShaderCompilationMode)new_value));
  case Config::ValueType::SHOW_CURSOR_VALUE_TYPE:
    return SetConfigSettingForLayer(args_list, ((Config::ShowCursor)new_value));
  case Config::ValueType::SI_DEVICE_TYPE:
    return SetConfigSettingForLayer(args_list, ((SerialInterface::SIDevices)new_value));
  case Config::ValueType::STERERO_MODE:
    return SetConfigSettingForLayer(args_list, ((StereoMode)new_value));
  case Config::ValueType::TEXTURE_FILTERING_MODE:
    return SetConfigSettingForLayer(args_list, ((TextureFilteringMode)new_value));
  case Config::ValueType::TRI_STATE:
    return SetConfigSettingForLayer(args_list, ((TriState)new_value));
  case Config::ValueType::WIIMOTE_SOURCE:
    return SetConfigSettingForLayer(args_list, ((WiimoteSource)new_value));
  default:
    return CreateErrorStringArgHolder(
        "Unknown implementation error occured in SetEnumConfigSettingForLayer() function. Did you "
        "add in a new type and forget to update the function?");
  }
}

template <typename T>
ArgHolder* DeleteConfigSettingFromLayer(std::vector<ArgHolder*>* args_list, T default_value)
{
  std::optional<Config::LayerType> layer_name = ParseLayer((*args_list)[0]->string_val);
  std::optional<Config::System> system_name = ParseSystem((*args_list)[1]->string_val);
  std::string section_name = (*args_list)[2]->string_val;
  std::string setting_name = (*args_list)[3]->string_val;

  if (!layer_name.has_value())
    return CreateErrorStringArgHolder("Invalid layer name of " + (*args_list)[0]->string_val +
                                      " was used.");

  else if (Config::GetLayer(layer_name.value()) == nullptr)
    return CreateErrorStringArgHolder("Attempted to get a layer which was not created/active of " +
                                      (*args_list)[0]->string_val);
  else if (!system_name.has_value())
    return CreateErrorStringArgHolder("Invalid system name of " + (*args_list)[1]->string_val +
                                      " was used.");

  bool return_value = Config::GetLayer(layer_name.value())
                          ->DeleteKey({system_name.value(), section_name, setting_name});
  if (return_value)
    Config::OnConfigChanged();
  return CreateBoolArgHolder(return_value);
}

ArgHolder* DeleteBooleanConfigSettingFromLayer(ScriptContext* current_script,
                                               std::vector<ArgHolder*>* args_list)
{
  return DeleteConfigSettingFromLayer(args_list, (bool)false);
}

ArgHolder* DeleteSignedIntConfigSettingFromLayer(ScriptContext* current_script,
                                                 std::vector<ArgHolder*>* args_list)
{
  return DeleteConfigSettingFromLayer(args_list, (int)0);
}

ArgHolder* DeleteUnsignedIntConfigSettingFromLayer(ScriptContext* current_script,
                                                   std::vector<ArgHolder*>* args_list)
{
  return DeleteConfigSettingFromLayer(args_list, (u32)0);
}

ArgHolder* DeleteFloatConfigSettingFromLayer(ScriptContext* current_script,
                                             std::vector<ArgHolder*>* args_list)
{
  return DeleteConfigSettingFromLayer(args_list, (float)0.0f);
}

ArgHolder* DeleteStringConfigSettingFromLayer(ScriptContext* current_script,
                                              std::vector<ArgHolder*>* args_list)
{
  return DeleteConfigSettingFromLayer(args_list, std::string(""));
}

ArgHolder* DeleteEnumConfigSettingFromLayer(ScriptContext* current_script,
                                            std::vector<ArgHolder*>* args_list)
{
  std::optional<Config::ValueType> optional_enum_type = ParseEnumType((*args_list)[4]->string_val);
  if (!optional_enum_type.has_value())
    return CreateErrorStringArgHolder("Invalid enum type passed into function.");
  Config::ValueType enum_type = optional_enum_type.value();

  switch (enum_type)
  {
  case Config::ValueType::ASPECT_MODE:
    return DeleteConfigSettingFromLayer(args_list, ((AspectMode)0));
  case Config::ValueType::CPU_CORE:
    return DeleteConfigSettingFromLayer(args_list, ((PowerPC::CPUCore)0));
  case Config::ValueType::DPL2_QUALITY:
    return DeleteConfigSettingFromLayer(args_list, ((AudioCommon::DPL2Quality)0));
  case Config::ValueType::EXI_DEVICE_TYPE:
    return DeleteConfigSettingFromLayer(args_list, ((ExpansionInterface::EXIDeviceType)0));
  case Config::ValueType::FREE_LOOK_CONTROL_TYPE:
    return DeleteConfigSettingFromLayer(args_list, ((FreeLook::ControlType)0));
  case Config::ValueType::HSP_DEVICE_TYPE:
    return DeleteConfigSettingFromLayer(args_list, ((HSP::HSPDeviceType)0));
  case Config::ValueType::LOG_LEVEL_TYPE:
    return DeleteConfigSettingFromLayer(args_list, ((Common::Log::LogLevel)0));
  case Config::ValueType::REGION:
    return DeleteConfigSettingFromLayer(args_list, ((DiscIO::Region)0));
  case Config::ValueType::SHADER_COMPILATION_MODE:
    return DeleteConfigSettingFromLayer(args_list, ((ShaderCompilationMode)0));
  case Config::ValueType::SHOW_CURSOR_VALUE_TYPE:
    return DeleteConfigSettingFromLayer(args_list, ((Config::ShowCursor)0));
  case Config::ValueType::SI_DEVICE_TYPE:
    return DeleteConfigSettingFromLayer(args_list, ((SerialInterface::SIDevices)0));
  case Config::ValueType::STERERO_MODE:
    return DeleteConfigSettingFromLayer(args_list, ((StereoMode)0));
  case Config::ValueType::TEXTURE_FILTERING_MODE:
    return DeleteConfigSettingFromLayer(args_list, ((TextureFilteringMode)0));
  case Config::ValueType::TRI_STATE:
    return DeleteConfigSettingFromLayer(args_list, ((TriState)0));
  case Config::ValueType::WIIMOTE_SOURCE:
    return DeleteConfigSettingFromLayer(args_list, ((WiimoteSource)0));
  default:
    return CreateErrorStringArgHolder("Unknown implementation error occured in "
                                      "DeleteEnumConfigSettingFromLayer() function. Did you "
                                      "add in a new type and forget to update the function?");
  }
}

template <typename T>
ArgHolder* GetConfigSetting(std::vector<ArgHolder*>* args_list, T default_value)
{
  std::optional<Config::System> system_name = ParseSystem((*args_list)[0]->string_val);
  std::string section_name = (*args_list)[1]->string_val;
  std::string setting_name = (*args_list)[2]->string_val;

  if (!system_name.has_value())
    return CreateErrorStringArgHolder("Invalid system name of " + (*args_list)[0]->string_val +
                                      " was used.");

  Config::Location location = {system_name.value(), section_name, setting_name};

  std::optional<T> returned_config_val = {};
  for (auto& layerType : Config::SEARCH_ORDER)
  {
    std::shared_ptr<Config::Layer> layer_ptr = Config::GetLayer(layerType);
    if (layer_ptr != nullptr)
    {
      returned_config_val = layer_ptr->Get<T>(location);
      if (returned_config_val.has_value())
      {
        if (std::is_same<T, bool>::value)
          return CreateBoolArgHolder((*((std::optional<bool>*)&returned_config_val)).value());
        else if (std::is_same<T, int>::value)
          return CreateS32ArgHolder((*((std::optional<int>*)&returned_config_val)).value());
        else if (std::is_same<T, u32>::value)
          return CreateU32ArgHolder((*((std::optional<u32>*)&returned_config_val)).value());
        else if (std::is_same<T, float>::value)
          return CreateFloatArgHolder((*((std::optional<float>*)&returned_config_val)).value());
        else if (std::is_same<T, std::string>::value)
          return CreateStringArgHolder(
              (*((std::optional<std::string>*)&returned_config_val)).value());
        else
          return CreateErrorStringArgHolder("An unknown implementation error occured.");
      }
    }
  }
  return CreateEmptyOptionalArgument();
}

template <typename T>
ArgHolder* GetConfigSetting_enum(
    std::vector<ArgHolder*>* args_list, T default_value,
    Config::ValueType enum_type)  // unfortunately, this needs to be in a seperate function to
                                  // prevent annoying template errors in the function above.
{
  std::optional<Config::System> system_name = ParseSystem((*args_list)[0]->string_val);
  std::string section_name = (*args_list)[1]->string_val;
  std::string setting_name = (*args_list)[2]->string_val;

  if (!system_name.has_value())
    return CreateErrorStringArgHolder("Invalid system name of " + (*args_list)[0]->string_val +
                                      " was used.");

  Config::Location location = {system_name.value(), section_name, setting_name};

  std::optional<T> returned_config_val = {};

  for (auto& layerType : Config::SEARCH_ORDER)
  {
    std::shared_ptr<Config::Layer> layer_ptr = Config::GetLayer(layerType);
    if (layer_ptr != nullptr)
    {
      returned_config_val = layer_ptr->Get<T>(location);
      if (returned_config_val.has_value())
      {
        std::string resulting_enum_string =
            ConvertEnumIntToStringForType(enum_type, (int)returned_config_val.value());
        if (resulting_enum_string.empty())
          return CreateEmptyOptionalArgument();
        else
          return CreateStringArgHolder(resulting_enum_string);
      }
    }
  }
  return CreateEmptyOptionalArgument();
}

ArgHolder* GetBooleanConfigSetting(ScriptContext* current_script,
                                   std::vector<ArgHolder*>* args_list)
{
  return GetConfigSetting(args_list, (bool)false);
}

ArgHolder* GetSignedIntConfigSetting(ScriptContext* current_script,
                                     std::vector<ArgHolder*>* args_list)
{
  return GetConfigSetting(args_list, (int)0);
}

ArgHolder* GetUnsignedIntConfigSetting(ScriptContext* current_script,
                                       std::vector<ArgHolder*>* args_list)
{
  return GetConfigSetting(args_list, (u32)0);
}

ArgHolder* GetFloatConfigSetting(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return GetConfigSetting(args_list, (float)0.0f);
}

ArgHolder* GetStringConfigSetting(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  return GetConfigSetting(args_list, std::string());
}

ArgHolder* GetEnumConfigSetting(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  std::string raw_enum_name = (*args_list)[3]->string_val;
  std::optional<Config::ValueType> optional_parsed_enum = ParseEnumType(raw_enum_name);
  if (!optional_parsed_enum.has_value())
    return CreateErrorStringArgHolder("Invalid enum type name passed into function");
  Config::ValueType parsed_enum = optional_parsed_enum.value();
  switch (parsed_enum)
  {
  case Config::ValueType::ASPECT_MODE:
    return GetConfigSetting_enum(args_list, (AspectMode)AspectMode::Auto, parsed_enum);
  case Config::ValueType::CPU_CORE:
    return GetConfigSetting_enum(args_list, (PowerPC::CPUCore)PowerPC::CPUCore::JIT64, parsed_enum);
  case Config::ValueType::DPL2_QUALITY:
    return GetConfigSetting_enum(
        args_list, (AudioCommon::DPL2Quality)AudioCommon::DPL2Quality::High, parsed_enum);
  case Config::ValueType::EXI_DEVICE_TYPE:
    return GetConfigSetting_enum(
        args_list, (ExpansionInterface::EXIDeviceType)ExpansionInterface::EXIDeviceType::MemoryCard,
        parsed_enum);
  case Config::ValueType::FREE_LOOK_CONTROL_TYPE:
    return GetConfigSetting_enum(args_list, (FreeLook::ControlType)FreeLook::ControlType::FPS,
                                 parsed_enum);
  case Config::ValueType::HSP_DEVICE_TYPE:
    return GetConfigSetting_enum(args_list, (HSP::HSPDeviceType)HSP::HSPDeviceType::None,
                                 parsed_enum);
  case Config::ValueType::LOG_LEVEL_TYPE:
    return GetConfigSetting_enum(args_list, (Common::Log::LogLevel)Common::Log::LogLevel::LINFO,
                                 parsed_enum);
  case Config::ValueType::REGION:
    return GetConfigSetting_enum(args_list, (DiscIO::Region)DiscIO::Region::NTSC_U, parsed_enum);
  case Config::ValueType::SHADER_COMPILATION_MODE:
    return GetConfigSetting_enum(
        args_list, (ShaderCompilationMode)ShaderCompilationMode::Synchronous, parsed_enum);
  case Config::ValueType::SHOW_CURSOR_VALUE_TYPE:
    return GetConfigSetting_enum(args_list, (Config::ShowCursor)Config::ShowCursor::OnMovement,
                                 parsed_enum);
  case Config::ValueType::SI_DEVICE_TYPE:
    return GetConfigSetting_enum(
        args_list, (SerialInterface::SIDevices)SerialInterface::SIDevices::SIDEVICE_GC_CONTROLLER,
        parsed_enum);
  case Config::ValueType::STERERO_MODE:
    return GetConfigSetting_enum(args_list, (StereoMode)StereoMode::Off, parsed_enum);
  case Config::ValueType::TEXTURE_FILTERING_MODE:
    return GetConfigSetting_enum(args_list, (TextureFilteringMode)TextureFilteringMode::Default,
                                 parsed_enum);
  case Config::ValueType::TRI_STATE:
    return GetConfigSetting_enum(args_list, (TriState)TriState::Auto, parsed_enum);
  case Config::ValueType::WIIMOTE_SOURCE:
    return GetConfigSetting_enum(args_list, (WiimoteSource)WiimoteSource::None, parsed_enum);
  default:
    return CreateErrorStringArgHolder(
        "An unknown implementation error occured. Did you add a new enum type to the ConfigAPI?");
  }
}

ArgHolder* SaveConfigSettings(ScriptContext* current_script, std::vector<ArgHolder*>* args_list)
{
  Config::Save();
  return CreateVoidTypeArgHolder();
}

}  // namespace Scripting::ConfigAPI
