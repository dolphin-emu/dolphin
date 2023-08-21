#pragma once

#include "Common/CommonTypes.h"

#define SLIPPI_PAD_FULL_SIZE 0xC
#define SLIPPI_PAD_DATA_SIZE 0x8

class SlippiPad
{
public:
  SlippiPad(int32_t frame);
  SlippiPad(int32_t frame, u8* pad_buf);
  SlippiPad(int32_t frame, s32 checksum_frame, u32 checksum, u8* pad_buf);
  ~SlippiPad();

  s32 frame;
  s32 checksum_frame;
  u32 checksum;
  u8 pad_buf[SLIPPI_PAD_FULL_SIZE];
};
