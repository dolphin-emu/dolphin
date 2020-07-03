#include "SlippiPad.h"

// TODO: Confirm the default and padding values are right
static u8 emptyPad[SLIPPI_PAD_FULL_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

SlippiPad::SlippiPad(int32_t frame)
{
  this->frame = frame;
  memcpy(this->padBuf, emptyPad, SLIPPI_PAD_FULL_SIZE);
}

SlippiPad::SlippiPad(int32_t frame, u8* padBuf) : SlippiPad(frame)
{
  // Overwrite the data portion of the pad
  memcpy(this->padBuf, padBuf, SLIPPI_PAD_DATA_SIZE);
}

SlippiPad::~SlippiPad()
{
  // Do nothing?
}
