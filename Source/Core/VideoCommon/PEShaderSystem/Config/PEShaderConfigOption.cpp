// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigOption.h"

#include <algorithm>

#include "Common/Logging/Log.h"
#include "Common/VariantUtil.h"

namespace VideoCommon::PE
{
namespace
{
template <typename OptionType>
bool ProcessNumeric(OptionType& option, const picojson::object& obj)
{
  using option_value_t = typename OptionType::ValueType;
  using element_type_t =
      std::remove_reference_t<decltype(*std::begin(std::declval<option_value_t&>()))>;
  const auto min_iter = obj.find("min");
  if (min_iter != obj.end() && min_iter->second.is<picojson::array>())
  {
    const auto min_json = min_iter->second.get<picojson::array>();
    if (!std::all_of(min_json.begin(), min_json.end(),
                     [](const picojson::value& v) { return v.is<double>(); }))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, enum option 'min' "
                           "should contain only doubles");
      return false;
    }

    if (min_json.size() != option.m_min_value.size())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, enum option 'min' "
                    "has unexpected number of elements, expected {}",
                    option.m_min_value.size());
      return false;
    }
    for (std::size_t i = 0; i < min_json.size(); i++)
    {
      option.m_min_value[i] = static_cast<element_type_t>(min_json[i].get<double>());
    }
  }

  const auto max_iter = obj.find("max");
  if (max_iter != obj.end() && max_iter->second.is<picojson::array>())
  {
    const auto max_json = max_iter->second.get<picojson::array>();
    if (!std::all_of(max_json.begin(), max_json.end(),
                     [](const picojson::value& v) { return v.is<double>(); }))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, enum option 'max' "
                           "should contain only doubles");
      return false;
    }

    if (max_json.size() != option.m_max_value.size())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, enum option 'max' "
                    "has unexpected number of elements, expected {}",
                    option.m_max_value.size());
      return false;
    }
    for (std::size_t i = 0; i < max_json.size(); i++)
    {
      option.m_max_value[i] = static_cast<element_type_t>(max_json[i].get<double>());
    }
  }

  const auto default_iter = obj.find("default");
  if (default_iter != obj.end() && default_iter->second.is<picojson::array>())
  {
    const auto default_json = default_iter->second.get<picojson::array>();
    if (!std::all_of(default_json.begin(), default_json.end(),
                     [](const picojson::value& v) { return v.is<double>(); }))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, enum option 'default' "
                           "should contain only doubles");
      return false;
    }

    if (default_json.size() != option.m_default_value.size())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, enum option 'default' "
                    "has unexpected number of elements, expected {}",
                    option.m_default_value.size());
      return false;
    }
    for (std::size_t i = 0; i < default_json.size(); i++)
    {
      option.m_default_value[i] = static_cast<element_type_t>(default_json[i].get<double>());
      option.m_value[i] = option.m_default_value[i];
    }
  }

  const auto step_iter = obj.find("step");
  if (step_iter == obj.end() || !step_iter->second.is<picojson::array>())
  {
    for (std::size_t i = 0; i < option.m_step_size.size(); i++)
    {
      option.m_step_size[i] = 1;
    }
  }
  else
  {
    const auto step_json = step_iter->second.get<picojson::array>();
    if (!std::all_of(step_json.begin(), step_json.end(),
                     [](const picojson::value& v) { return v.is<double>(); }))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, enum option 'step' "
                           "should contain only doubles");
      return false;
    }

    if (step_json.size() != option.m_step_size.size())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, enum option 'default' "
                    "has unexpected number of elements, expected {}",
                    option.m_step_size.size());
      return false;
    }
    for (std::size_t i = 0; i < step_json.size(); i++)
    {
      option.m_step_size[i] = static_cast<element_type_t>(step_json[i].get<double>());
    }
  }
  return true;
}
}  // namespace
bool IsValid(const ShaderConfigOption& option)
{
  return false;
}

