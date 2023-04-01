// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerEmu.h"

namespace WiimoteEmu
{

struct ExtensionReg;

class Attachment : public ControllerEmu
{
public:
	Attachment(const char* const _name, WiimoteEmu::ExtensionReg& _reg);

	virtual void GetState(u8* const data) {}
	virtual bool IsButtonPressed() const { return false; }
	void Reset();
	std::string GetName() const override;

	const char* const         name;
	WiimoteEmu::ExtensionReg& reg;

	u8 id[6];
	u8 calibration[0x10];

protected:

	// Default radius for attachment analog sticks.
	static constexpr ControlState DEFAULT_ATTACHMENT_STICK_RADIUS = 1.0;
};

class None : public Attachment
{
public:
	None(WiimoteEmu::ExtensionReg& _reg);
};

}
