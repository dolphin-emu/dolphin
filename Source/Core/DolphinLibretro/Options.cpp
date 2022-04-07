
#include <libretro.h>

#include "DolphinLibretro/Options.h"

namespace Libretro
{
namespace Options
{
static std::vector<retro_variable> optionsList;
static std::vector<bool*> dirtyPtrList;

template <typename T>
void Option<T>::Register()
{
  if (!m_options.empty())
    return;

  m_options = m_name;
  m_options.push_back(';');
  for (auto& option : m_list)
  {
    if (option.first == m_list.begin()->first)
      m_options += std::string(" ") + option.first;
    else
      m_options += std::string("|") + option.first;
  }
  optionsList.push_back({m_id, m_options.c_str()});
  dirtyPtrList.push_back(&m_dirty);
  Updated();
  m_dirty = true;
}

void SetVariables()
{
  if (optionsList.empty())
    return;
  if (optionsList.back().key)
    optionsList.push_back({});
  environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)optionsList.data());
}

void CheckVariables()
{
  bool updated = false;
  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && !updated)
    return;

  for (bool* ptr : dirtyPtrList)
    *ptr = true;
}

template <typename T>
Option<T>::Option(const char* id, const char* name,
                  std::initializer_list<std::pair<const char*, T>> list)
    : m_id(id), m_name(name), m_list(list.begin(), list.end())
{
  Register();
}

template <typename T>
Option<T>::Option(const char* id, const char* name, std::initializer_list<const char*> list)
    : m_id(id), m_name(name)
{
  for (auto option : list)
    m_list.push_back({option, (T)m_list.size()});
  Register();
}
template <>
Option<std::string>::Option(const char* id, const char* name,
                            std::initializer_list<const char*> list)
    : m_id(id), m_name(name)
{
  for (auto option : list)
    m_list.push_back({option, option});
  Register();
}
template <>
Option<const char*>::Option(const char* id, const char* name,
                            std::initializer_list<const char*> list)
    : m_id(id), m_name(name)
{
  for (auto option : list)
    m_list.push_back({option, option});
  Register();
}

template <typename T>
Option<T>::Option(const char* id, const char* name, T first,
                  std::initializer_list<const char*> list)
    : m_id(id), m_name(name)
{
  for (auto option : list)
    m_list.push_back({option, first + (int)m_list.size()});
  Register();
}

template <typename T>
Option<T>::Option(const char* id, const char* name, T first, int count, int step)
    : m_id(id), m_name(name)
{
  for (T i = first; i < first + count; i += step)
    m_list.push_back({std::to_string(i), i});
  Register();
}

template <>
Option<bool>::Option(const char* id, const char* name, bool initial) : m_id(id), m_name(name)
{
  m_list.push_back({initial ? "enabled" : "disabled", initial});
  m_list.push_back({!initial ? "enabled" : "disabled", !initial});
  Register();
}

Option<std::string> renderer("dolphin_renderer", "Renderer", {"Hardware", "Software"
#if defined(_DEBUG) || defined(DEBUGFAST)
    , "Null"
#endif
});
Option<int> efbScale("dolphin_efb_scale", "Internal Resolution", 1,
                     {"x1 (640 x 528)", "x2 (1280 x 1056)", "x3 (1920 x 1584)", "x4 (2560 x 2112)",
                      "x5 (3200 x 2640)", "x6 (3840 x 3168)"});
Option<bool> Widescreen("dolphin_widescreen", "Widescreen (Wii)", true);
Option<bool> WidescreenHack("dolphin_widescreen_hack", "WideScreen Hack", false);
Option<ShaderCompilationMode> shaderCompilationMode(
    "dolphin_shader_compilation_mode", "Shader Compilation Mode",
    {{"sync", ShaderCompilationMode::Synchronous},
     {"a-sync Skip Rendering", ShaderCompilationMode::AsynchronousSkipRendering},
     {"sync UberShaders", ShaderCompilationMode::SynchronousUberShaders},
     {"a-sync UberShaders", ShaderCompilationMode::AsynchronousUberShaders}});