void Reset(ShaderConfigOption& option)
{
  std::visit(overloaded{[&](EnumChoiceOption& o) { o.m_index = o.m_default_index; },
                        [&](FloatOption<1>& o) { o.m_value = o.m_default_value; },
                        [&](FloatOption<2>& o) { o.m_value = o.m_default_value; },
                        [&](FloatOption<3>& o) { o.m_value = o.m_default_value; },
                        [&](FloatOption<4>& o) { o.m_value = o.m_default_value; },
                        [&](IntOption<1>& o) { o.m_value = o.m_default_value; },
                        [&](IntOption<2>& o) { o.m_value = o.m_default_value; },
                        [&](IntOption<3>& o) { o.m_value = o.m_default_value; },
                        [&](IntOption<4>& o) { o.m_value = o.m_default_value; },
                        [&](BoolOption& o) { o.m_value = o.m_default_value; },
                        [&](ColorOption& o) { o.m_value = o.m_default_value; },
                        [&](ColorAlphaOption& o) { o.m_value = o.m_default_value; }},
             option);
}

void LoadSnapshot(ShaderConfigOption& option, std::size_t channel)
{
  const auto load_snapshot = [&](auto& o) {
    if (channel < o.m_snapshots.m_values.size() && o.m_snapshots.m_values[channel])
    {
      o.m_value = *o.m_snapshots.m_values[channel];
    }
  };
  std::visit(
      overloaded{
          [&](EnumChoiceOption& o) {
            if (channel < o.m_snapshots.m_values.size() && o.m_snapshots.m_values[channel])
            {
              o.m_index = *o.m_snapshots.m_values[channel];
            }
          },
          [&](FloatOption<1>& o) { load_snapshot(o); },
          [&](FloatOption<2>& o) { load_snapshot(o); },
          [&](FloatOption<3>& o) { load_snapshot(o); },
          [&](FloatOption<4>& o) { load_snapshot(o); }, [&](IntOption<1>& o) { load_snapshot(o); },
          [&](IntOption<2>& o) { load_snapshot(o); }, [&](IntOption<3>& o) { load_snapshot(o); },
          [&](IntOption<4>& o) { load_snapshot(o); }, [&](BoolOption& o) { load_snapshot(o); },
          [&](ColorOption& o) { load_snapshot(o); },
          [&](ColorAlphaOption& o) { load_snapshot(o); }},
      option);
}

void SaveSnapshot(ShaderConfigOption& option, std::size_t channel)
{
  const auto save_snapshot = [&](auto& o) {
    if (channel < o.m_snapshots.m_values.size())
    {
      o.m_snapshots.m_values[channel] = o.m_value;
    }
  };
  std::visit(
      overloaded{
          [&](EnumChoiceOption& o) {
            if (channel < o.m_snapshots.m_values.size())
            {
              o.m_snapshots.m_values[channel] = o.m_index;
            }
          },
          [&](FloatOption<1>& o) { save_snapshot(o); },
          [&](FloatOption<2>& o) { save_snapshot(o); },
          [&](FloatOption<3>& o) { save_snapshot(o); },
          [&](FloatOption<4>& o) { save_snapshot(o); }, [&](IntOption<1>& o) { save_snapshot(o); },
          [&](IntOption<2>& o) { save_snapshot(o); }, [&](IntOption<3>& o) { save_snapshot(o); },
          [&](IntOption<4>& o) { save_snapshot(o); }, [&](BoolOption& o) { save_snapshot(o); },
          [&](ColorOption& o) { save_snapshot(o); },
          [&](ColorAlphaOption& o) { save_snapshot(o); }},
      option);
}

