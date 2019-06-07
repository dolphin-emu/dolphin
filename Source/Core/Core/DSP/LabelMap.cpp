// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/LabelMap.h"

#include <algorithm>
#include <string>
#include <vector>

#include "Core/DSP/DSPTables.h"

namespace DSP
{
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

void LabelMap::RegisterLabel(const std::string& label, u16 lval, LabelType type)
{
  u16 old_value;
  if (GetLabelValue(label, &old_value) && old_value != lval)
  {
    printf("WARNING: Redefined label %s to %04x - old value %04x\n", label.c_str(), lval,
           old_value);
    DeleteLabel(label);
  }
  labels.emplace_back(label, lval, type);
}

void LabelMap::DeleteLabel(const std::string& label)
{
  const auto iter = std::find_if(labels.cbegin(), labels.cend(),
                                 [&label](const auto& entry) { return entry.name == label; });

  if (iter == labels.cend())
    return;

  labels.erase(iter);
}

bool LabelMap::GetLabelValue(const std::string& name, u16* value, LabelType type) const
{
  for (auto& label : labels)
  {
    if (!name.compare(label.name))
    {
      if (type & label.type)
      {
        *value = label.addr;
        return true;
      }
      else
      {
        printf("WARNING: Wrong label type requested. %s\n", name.c_str());
      }
    }
  }
  return false;
}

void LabelMap::Clear()
{
  labels.clear();
}
}  // namespace DSP
