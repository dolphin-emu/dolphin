// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <picojson.h>

#include "Common/CommonTypes.h"

namespace VideoCommon::PE
{
struct CommonOptionContext
{
  std::string m_ui_name;
  std::string m_ui_description;
  std::string m_name;
  std::string m_dependent_option;
  std::string m_group_name;

  bool m_compile_time_constant = false;
};

const constexpr std::size_t MAX_SNAPSHOTS = 5;

template <typename T>
struct Snapshots
{
  std::array<T, MAX_SNAPSHOTS> m_values;
};

struct EnumChoiceOption
{
  using ValueType = s32;
  CommonOptionContext m_common;

  std::vector<std::string> m_ui_choices = {};
  std::vector<ValueType> m_values_for_choices = {};
  u32 m_index = 0;
  u32 m_default_index = 0;

  Snapshots<std::optional<ValueType>> m_snapshots;
};

struct BoolOption
{
  CommonOptionContext m_common;

  bool m_default_value = false;
  bool m_value = false;

  Snapshots<std::optional<bool>> m_snapshots;
};

template <std::size_t Size>
struct FloatOption
{
  using ValueType = std::array<float, Size>;
  CommonOptionContext m_common;

  ValueType m_default_value = {};
  ValueType m_min_value = {};
  ValueType m_max_value = {};
  ValueType m_value = {};
  ValueType m_step_size = {};
  ValueType m_decimal_precision = {};

  Snapshots<std::optional<ValueType>> m_snapshots;
};

template <std::size_t Size>
struct IntOption
{
  using ValueType = std::array<s32, Size>;
  CommonOptionContext m_common;

  ValueType m_default_value = {};
  ValueType m_min_value = {};
  ValueType m_max_value = {};
  ValueType m_value = {};
  ValueType m_step_size = {};

  Snapshots<std::optional<ValueType>> m_snapshots;
};

struct ColorOption final : public FloatOption<3>
{
  using FloatOption<3>::FloatOption;
  using ValueType = FloatOption<3>::ValueType;
};

struct ColorAlphaOption final : public FloatOption<4>
{
  using FloatOption<4>::FloatOption;
  using ValueType = FloatOption<4>::ValueType;
};

// TODO: game-memory option...

using ShaderConfigOption =
    std::variant<EnumChoiceOption, FloatOption<1>, FloatOption<2>, FloatOption<3>, FloatOption<4>,
                 IntOption<1>, IntOption<2>, IntOption<3>, IntOption<4>, BoolOption, ColorOption,
                 ColorAlphaOption>;

bool IsValid(const ShaderConfigOption& option);
void Reset(ShaderConfigOption& option);
void LoadSnapshot(ShaderConfigOption& option, std::size_t channel);
void SaveSnapshot(ShaderConfigOption& option, std::size_t channel);
bool HasSnapshot(const ShaderConfigOption& option, std::size_t channel);
std::string GetOptionGroup(const ShaderConfigOption& option);
bool IsUnsatisfiedDependency(const ShaderConfigOption& option, const std::string& dependency_name);
std::optional<ShaderConfigOption> DeserializeOptionFromConfig(const picojson::object& obj);

void SerializeOptionToProfile(picojson::object& obj, const ShaderConfigOption& option);
void DeserializeOptionFromProfile(const picojson::object& obj, ShaderConfigOption& option);
}  // namespace VideoCommon::PE
