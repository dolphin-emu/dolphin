// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
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
  struct label_t
  {
    label_t(const std::string& lbl, s32 address, LabelType ltype)
        : name(lbl), addr(address), type(ltype)
    {
    }
    std::string name;
    s32 addr;
    LabelType type;
  };
  std::vector<label_t> labels;

public:
  LabelMap();
  ~LabelMap() {}
  void RegisterDefaults();
  void RegisterLabel(const std::string& label, u16 lval, LabelType type = LABEL_VALUE);
  void DeleteLabel(const std::string& label);
  bool GetLabelValue(const std::string& label, u16* value, LabelType type = LABEL_ANY) const;
  void Clear();
};
}  // namespace DSP
