#pragma once

#include <cassert>
#include <string>
#include <vector>

#include "Common/Logging/Log.h"
#include "VideoCommon/VideoConfig.h"
#include "Core/PowerPC/PowerPC.h"
#include "DiscIO/Enums.h"

namespace Libretro
{
namespace Options
{
void SetVariables();
void CheckVariables();
void Register(const char* id, const char* desc, bool* dirtyPtr);

template <typename T>
class Option
{
public:
  Option(const char* id, const char* name, std::initializer_list<std::pair<const char*, T>> list);
  Option(const char* id, const char* name, std::initializer_list<const char*> list);
  Option(const char* id, const char* name, T first, std::initializer_list<const char*> list);
  Option(const char* id, const char* name, T first, int count, int step = 1);
  Option(const char* id, const char* name, bool initial);

  bool Updated();

  operator T()
  {
    Updated();
    return m_value;
  }

  template <typename S>
  bool operator==(S value)
  {
    return (T)(*this) == value;
  }

  template <typename S>
  bool operator!=(S value)
  {
    return (T)(*this) != value;
  }

private:
  void Register();

  const char* m_id;
  const char* m_name;
  T m_value;
  bool m_dirty = true;
  std::string m_options;
  std::vector<std::pair<std::string, T>> m_list;
};

extern Option<int> efbScale;
extern Option<LogTypes::LOG_LEVELS> logLevel;
extern Option<float> cpuClockRate;
extern Option<std::string> renderer;
extern Option<bool> fastmem;
extern Option<bool> DSPHLE;
extern Option<bool> DSPEnableJIT;
extern Option<PowerPC::CPUCore> cpu_core;
extern Option<DiscIO::Language> Language;
extern Option<bool> Widescreen;
extern Option<bool> WidescreenHack;
extern Option<bool> progressiveScan;
extern Option<bool> pal60;
extern Option<u32> sensorBarPosition;
extern Option<unsigned int> audioMixerRate;
extern Option<ShaderCompilationMode> shaderCompilationMode;
extern Option<int> maxAnisotropy;
extern Option<bool> efbScaledCopy;
extern Option<bool> gpuTextureDecoding;
extern Option<bool> waitForShaders;
}  // namespace Options
}  // namespace Libretro
