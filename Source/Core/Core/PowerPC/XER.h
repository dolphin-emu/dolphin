// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

namespace PowerPC
{
// XER, reformatted into byte fields for easier access.
struct XER
{
  u8 ca;
  union
  {
    u8 so_ov;
    BitField<0, 1, u8> ov;
    BitField<1, 1, u8> so;
  };
  // The Broadway CPU implements bits 16-23 of the XER register... even though it doesn't support
  // lscbx
  u16 stringctrl;

  UReg_XER Get() const
  {
    u32 xer = 0;
    xer |= stringctrl;
    xer |= ca << XER_CA_SHIFT;
    xer |= so_ov << XER_OV_SHIFT;
    return UReg_XER{xer};
  }

  void Set(UReg_XER new_xer)
  {
    stringctrl = new_xer.BYTE_COUNT + (new_xer.BYTE_CMP << 8);
    ca = new_xer.CA;
    so_ov = (new_xer.SO << 1) + new_xer.OV;
  }

  void SetSO(bool value) { so = so | static_cast<u32>(value); }

  void SetOV(bool value)
  {
    ov = static_cast<u32>(value);
    SetSO(value);
  }
};
// XER must be standard layout in order for offsetof to work, which is used by the JITs
static_assert(std::is_standard_layout<XER>(), "XER must be standard layout");

}  // namespace PowerPC