Option<bool> waitForShaders("dolphin_wait_for_shaders", "Wait for Shaders before Starting", false);
Option<bool> progressiveScan("dolphin_progressive_scan", "Progressive Scan", true);
Option<bool> pal60("dolphin_pal60", "PAL60", true);
Option<int> maxAnisotropy("dolphin_max_anisotropy", "Max Anisotropy", {"1x", "2x", "4x", "8x", "16x"});
Option<bool> skipDupeFrames("dolphin_skip_dupe_frames", "Skip Presenting Duplicate Frames", true);
Option<bool> efbScaledCopy("dolphin_efb_scaled_copy", "Scaled EFB Copy", true);
Option<bool> forceTextureFiltering("dolphin_force_texture_filtering", "Force Texture Filtering", false);
Option<bool> efbToTexture("dolphin_efb_to_texture", "Store EFB Copies on GPU", true);
Option<int> textureCacheAccuracy("dolphin_texture_cache_accuracy", "Texture Cache Accuracy",
                                 {{"Fast", 128}, {"Middle", 512}, {"Safe", 0}});
Option<bool> gpuTextureDecoding("dolphin_gpu_texture_decoding", "GPU Texture Decoding", false);
Option<bool> fastDepthCalc("dolphin_fast_depth_calculation", "Fast Depth Calculation", true);
Option<bool> bboxEnabled("dolphin_bbox_enabled", "Bounding Box Emulation", false);
Option<bool> efbToVram("dolphin_efb_to_vram", "Disable EFB to VRAM", false);
Option<bool> loadCustomTextures("dolphin_load_custom_textures", "Load Custom Textures", false);
Option<PowerPC::CPUCore> cpu_core("dolphin_cpu_core", "CPU Core",
                                  {
#ifdef _M_X86
                                  {"JIT64", PowerPC::CPUCore::JIT64},
#elif _M_ARM_64
                                  {"JITARM64", PowerPC::CPUCore::JITARM64},
#endif
                                  {"Interpreter", PowerPC::CPUCore::Interpreter},
                                  {"Cached Interpreter", PowerPC::CPUCore::CachedInterpreter}});
Option<float> cpuClockRate("dolphin_cpu_clock_rate", "CPU Clock Rate",
                           {{"100%", 1.0},
                            {"150%", 1.5},
                            {"200%", 2.0},
                            {"250%", 2.5},
                            {"300%", 3.0},
                            {"5%", 0.05},
                            {"10%", 0.1},
                            {"20%", 0.2},
                            {"30%", 0.3},
                            {"40%", 0.4},
                            {"50%", 0.5},
                            {"60%", 0.6},
                            {"70%", 0.7},
                            {"80%", 0.8},
                            {"90%", 0.9}});
Option<bool> fastmem("dolphin_fastmem", "Fastmem", true);
Option<int> irMode("dolphin_ir_mode", "Wiimote IR Mode", 1,
    {"Right Stick controls pointer (relative)",
     "Right Stick controls pointer (absolute)",
     "Mouse controls pointer"});
