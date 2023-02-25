// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

namespace DSP
{
enum LabelType
{
  LABEL_IADDR = 1,  // Jump addresses, etc
  LABEL_DADDR = 2,  // Data addresses, etc
  LABEL_VALUE = 4,
  LABEL_ANY = 0xFF,
};

class LabelMap
{
public:
  LabelMap();
  ~LabelMap();

  void RegisterDefaults();
  bool RegisterLabel(std::string label, u16 lval, LabelType type = LABEL_VALUE);
  void DeleteLabel(std::string_view label);
  std::optional<u16> GetLabelValue(std::string_view name, LabelType type = LABEL_ANY) const;
  void Clear();

private:
  struct Label;
  std::vector<Label> labels;
};
}  // namespace DSP