bool HasSnapshot(const ShaderConfigOption& option, std::size_t channel)
{
  bool result = false;
  const auto set_has_snapshot = [&](auto& o) {
    result = channel < o.m_snapshots.m_values.size() && o.m_snapshots.m_values[channel].has_value();
  };
  std::visit(overloaded{[&](const EnumChoiceOption& o) { set_has_snapshot(o); },
                        [&](const FloatOption<1>& o) { set_has_snapshot(o); },
                        [&](const FloatOption<2>& o) { set_has_snapshot(o); },
                        [&](const FloatOption<3>& o) { set_has_snapshot(o); },
                        [&](const FloatOption<4>& o) { set_has_snapshot(o); },
                        [&](const IntOption<1>& o) { set_has_snapshot(o); },
                        [&](const IntOption<2>& o) { set_has_snapshot(o); },
                        [&](const IntOption<3>& o) { set_has_snapshot(o); },
                        [&](const IntOption<4>& o) { set_has_snapshot(o); },
                        [&](const BoolOption& o) { set_has_snapshot(o); },
                        [&](const ColorOption& o) { set_has_snapshot(o); },
                        [&](const ColorAlphaOption& o) { set_has_snapshot(o); }},
             option);
  return result;
}

std::string GetOptionGroup(const ShaderConfigOption& option)
{
  std::string result;

  const auto set_group = [&](auto& o) { result = o.m_common.m_group_name; };
  std::visit(overloaded{[&](const EnumChoiceOption& o) { set_group(o); },
                        [&](const FloatOption<1>& o) { set_group(o); },
                        [&](const FloatOption<2>& o) { set_group(o); },
                        [&](const FloatOption<3>& o) { set_group(o); },
                        [&](const FloatOption<4>& o) { set_group(o); },
                        [&](const IntOption<1>& o) { set_group(o); },
                        [&](const IntOption<2>& o) { set_group(o); },
                        [&](const IntOption<3>& o) { set_group(o); },
                        [&](const IntOption<4>& o) { set_group(o); },
                        [&](const BoolOption& o) { set_group(o); },
                        [&](const ColorOption& o) { set_group(o); },
                        [&](const ColorAlphaOption& o) { set_group(o); }},
             option);

  return result;
}

bool IsUnsatisfiedDependency(const ShaderConfigOption& option, const std::string& dependency_name)
{
  bool result = false;
  std::visit(overloaded{[&](const EnumChoiceOption& o) {}, [&](const FloatOption<1>& o) {},
                        [&](const FloatOption<2>& o) {}, [&](const FloatOption<3>& o) {},
                        [&](const FloatOption<4>& o) {}, [&](const IntOption<1>& o) {},
                        [&](const IntOption<2>& o) {}, [&](const IntOption<3>& o) {},
                        [&](const IntOption<4>& o) {},
                        [&](const BoolOption& o) {
                          result = o.m_common.m_name == dependency_name && !o.m_value;
                        },
                        [&](const ColorOption& o) {}, [&](const ColorAlphaOption& o) {}},
             option);

  return result;
}

