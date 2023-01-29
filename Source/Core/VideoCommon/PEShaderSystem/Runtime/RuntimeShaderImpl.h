// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderOption.h"
#include "VideoCommon/ShaderGenCommon.h"

namespace VideoCommon::PE
{
template <std::size_t N, typename ConfigType = FloatOption<N>>
class RuntimeFloatOption final : public ShaderOption
{
public:
  using OptionType = std::array<float, N>;

  explicit RuntimeFloatOption(std::string name) : m_name(std::move(name)) {}
  RuntimeFloatOption(std::string name, bool compile_time_constant, OptionType value)
      : m_name(std::move(name)), m_value(std::move(value))
  {
    m_evaluate_at_compile_time = compile_time_constant;
  }

  void Update(const OptionType& value) { m_value = value; }

  void Update(const ShaderConfigOption& config_option) override
  {
    if (auto v = std::get_if<ConfigType>(&config_option))
      m_value = v->m_value;
  }

  void WriteToMemoryImpl(u8*& buffer) const override
  {
    std::memcpy(buffer, m_value.data(), m_bytes);
    std::memset(buffer + m_bytes, 0, m_padding_bytes);
    buffer += SizeImpl();
  }

  void WriteShaderUniformsImpl(ShaderCode& shader_source) const override
  {
    if constexpr (N == 1UL)
    {
      shader_source.Write("float {};\n", m_name);
    }
    else
    {
      shader_source.Write("float{} {};\n", N, m_name);
    }
    for (std::size_t i = 0; i < m_padding; i++)
    {
      shader_source.Write("float _{}{}_padding;\n", m_name, i);
    }
  }

  void WriteShaderConstantsImpl(ShaderCode& shader_source) const override
  {
    if constexpr (N == 1UL)
    {
      shader_source.Write("#define {} float({})\n", m_name, m_value[0]);
    }
    else
    {
      shader_source.Write("#define {} float{}({})\n", m_name, N, fmt::join(m_value, ","));
    }
  }

  std::size_t SizeImpl() const override { return m_bytes + m_padding_bytes; }

private:
  static_assert(N <= 4UL);

  static constexpr std::size_t m_bytes = N * sizeof(float);
  static constexpr std::size_t m_padding = 4UL - N;
  static constexpr std::size_t m_padding_bytes = m_padding * sizeof(float);

  std::string m_name;
  OptionType m_value;
};

using RuntimeColorOption = RuntimeFloatOption<3, ColorOption>;
using RuntimeColorAlphaOption = RuntimeFloatOption<4, ColorAlphaOption>;

template <std::size_t N, typename ConfigType = IntOption<N>>
class RuntimeIntOption final : public ShaderOption
{
public:
  using OptionType = std::array<s32, N>;

  explicit RuntimeIntOption(std::string name) : m_name(std::move(name)) {}
  RuntimeIntOption(std::string name, bool compile_time_constant, OptionType value)
      : m_name(std::move(name)), m_value(std::move(value))
  {
    m_evaluate_at_compile_time = compile_time_constant;
  }

  void Update(const OptionType& value) { m_value = value; }

  void Update(const ShaderConfigOption& config_option) override
  {
    if (auto v = std::get_if<ConfigType>(&config_option))
    {
      if constexpr (std::is_same_v<ConfigType, EnumChoiceOption>)
      {
        m_value[0] = v->m_values_for_choices[v->m_index];
      }
      else
      {
        m_value = v->m_value;
      }
    }
  }

  void WriteToMemoryImpl(u8*& buffer) const override
  {
    std::memcpy(buffer, m_value.data(), m_bytes);
    std::memset(buffer + m_bytes, 0, m_padding_bytes);
    buffer += SizeImpl();
  }

  void WriteShaderUniformsImpl(ShaderCode& shader_source) const override
  {
    if constexpr (N == 1UL)
    {
      shader_source.Write("int {};\n", m_name);
    }
    else
    {
      shader_source.Write("int{} {};\n", N, m_name);
    }
    for (std::size_t i = 0; i < m_padding; i++)
    {
      shader_source.Write("int _{}{}_padding;\n", m_name, i);
    }
  }

  void WriteShaderConstantsImpl(ShaderCode& shader_source) const override
  {
    if constexpr (N == 1UL)
    {
      shader_source.Write("#define {} int({})\n", m_name, m_value[0]);
    }
    else
    {
      shader_source.Write("#define {} int{}({})\n", m_name, N, fmt::join(m_value, ","));
    }
  }

  std::size_t SizeImpl() const override { return m_bytes + m_padding_bytes; }

private:
  static_assert(N <= 4UL);

  static constexpr std::size_t m_bytes = N * sizeof(s32);
  static constexpr std::size_t m_padding = 4UL - N;
  static constexpr std::size_t m_padding_bytes = m_padding * sizeof(s32);

  std::string m_name;
  OptionType m_value;
};

using RuntimeEnumOption = RuntimeIntOption<1, EnumChoiceOption>;

class RuntimeBoolOption final : public ShaderOption
{
public:
  explicit RuntimeBoolOption(std::string name) : m_name(std::move(name)) {}
  RuntimeBoolOption(std::string name, bool compile_time_constant, bool value)
      : m_name(std::move(name)), m_value(value)
  {
    m_evaluate_at_compile_time = compile_time_constant;
  }

  void Update(bool value) { m_value = value; }

  void Update(const ShaderConfigOption& config_option) override
  {
    if (auto v = std::get_if<BoolOption>(&config_option))
      m_value = v->m_value;
  }

  void WriteToMemoryImpl(u8*& buffer) const override
  {
    int val = m_value ? 1 : 0;
    std::memcpy(buffer, &val, m_bytes);
    std::memset(buffer + m_bytes, 0, m_padding_bytes);
    buffer += SizeImpl();
  }

  void WriteShaderUniformsImpl(ShaderCode& shader_source) const override
  {
    shader_source.Write("bool {};\n", m_name);
    for (std::size_t i = 0; i < m_padding; i++)
    {
      shader_source.Write("bool _{}{}_padding;\n", m_name, i);
    }
  }

  void WriteShaderConstantsImpl(ShaderCode& shader_source) const override
  {
    shader_source.Write("#define {} ({})\n", m_name, static_cast<u32>(m_value));
  }

  std::size_t SizeImpl() const override { return m_bytes + m_padding_bytes; }

private:
  static constexpr std::size_t m_bytes = 4;
  static constexpr std::size_t m_padding = 3;
  static constexpr std::size_t m_padding_bytes = m_padding * sizeof(u32);

  std::string m_name;
  bool m_value;
};
}  // namespace VideoCommon::PE