Option<int> irCenter("dolphin_ir_offset", "Wiimote IR Vertical Offset",
    {{"10", 10}, {"11", 11}, {"12", 12}, {"13", 13}, {"14", 14}, {"15", 15}, {"16", 16}, {"17", 17}, {"18", 18}, {"19", 19},
     {"20", 20}, {"21", 21}, {"22", 22}, {"23", 23}, {"24", 24}, {"25", 25}, {"26", 26}, {"27", 27}, {"28", 28}, {"29", 29},
     {"30", 30}, {"31", 31}, {"32", 32}, {"33", 33}, {"34", 34}, {"35", 35}, {"36", 36}, {"37", 37}, {"38", 38}, {"39", 39},
     {"40", 40}, {"41", 41}, {"42", 42}, {"43", 43}, {"44", 44}, {"45", 45}, {"46", 46}, {"47", 47}, {"48", 48}, {"49", 49},
     {"50", 50}, {"-50", -50}, {"-49", -49}, {"-48", -48}, {"-47", -47}, {"-46", -46}, {"-45", -45}, {"-44", -44}, {"-43", -43},
     {"-42", -42}, {"-41", -41}, {"-40", -40}, {"-39", -39}, {"-38", -38}, {"-37", -37}, {"-36", -36}, {"-35", -35}, {"-34", -34},
     {"-33", -33}, {"-32", -32}, {"-31", -31}, {"-30", -30}, {"-29", -29}, {"-28", -28}, {"-27", -27}, {"-26", -26}, {"-25", -25},
     {"-24", -24}, {"-23", -23}, {"-22", -22}, {"-21", -21}, {"-20", -20}, {"-19", -19}, {"-18", -18}, {"-17", -17}, {"-16", -16},
     {"-15", -15}, {"-14", -14}, {"-13", -13}, {"-12", -12}, {"-11", -11}, {"-10", -10}, {"-9", -9}, {"-8", -8}, {"-7", -7},
     {"-6", -6}, {"-5", -5}, {"-4", -4}, {"-3", -3}, {"-2", -2}, {"-1", -1}, {"0", 0}, {"1", 1}, {"2", 2}, {"3", 3}, {"4", 4},
     {"5", 5}, {"6", 6}, {"7", 7}, {"8", 8}, {"9", 9}});
Option<int> irWidth("dolphin_ir_yaw", "Wiimote IR Total Yaw",
    {{"15", 15}, {"16", 16}, {"17", 17}, {"18", 18}, {"19", 19}, {"20", 20}, {"21", 21}, {"22", 22}, {"23", 23}, {"24", 24},
     {"25", 25}, {"26", 26}, {"27", 27}, {"28", 28}, {"29", 29}, {"30", 30}, {"31", 31}, {"32", 32}, {"33", 33}, {"34", 34},
     {"35", 35}, {"36", 36}, {"37", 37}, {"38", 38}, {"39", 39}, {"40", 40}, {"41", 41}, {"42", 42}, {"43", 43}, {"44", 44},
     {"45", 45}, {"46", 46}, {"47", 47}, {"48", 48}, {"49", 49}, {"50", 50}, {"51", 51}, {"52", 52}, {"53", 53}, {"54", 54},
     {"55", 55}, {"56", 56}, {"57", 57}, {"58", 58}, {"59", 59}, {"60", 60}, {"61", 61}, {"62", 62}, {"63", 63}, {"64", 64},
     {"65", 65}, {"66", 66}, {"67", 67}, {"68", 68}, {"69", 69}, {"70", 70}, {"71", 71}, {"72", 72}, {"73", 73}, {"74", 74},
     {"75", 75}, {"76", 76}, {"77", 77}, {"78", 78}, {"79", 79}, {"80", 80}, {"81", 81}, {"82", 82}, {"83", 83}, {"84", 84},
     {"85", 85}, {"86", 86}, {"87", 87}, {"88", 88}, {"89", 89}, {"90", 90}, {"91", 91}, {"92", 92}, {"93", 93}, {"94", 94},
     {"95", 95}, {"96", 96}, {"97", 97}, {"98", 98}, {"99", 99}, {"100", 100}, {"0", 0}, {"1", 1}, {"2", 2}, {"3", 3},
     {"4", 4}, {"5", 5}, {"6", 6}, {"7", 7}, {"8", 8}, {"9", 9}, {"10", 10}, {"11", 11}, {"12", 12}, {"13", 13}, {"14", 14}});