std::optional<ShaderConfigOption> DeserializeOptionFromConfig(const picojson::object& obj)
{
  const auto type_iter = obj.find("type");
  if (type_iter == obj.end())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, option 'type' not found");
    return std::nullopt;
  }

  const auto process_common = [&](auto& option) -> bool {
    const auto name_iter = obj.find("name");
    if (name_iter == obj.end())
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, option 'name' not found");
      return false;
    }
    if (!name_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, option 'name' is not a string type");
      return false;
    }
    option.m_common.m_name = name_iter->second.to_str();

    const auto ui_name_iter = obj.find("ui_name");
    if (ui_name_iter == obj.end() || !ui_name_iter->second.is<std::string>())
    {
      option.m_common.m_ui_name = option.m_common.m_name;
    }
    else
    {
      option.m_common.m_ui_name = ui_name_iter->second.to_str();
    }

    const auto ui_description_iter = obj.find("ui_description");
    if (ui_description_iter != obj.end() && ui_description_iter->second.is<std::string>())
    {
      option.m_common.m_ui_description = ui_description_iter->second.to_str();
    }

    const auto dependent_option_iter = obj.find("dependent_option");
    if (dependent_option_iter != obj.end() && dependent_option_iter->second.is<std::string>())
    {
      option.m_common.m_dependent_option = dependent_option_iter->second.to_str();
    }

    const auto group_name_iter = obj.find("group_name");
    if (group_name_iter != obj.end() && group_name_iter->second.is<std::string>())
    {
      option.m_common.m_group_name = group_name_iter->second.to_str();
    }

    const auto is_constant_iter = obj.find("is_constant");
    if (is_constant_iter != obj.end() && is_constant_iter->second.is<bool>())
    {
      option.m_common.m_compile_time_constant = is_constant_iter->second.get<bool>();
    }

    return true;
  };

  const auto process_float_precision = [&](auto& option) {
    const auto precision_iter = obj.find("precision");
    if (precision_iter != obj.end() && precision_iter->second.is<picojson::array>())
    {
      const auto precision_json = precision_iter->second.get<picojson::array>();
      if (!std::all_of(precision_json.begin(), precision_json.end(),
                       [](const picojson::value& v) { return v.is<double>(); }))
      {
        ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, float option 'precision' "
                             "should contain only doubles");
        return false;
      }

      if (precision_json.size() != option.m_decimal_precision.size())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load shader configuration file, float option 'precision' "
                      "has unexpected number of elements, expected {}",
                      option.m_decimal_precision.size());
        return false;
      }
      for (std::size_t i = 0; i < precision_json.size(); i++)
      {
        option.m_decimal_precision[i] = static_cast<float>(precision_json[i].get<double>());
      }
    }
    return true;
  };

  std::string type = type_iter->second.to_str();
  std::transform(type.begin(), type.end(), type.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (type == "bool")
  {
    BoolOption option;
    if (!process_common(option))
      return std::nullopt;
    const auto default_iter = obj.find("default");
    if (default_iter != obj.end() && default_iter->second.is<bool>())
    {
      option.m_default_value = default_iter->second.get<bool>();
      option.m_value = option.m_default_value;
    }
    return option;
  }
  else if (type == "enum")
  {
    EnumChoiceOption option;
    if (!process_common(option))
      return std::nullopt;
    const auto ui_choices_iter = obj.find("ui_choices");
    if (ui_choices_iter == obj.end() || !ui_choices_iter->second.is<picojson::array>())
    {
      ERROR_LOG_FMT(
          VIDEO, "Failed to load shader configuration file, enum option 'ui_choices' is not valid");
      return std::nullopt;
    }

    const auto ui_choices_json = ui_choices_iter->second.get<picojson::array>();
    if (!std::all_of(ui_choices_json.begin(), ui_choices_json.end(),
                     [](const picojson::value& v) { return v.is<std::string>(); }))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, enum option 'ui_choices' "
                           "should contain only strings");
      return std::nullopt;
    }

    const auto values_iter = obj.find("values");
    if (values_iter == obj.end() || !values_iter->second.is<picojson::array>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, enum option 'values' is not valid");
      return std::nullopt;
    }

    const auto values_json = values_iter->second.get<picojson::array>();
    if (!std::all_of(values_json.begin(), values_json.end(),
                     [](const picojson::value& v) { return v.is<double>(); }))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, enum option 'values' should "
                           "contain only doubles");
      return std::nullopt;
    }

    if (ui_choices_json.size() != values_json.size())
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, enum option 'ui_choices' and "
                           "'values' does not match!");
      return std::nullopt;
    }

    for (const auto& ui_choice_json : ui_choices_json)
    {
      option.m_ui_choices.push_back(ui_choice_json.to_str());
    }

    for (const auto& value_json : values_json)
    {
      option.m_values_for_choices.push_back(static_cast<s32>(value_json.get<double>()));
    }

    const auto default_index_iter = obj.find("default_index");
    if (default_index_iter != obj.end() && default_index_iter->second.is<double>())
    {
      option.m_default_index = static_cast<u32>(default_index_iter->second.get<double>());
      option.m_index = option.m_default_index;
    }

    return option;
  }
  else if (type == "float")
  {
    FloatOption<1> option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    if (!process_float_precision(option))
      return std::nullopt;
    return option;
  }
  else if (type == "float2")
  {
    FloatOption<2> option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    if (!process_float_precision(option))
      return std::nullopt;
    return option;
  }
  else if (type == "float3")
  {
    FloatOption<3> option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    if (!process_float_precision(option))
      return std::nullopt;
    return option;
  }
  else if (type == "float4")
  {
    FloatOption<4> option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    if (!process_float_precision(option))
      return std::nullopt;
    return option;
  }
  else if (type == "int")
  {
    IntOption<1> option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    return option;
  }
  else if (type == "int2")
  {
    IntOption<2> option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    return option;
  }
  else if (type == "int3")
  {
    IntOption<3> option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    return option;
  }
  else if (type == "int4")
  {
    IntOption<4> option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    return option;
  }
  else if (type == "rgb")
  {
    ColorOption option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    return option;
  }
  else if (type == "rgba")
  {
    ColorAlphaOption option;
    if (!process_common(option))
      return std::nullopt;
    if (!ProcessNumeric(option, obj))
      return std::nullopt;
    return option;
  }
  else
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, option invalid type '{}'",
                  type);
  }

  return std::nullopt;
}

