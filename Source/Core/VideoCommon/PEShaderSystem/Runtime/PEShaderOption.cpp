// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Runtime/PEShaderOption.h"
#include "VideoCommon/PEShaderSystem/Runtime/RuntimeShaderImpl.h"

#include <array>
#include <string>

#include "Common/VariantUtil.h"
#include "VideoCommon/ShaderGenCommon.h"

namespace VideoCommon::PE
{
std::unique_ptr<ShaderOption> ShaderOption::Create(const ShaderConfigOption& config_option)
{
  std::unique_ptr<ShaderOption> result;
  std::visit(overloaded{[&](EnumChoiceOption o) {
                          result = std::make_unique<RuntimeEnumOption>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant,
                              RuntimeEnumOption::OptionType{o.m_values_for_choices[o.m_index]});
                        },
                        [&](FloatOption<1> o) {
                          result = std::make_unique<RuntimeFloatOption<1>>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](FloatOption<2> o) {
                          result = std::make_unique<RuntimeFloatOption<2>>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](FloatOption<3> o) {
                          result = std::make_unique<RuntimeFloatOption<3>>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](FloatOption<4> o) {
                          result = std::make_unique<RuntimeFloatOption<4>>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](IntOption<1> o) {
                          result = std::make_unique<RuntimeIntOption<1>>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](IntOption<2> o) {
                          result = std::make_unique<RuntimeIntOption<2>>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](IntOption<3> o) {
                          result = std::make_unique<RuntimeIntOption<3>>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](IntOption<4> o) {
                          result = std::make_unique<RuntimeIntOption<4>>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](BoolOption o) {
                          result = std::make_unique<RuntimeBoolOption>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](ColorOption o) {
                          result = std::make_unique<RuntimeColorOption>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        },
                        [&](ColorAlphaOption o) {
                          result = std::make_unique<RuntimeColorAlphaOption>(
                              o.m_common.m_name, o.m_common.m_compile_time_constant, o.m_value);
                        }},
             config_option);
  return result;
}

std::size_t ShaderOption::Size() const
{
  if (m_evaluate_at_compile_time)
    return 0;

  return SizeImpl();
}

void ShaderOption::WriteToMemory(u8*& buffer) const
{
  if (m_evaluate_at_compile_time)
    return;

  return WriteToMemoryImpl(buffer);
}

void ShaderOption::WriteShaderUniforms(ShaderCode& shader_source) const
{
  if (m_evaluate_at_compile_time)
    return;

  WriteShaderUniformsImpl(shader_source);
}

void ShaderOption::WriteShaderConstants(ShaderCode& shader_source) const
{
  if (!m_evaluate_at_compile_time)
    return;

  WriteShaderConstantsImpl(shader_source);
}
}  // namespace VideoCommon::PE
