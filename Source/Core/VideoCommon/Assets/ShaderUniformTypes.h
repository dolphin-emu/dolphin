// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>

#include "Common/CommonTypes.h"
#include "VideoCommon/Assets/ShaderUniform.h"
#include "VideoCommon/ShaderGenCommon.h"

namespace VideoCommon
{
// All the uniform types in this file are
// designed to fit into a 'std140' shader layout

template <std::size_t N>
class ShaderUniformFloat final : public ShaderUniform
{
public:
  using DataType = float;
  using ValueType = std::array<DataType, N>;

  ShaderUniformFloat(std::string name, ValueType value) : m_name(std::move(name)), m_value(value) {}

  explicit ShaderUniformFloat(std::string name) : m_name(std::move(name)) {}

  void WriteToMemory(u8*& buffer) const override
  {
    std::memcpy(buffer, m_value.data(), m_bytes);
    std::memset(buffer + m_bytes, 0, m_padding_bytes);
    buffer += Size();
  }

  void WriteAsShaderCode(ShaderCode& shader_source) const override
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

  std::size_t Size() const override { return m_bytes + m_padding_bytes; }

  std::unique_ptr<ShaderUniform> clone() const override
  {
    return std::make_unique<ShaderUniformFloat<N>>(m_name, m_value);
  }

  DataType* Data() { return m_value.data(); }
  const DataType* Data() const { return m_value.data(); }

  static constexpr std::size_t ElementCount() { return N; }

private:
  static_assert(N <= 4UL);

  static constexpr std::size_t m_bytes = N * sizeof(DataType);
  static constexpr std::size_t m_padding = 4UL - N;
  static constexpr std::size_t m_padding_bytes = m_padding * sizeof(DataType);

  std::string m_name;
  ValueType m_value = {};
};

template <std::size_t N>
class ShaderUniformInt final : public ShaderUniform
{
public:
  using ValueType = std::array<s32, N>;
  using DataType = s32;

  ShaderUniformInt(std::string name, ValueType value) : m_name(std::move(name)), m_value(value) {}

  explicit ShaderUniformInt(std::string name) : m_name(std::move(name)) {}

  void WriteToMemory(u8*& buffer) const override
  {
    std::memcpy(buffer, m_value.data(), m_bytes);
    std::memset(buffer + m_bytes, 0, m_padding_bytes);
    buffer += Size();
  }

  void WriteAsShaderCode(ShaderCode& shader_source) const override
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

  std::size_t Size() const override { return m_bytes + m_padding_bytes; }

  std::unique_ptr<ShaderUniform> clone() const override
  {
    return std::make_unique<ShaderUniformInt<N>>(m_name, m_value);
  }

  DataType* Data() { return m_value.data(); }
  const DataType* Data() const { return m_value.data(); }

  static constexpr std::size_t ElementCount() { return N; }

private:
  static_assert(N <= 4UL);

  static constexpr std::size_t m_bytes = N * sizeof(DataType);
  static constexpr std::size_t m_padding = 4UL - N;
  static constexpr std::size_t m_padding_bytes = m_padding * sizeof(DataType);

  std::string m_name;
  ValueType m_value = {};
};

class ShaderUniformBool final : public ShaderUniform
{
public:
  using DataType = bool;

  ShaderUniformBool(std::string name, bool value) : m_name(std::move(name)), m_value(value) {}

  explicit ShaderUniformBool(std::string name) : m_name(std::move(name)) {}

  void WriteToMemory(u8*& buffer) const override
  {
    int val = m_value ? 1 : 0;
    std::memcpy(buffer, &val, m_bytes);
    std::memset(buffer + m_bytes, 0, m_padding_bytes);
    buffer += Size();
  }

  void WriteAsShaderCode(ShaderCode& shader_source) const override
  {
    shader_source.Write("bool {};\n", m_name);
    for (std::size_t i = 0; i < m_padding; i++)
    {
      shader_source.Write("bool _{}{}_padding;\n", m_name, i);
    }
  }

  std::size_t Size() const override { return m_bytes + m_padding_bytes; }

  DataType* Data() { return &m_value; }
  const DataType* Data() const { return &m_value; }

  std::unique_ptr<ShaderUniform> clone() const override
  {
    return std::make_unique<ShaderUniformBool>(m_name, m_value);
  }

private:
  static constexpr std::size_t m_bytes = 4;
  static constexpr std::size_t m_padding = 3;
  static constexpr std::size_t m_padding_bytes = m_padding * sizeof(u32);

  std::string m_name;
  DataType m_value = false;
};

using ShaderUniformRGBA = ShaderUniformFloat<4>;
using ShaderUniformRGB = ShaderUniformFloat<3>;
}  // namespace VideoCommon
