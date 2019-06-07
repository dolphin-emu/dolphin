// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  void RegisterLabel(std::string label, u16 lval, LabelType type = LABEL_VALUE);
  void DeleteLabel(std::string_view label);
  std::optional<u16> GetLabelValue(const std::string& label, LabelType type = LABEL_ANY) const;
  void Clear();

private:
  struct Label;
  std::vector<Label> labels;
};
}  // namespace DSP
