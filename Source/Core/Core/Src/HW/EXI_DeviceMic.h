// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _EXI_DEVICEMIC_H
#define _EXI_DEVICEMIC_H

class CEXIMic : public IEXIDevice
{
public:
	CEXIMic(int _Index);
	virtual ~CEXIMic();
	void SetCS(int cs);
	void Update();
	bool IsInterruptSet();
	bool IsPresent();

private:

	enum
	{
		EXI_DEVTYPE_MIC	= 0x0A000000
	};

	enum 
	{
		cmdID			= 0x00,
		cmdGetStatus	= 0x40,
		cmdSetStatus	= 0x80,
		cmdGetBuffer	= 0x20,
		cmdWakeUp		= 0xFF,
	};

	// STATE_TO_SAVE
	int interruptSwitch;
	int command;
	union uStatus
	{
		u16 U16;
		u8 U8[2];
		struct
		{
			unsigned			:8; // Unknown
			unsigned button		:1; // 1: Button Pressed
			unsigned unk1		:1; // 1 ? Overflow?
			unsigned unk2		:1; // Unknown related to 0 and 15 values It seems
			unsigned sRate		:2; // Sample Rate, 00-11025, 01-22050, 10-44100, 11-??
			unsigned pLength	:2; // Period Length, 00-32, 01-64, 10-128, 11-???
			unsigned sampling	:1; // If We Are Sampling or Not
		};
	};
	int Index;
	u32 m_uPosition;
	uStatus Status;	
	
protected:
	virtual void TransferByte(u8 &byte);
};

void SetMic(bool Value);

#endif