void SerializeOptionToProfile(picojson::object& obj, const ShaderConfigOption& option)
{
  const auto to_serialized_array = [](const auto& val) -> picojson::array {
    picojson::array result;
    for (const auto& v : val)
    {
      result.push_back(picojson::value{static_cast<double>(v)});
    }
    return result;
  };
  std::visit(overloaded{[&](const EnumChoiceOption& o) {
                          obj["index"] = picojson::value{static_cast<double>(o.m_index)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const FloatOption<1>& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const FloatOption<2>& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const FloatOption<3>& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const FloatOption<4>& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const IntOption<1>& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const IntOption<2>& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const IntOption<3>& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const IntOption<4>& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const BoolOption& o) {
                          obj["value"] = picojson::value{o.m_value};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const ColorOption& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        },
                        [&](const ColorAlphaOption& o) {
                          obj["values"] = picojson::value{to_serialized_array(o.m_value)};
                          obj["name"] = picojson::value{o.m_common.m_name};
                        }},
             option);
}

void DeserializeOptionFromProfile(const picojson::object& obj, ShaderConfigOption& option)
{
  const auto set_values = [&](auto& values) {
    if (auto it = obj.find("values"); it != obj.end())
    {
      if (it->second.is<picojson::array>())
      {
        auto values_serialized = it->second.get<picojson::array>();
        if (values_serialized.size() == values.size())
        {
          for (std::size_t i = 0; i < values_serialized.size(); i++)
          {
            if (values_serialized[i].is<double>())
            {
              values[i] = static_cast<typename std::remove_reference<decltype(values[i])>::type>(
                  values_serialized[i].get<double>());
            }
          }
        }
      }
    }
  };
  const auto name_matches = [&](const auto& o) -> bool {
    if (const auto it = obj.find("name"); it != obj.end())
    {
      if (it->second.is<std::string>())
      {
        return it->second.to_str() == o.m_common.m_name;
      }
    }
    return false;
  };

  std::visit(overloaded{[&](EnumChoiceOption& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          if (auto it = obj.find("index"); it != obj.end())
                          {
                            if (it->second.is<double>())
                            {
                              o.m_index = static_cast<u32>(it->second.get<double>());
                            }
                          }
                        },
                        [&](FloatOption<1>& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        },
                        [&](FloatOption<2>& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        },
                        [&](FloatOption<3>& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        },
                        [&](FloatOption<4>& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        },
                        [&](IntOption<1>& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        },
                        [&](IntOption<2>& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        },
                        [&](IntOption<3>& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        },
                        [&](IntOption<4>& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        },
                        [&](BoolOption& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          if (auto it = obj.find("value"); it != obj.end())
                          {
                            if (it->second.is<bool>())
                            {
                              o.m_value = it->second.get<bool>();
                            }
                          }
                        },
                        [&](ColorOption& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        },
                        [&](ColorAlphaOption& o) {
                          if (!name_matches(o))
                          {
                            return;
                          }
                          set_values(o.m_value);
                        }},
             option);
}
}  // namespace VideoCommon::PE
