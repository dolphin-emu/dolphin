#pragma once

#include "Common/CommonTypes.h"

#define SLIPPI_PAD_FULL_SIZE 0xC
#define SLIPPI_PAD_DATA_SIZE 0x8

class SlippiPad
{
public:
  SlippiPad(int32_t frame);
  SlippiPad(int32_t frame, u8* padBuf);
  ~SlippiPad();

  int32_t frame;
  u8 padBuf[SLIPPI_PAD_FULL_SIZE];
};

