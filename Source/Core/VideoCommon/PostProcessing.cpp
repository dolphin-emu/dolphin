// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoConfig.h"

static const char s_default_shader[] = "void main() { SetOutput(Sample()); }\n";

PostProcessingShaderImplementation::PostProcessingShaderImplementation()
{
  m_timer.Start();
}

PostProcessingShaderImplementation::~PostProcessingShaderImplementation()
{
  m_timer.Stop();
}

static std::vector<std::string> GetShaders(const std::string& sub_dir = "")
{
  std::vector<std::string> paths =
      Common::DoFileSearch({File::GetUserPath(D_SHADERS_IDX) + sub_dir,
                            File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir},
                           {".glsl"});
  std::vector<std::string> result;
  for (std::string path : paths)
  {
    std::string name;
    SplitPath(path, nullptr, &name, nullptr);
    result.push_back(name);
  }
  return result;
}

std::vector<std::string> PostProcessingShaderImplementation::GetShaderList(APIType api_type)
{
  // Currently there is no differentiation between API types and shader languages.
  // This could change in the future, hence the api_type parameter, but ideally,
  // shaders should be compatible across backends.
  if (api_type == APIType::OpenGL || api_type == APIType::Vulkan)
    return GetShaders();

  return {};
}

std::vector<std::string> PostProcessingShaderImplementation::GetAnaglyphShaderList(APIType api_type)
{
  if (api_type == APIType::OpenGL || api_type == APIType::Vulkan)
    return GetShaders(ANAGLYPH_DIR DIR_SEP);

  return {};
}

std::string PostProcessingShaderConfiguration::LoadShader(std::string shader)
{
  // Load the shader from the configuration if there isn't one sent to us.
  if (shader == "")
    shader = g_ActiveConfig.sPostProcessingShader;
  m_current_shader = shader;

  const std::string sub_dir = (g_Config.iStereoMode == STEREO_ANAGLYPH) ? ANAGLYPH_DIR DIR_SEP : "";

  // loading shader code
  std::string code;
  std::string path = File::GetUserPath(D_SHADERS_IDX) + sub_dir + shader + ".glsl";

  if (shader == "")
  {
    code = s_default_shader;
  }
  else
  {
    if (!File::Exists(path))
    {
      // Fallback to shared user dir
      path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir + shader + ".glsl";
    }

    if (!File::ReadFileToString(path, code))
    {
      ERROR_LOG(VIDEO, "Post-processing shader not found: %s", path.c_str());
      code = s_default_shader;
    }
  }

  LoadOptions(code);
  LoadOptionsConfiguration();

  return code;
}

void PostProcessingShaderConfiguration::LoadOptions(const std::string& code)
{
  const std::string config_start_delimiter = "[configuration]";
  const std::string config_end_delimiter = "[/configuration]";
  size_t configuration_start = code.find(config_start_delimiter);
  size_t configuration_end = code.find(config_end_delimiter);

  m_options.clear();
  m_any_options_dirty = true;

  if (configuration_start == std::string::npos || configuration_end == std::string::npos)
  {
    // Issue loading configuration or there isn't one.
    return;
  }

  std::string configuration_string =
      code.substr(configuration_start + config_start_delimiter.size(),
                  configuration_end - configuration_start - config_start_delimiter.size());

  std::istringstream in(configuration_string);

  struct GLSLStringOption
  {
    std::string m_type;
    std::vector<std::pair<std::string, std::string>> m_options;
  };

  std::vector<GLSLStringOption> option_strings;
  GLSLStringOption* current_strings = nullptr;
  while (!in.eof())
  {
    std::string line;

    if (std::getline(in, line))
    {
#ifndef _WIN32
      // Check for CRLF eol and convert it to LF
      if (!line.empty() && line.at(line.size() - 1) == '\r')
      {
        line.erase(line.size() - 1);
      }
#endif

      if (line.size() > 0)
      {
        if (line[0] == '[')
        {
          size_t endpos = line.find("]");

          if (endpos != std::string::npos)
          {
            // New section!
            std::string sub = line.substr(1, endpos - 1);
            option_strings.push_back({sub});
            current_strings = &option_strings.back();
          }
        }
        else
        {
          if (current_strings)
          {
            std::string key, value;
            IniFile::ParseLine(line, &key, &value);

            if (!(key == "" && value == ""))
              current_strings->m_options.emplace_back(key, value);
          }
        }
      }
    }
  }

  for (const auto& it : option_strings)
  {
    ConfigurationOption option;
    option.m_dirty = true;

    if (it.m_type == "OptionBool")
      option.m_type = ConfigurationOption::OptionType::OPTION_BOOL;
    else if (it.m_type == "OptionRangeFloat")
      option.m_type = ConfigurationOption::OptionType::OPTION_FLOAT;
    else if (it.m_type == "OptionRangeInteger")
      option.m_type = ConfigurationOption::OptionType::OPTION_INTEGER;

    for (const auto& string_option : it.m_options)
    {
      if (string_option.first == "GUIName")
      {
        option.m_gui_name = string_option.second;
      }
      else if (string_option.first == "OptionName")
      {
        option.m_option_name = string_option.second;
      }
      else if (string_option.first == "DependentOption")
      {
        option.m_dependent_option = string_option.second;
      }
      else if (string_option.first == "MinValue" || string_option.first == "MaxValue" ||
               string_option.first == "DefaultValue" || string_option.first == "StepAmount")
      {
        std::vector<s32>* output_integer = nullptr;
        std::vector<float>* output_float = nullptr;

        if (string_option.first == "MinValue")
        {
          output_integer = &option.m_integer_min_values;
          output_float = &option.m_float_min_values;
        }
        else if (string_option.first == "MaxValue")
        {
          output_integer = &option.m_integer_max_values;
          output_float = &option.m_float_max_values;
        }
        else if (string_option.first == "DefaultValue")
        {
          output_integer = &option.m_integer_values;
          output_float = &option.m_float_values;
        }
        else if (string_option.first == "StepAmount")
        {
          output_integer = &option.m_integer_step_values;
          output_float = &option.m_float_step_values;
        }

        if (option.m_type == ConfigurationOption::OptionType::OPTION_BOOL)
        {
          TryParse(string_option.second, &option.m_bool_value);
        }
        else if (option.m_type == ConfigurationOption::OptionType::OPTION_INTEGER)
        {
          TryParseVector(string_option.second, output_integer);
          if (output_integer->size() > 4)
            output_integer->erase(output_integer->begin() + 4, output_integer->end());
        }
        else if (option.m_type == ConfigurationOption::OptionType::OPTION_FLOAT)
        {
          TryParseVector(string_option.second, output_float);
          if (output_float->size() > 4)
            output_float->erase(output_float->begin() + 4, output_float->end());
        }
      }
    }
    m_options[option.m_option_name] = option;
  }
}

