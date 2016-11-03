// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

namespace WiimoteEmu
{
struct ExtensionReg;

class Guitar : public Attachment
{
public:
  Guitar(WiimoteEmu::ExtensionReg& _reg);
  void GetState(u8* const data) override;
  bool IsButtonPressed() const override;

  enum
  {
    BUTTON_PLUS = 0x04,
    BUTTON_MINUS = 0x10,
    BAR_DOWN = 0x40,

    BAR_UP = 0x0100,
    FRET_YELLOW = 0x0800,
    FRET_GREEN = 0x1000,
    FRET_BLUE = 0x2000,
    FRET_RED = 0x4000,
    FRET_ORANGE = 0x8000,
  };

private:
  Buttons* m_buttons;
  Buttons* m_frets;
  Buttons* m_strum;
  Triggers* m_whammy;
  AnalogStick* m_stick;
	Slider* m_touchbar;

	const std::map <const ControlState, const u8> m_controlCodes
	{//values determined using a PS3 Guitar Hero 5 controller, which maps the touchbar to Zr on Windows
		{0.5, 0x0F},//not touching
		{-0.753906, 0x04},//top fret
		{-0.4375, 0x07},//top and second fret
		{-0.097656, 0x0A},//second fret
		{0.195313, 0x0C},//second and third fret
		{0.601563, 0x12},//third fret
		{0.683594, 0x14},//third and fourth fret
		{0.789063, 0x17},//fourth fret
		{0.902344, 0x1A},//fourth and bottom fret
		{1.0, 0x1F}//bottom fret
};
};
}
