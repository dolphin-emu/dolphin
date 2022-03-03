// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/LabelMap.h"

#include <algorithm>
#include <string>
#include <vector>

#include "Common/Logging/Log.h"
#include "Core/DSP/DSPTables.h"

namespace DSP
{
struct LabelMap::Label
{
  Label(std::string lbl, s32 address, LabelType ltype)
      : name(std::move(lbl)), addr(address), type(ltype)
  {
  }
  std::string name;
  s32 addr;
  LabelType type;
};

LabelMap::LabelMap() = default;

LabelMap::~LabelMap() = default;

void LabelMap::RegisterDefaults()
{
  for (const auto& reg_name_label : regnames)
  {
    if (reg_name_label.name != nullptr)
      RegisterLabel(reg_name_label.name, reg_name_label.addr);
  }

  for (const auto& predefined_label : pdlabels)
  {
    if (predefined_label.name != nullptr)
      RegisterLabel(predefined_label.name, predefined_label.addr);
  }
}

void LabelMap::RegisterLabel(std::string label, u16 lval, LabelType type)
{
  const std::optional<u16> old_value = GetLabelValue(label);
  if (old_value && old_value != lval)
  {
    WARN_LOG_FMT(AUDIO, "Redefined label {} to {:04x} - old value {:04x}\n", label, lval,
                 *old_value);
    DeleteLabel(label);
  }
  labels.emplace_back(std::move(label), lval, type);
}

void LabelMap::DeleteLabel(std::string_view label)
{
  const auto iter = std::find_if(labels.cbegin(), labels.cend(),
                                 [&label](const auto& entry) { return entry.name == label; });

  if (iter == labels.cend())
    return;

  labels.erase(iter);
}

std::optional<u16> LabelMap::GetLabelValue(std::string_view name, LabelType type) const
{
  for (const auto& label : labels)
  {
    if (name == label.name)
    {
      if ((type & label.type) != 0)
      {
        return label.addr;
      }
      else
      {
        WARN_LOG_FMT(AUDIO, "Wrong label type requested. {}\n", name);
      }
    }
  }

  return std::nullopt;
}

void LabelMap::Clear()
{
  labels.clear();
}
}  // namespace DSP
