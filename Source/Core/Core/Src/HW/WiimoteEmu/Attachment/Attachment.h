// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _ATTACHMENT_EMU_H_
#define _ATTACHMENT_EMU_H_

#include "ControllerEmu.h"
#include "../WiimoteEmu.h"

namespace WiimoteEmu
{

class Attachment : public ControllerEmu
{
public:
	Attachment( const char* const _name, WiimoteEmu::ExtensionReg& _reg );

	virtual void GetState( u8* const data, const bool focus = true ) {}
	void Reset();
	std::string GetName() const;

	const char*	const			name;
	WiimoteEmu::ExtensionReg&	reg;

	u8 id[6];
	u8 calibration[0x10];
};

class None : public Attachment
{
public:
	None( WiimoteEmu::ExtensionReg& _reg );
};

}

#endif