Option<int> irHeight("dolphin_ir_pitch", "Wiimote IR Total Pitch",
    {{"15", 15}, {"16", 16}, {"17", 17}, {"18", 18}, {"19", 19}, {"20", 20}, {"21", 21}, {"22", 22}, {"23", 23}, {"24", 24},
     {"25", 25}, {"26", 26}, {"27", 27}, {"28", 28}, {"29", 29}, {"30", 30}, {"31", 31}, {"32", 32}, {"33", 33}, {"34", 34},
     {"35", 35}, {"36", 36}, {"37", 37}, {"38", 38}, {"39", 39}, {"40", 40}, {"41", 41}, {"42", 42}, {"43", 43}, {"44", 44},
     {"45", 45}, {"46", 46}, {"47", 47}, {"48", 48}, {"49", 49}, {"50", 50}, {"51", 51}, {"52", 52}, {"53", 53}, {"54", 54},
     {"55", 55}, {"56", 56}, {"57", 57}, {"58", 58}, {"59", 59}, {"60", 60}, {"61", 61}, {"62", 62}, {"63", 63}, {"64", 64},
     {"65", 65}, {"66", 66}, {"67", 67}, {"68", 68}, {"69", 69}, {"70", 70}, {"71", 71}, {"72", 72}, {"73", 73}, {"74", 74},
     {"75", 75}, {"76", 76}, {"77", 77}, {"78", 78}, {"79", 79}, {"80", 80}, {"81", 81}, {"82", 82}, {"83", 83}, {"84", 84},
     {"85", 85}, {"86", 86}, {"87", 87}, {"88", 88}, {"89", 89}, {"90", 90}, {"91", 91}, {"92", 92}, {"93", 93}, {"94", 94},
     {"95", 95}, {"96", 96}, {"97", 97}, {"98", 98}, {"99", 99}, {"100", 100}, {"0", 0}, {"1", 1}, {"2", 2}, {"3", 3},
     {"4", 4}, {"5", 5}, {"6", 6}, {"7", 7}, {"8", 8}, {"9", 9}, {"10", 10}, {"11", 11}, {"12", 12}, {"13", 13}, {"14", 14}});
Option<bool> enableRumble("dolphin_enable_rumble", "Rumble", true);
Option<u32> sensorBarPosition("dolphin_sensor_bar_position", "Sensor Bar Position",
                              {"Bottom", "Top"});
Option<bool> WiimoteContinuousScanning("dolphin_wiimote_continuous_scanning", "Wiimote Continuous Scanning", false);
Option<bool> altGCPorts("dolphin_alt_gc_ports_on_wii", "Use ports 5-8 for GameCube controllers in Wii mode", false);
Option<unsigned int> audioMixerRate("dolphin_mixer_rate", "Audio Mixer Rate",
                                    {{"32000", 32000u}, {"48000", 48000u}});
Option<bool> DSPHLE("dolphin_dsp_hle", "DSP HLE", true);
Option<bool> DSPEnableJIT("dolphin_dsp_jit", "DSP Enable JIT", true);
Option<DiscIO::Language> Language("dolphin_language", "Language",
                                  {{"English", DiscIO::Language::English},
                                   {"Japanese", DiscIO::Language::Japanese},
                                   {"German", DiscIO::Language::German},
                                   {"French", DiscIO::Language::French},
                                   {"Spanish", DiscIO::Language::Spanish},
                                   {"Italian", DiscIO::Language::Italian},
                                   {"Dutch", DiscIO::Language::Dutch},
                                   {"Simplified Chinese", DiscIO::Language::SimplifiedChinese},
                                   {"Traditional Chinese", DiscIO::Language::TraditionalChinese},
                                   {"Korean", DiscIO::Language::Korean}});
Option<bool> cheatsEnabled("dolphin_cheats_enabled", "Internal Cheats Enabled", false);
Option<bool> osdEnabled("dolphin_osd_enabled", "OSD Enabled", true);
Option<Common::Log::LOG_LEVELS> logLevel("dolphin_log_level", "Log Level", {
                                         {"Info", Common::Log::LINFO},
#if defined(_DEBUG) || defined(DEBUGFAST)
                                         {"Debug", Common::Log::LDEBUG},
#endif
                                         {"Notice", Common::Log::LNOTICE},
                                         {"Error", Common::Log::LERROR},
                                         {"Warning", Common::Log::LWARNING}});
}  // namespace Options
}  // namespace Libretro
