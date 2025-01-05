#include <memory>

#include "SlippiPad.h"

#include <string.h>

// TODO: Confirm the default and padding values are right
static u8 emptyPad[SLIPPI_PAD_FULL_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

SlippiPad::SlippiPad(int32_t frame)
{
  this->frame = frame;
  this->checksum = 0;
  this->checksum_frame = 0;
  memcpy(this->pad_buf, emptyPad, SLIPPI_PAD_FULL_SIZE);
}

SlippiPad::SlippiPad(int32_t frame, u8* pad_buf) : SlippiPad(frame)
{
  // Overwrite the data portion of the pad
  memcpy(this->pad_buf, pad_buf, SLIPPI_PAD_DATA_SIZE);
}

SlippiPad::SlippiPad(int32_t frame, s32 checksum_frame, u32 checksum, u8* pad_buf)
    : SlippiPad(frame, pad_buf)
{
  this->checksum_frame = checksum_frame;
  this->checksum = checksum;
}

SlippiPad::~SlippiPad()
{
  // Do nothing?
}