void PostProcessingShaderConfiguration::LoadOptionsConfiguration()
{
  IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  std::string section = m_current_shader + "-options";

  for (auto& it : m_options)
  {
    switch (it.second.m_type)
    {
    case ConfigurationOption::OptionType::OPTION_BOOL:
      ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &it.second.m_bool_value,
                                           it.second.m_bool_value);
      break;
    case ConfigurationOption::OptionType::OPTION_INTEGER:
    {
      std::string value;
      ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &value);
      if (value != "")
        TryParseVector(value, &it.second.m_integer_values);
    }
    break;
    case ConfigurationOption::OptionType::OPTION_FLOAT:
    {
      std::string value;
      ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &value);
      if (value != "")
        TryParseVector(value, &it.second.m_float_values);
    }
    break;
    }
  }
}

void PostProcessingShaderConfiguration::SaveOptionsConfiguration()
{
  IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  std::string section = m_current_shader + "-options";

  for (auto& it : m_options)
  {
    switch (it.second.m_type)
    {
    case ConfigurationOption::OptionType::OPTION_BOOL:
    {
      ini.GetOrCreateSection(section)->Set(it.second.m_option_name, it.second.m_bool_value);
    }
    break;
    case ConfigurationOption::OptionType::OPTION_INTEGER:
    {
      std::string value = "";
      for (size_t i = 0; i < it.second.m_integer_values.size(); ++i)
        value += StringFromFormat("%d%s", it.second.m_integer_values[i],
                                  i == (it.second.m_integer_values.size() - 1) ? "" : ", ");
      ini.GetOrCreateSection(section)->Set(it.second.m_option_name, value);
    }
    break;
    case ConfigurationOption::OptionType::OPTION_FLOAT:
    {
      std::ostringstream value;
      value.imbue(std::locale("C"));

      for (size_t i = 0; i < it.second.m_float_values.size(); ++i)
      {
        value << it.second.m_float_values[i];
        if (i != (it.second.m_float_values.size() - 1))
          value << ", ";
      }
      ini.GetOrCreateSection(section)->Set(it.second.m_option_name, value.str());
    }
    break;
    }
  }
  ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));
}

void PostProcessingShaderConfiguration::ReloadShader()
{
  m_current_shader = "";
}

void PostProcessingShaderConfiguration::SetOptionf(const std::string& option, int index,
                                                   float value)
{
  auto it = m_options.find(option);

  it->second.m_float_values[index] = value;
  it->second.m_dirty = true;
  m_any_options_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptioni(const std::string& option, int index, s32 value)
{
  auto it = m_options.find(option);

  it->second.m_integer_values[index] = value;
  it->second.m_dirty = true;
  m_any_options_dirty = true;
}

void PostProcessingShaderConfiguration::SetOptionb(const std::string& option, bool value)
{
  auto it = m_options.find(option);

  it->second.m_bool_value = value;
  it->second.m_dirty = true;
  m_any_options_dirty = true;
}
