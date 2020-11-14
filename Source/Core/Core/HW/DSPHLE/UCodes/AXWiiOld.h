// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/DSPHLE/UCodes/AXWii.h"

namespace DSP::HLE
{
class DSPHLE;

class AXWiiOldUCode : public AXWiiUCode
{
public:
  AXWiiOldUCode(DSPHLE* dsphle, u32 crc);
  ~AXWiiOldUCode() override;

protected:
  void HandleCommandList() override;
  void ProcessPBList(u32 pb_addr);

private:
  // A lot of these are similar to the new version, but there is an offset in
  // the command ids due to the PB_ADDR command (which was removed from the
  // new AXWii).
  enum CmdType
  {
    CMD_SETUP = 0x00,
    CMD_ADD_TO_LR = 0x01,
    CMD_SUB_TO_LR = 0x02,
    CMD_ADD_SUB_TO_LR = 0x03,
    CMD_PB_ADDR = 0x04,
    CMD_PROCESS = 0x05,
    CMD_MIX_AUXA = 0x06,
    CMD_MIX_AUXB = 0x07,
    CMD_MIX_AUXC = 0x08,
    CMD_UPL_AUXA_MIX_LRSC = 0x09,
    CMD_UPL_AUXB_MIX_LRSC = 0x0a,
    CMD_UNK_0B = 0x0B,
    CMD_OUTPUT = 0x0C,  // no volume!
    CMD_OUTPUT_DPL2 = 0x0D,
    CMD_WM_OUTPUT = 0x0E,
    CMD_END = 0x0F
  };
};
}  // namespace DSP::HLE
