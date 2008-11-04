// Copyright (C) 2003-2008 Dolphin Project.

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
		cmdID					= 0x00,
		cmdStatus				= 0x40,
		cmdSetStatus			= 0x80,
		cmdGetBuffer			= 0x20,
		cmdWriteBuffer			= 0x82,
		cmdReadStatus			= 0x83,		
		cmdReadID				= 0x85,
		cmdReadErrorBuffer		= 0x86,
		cmdWakeUp				= 0x87,
		cmdSleep				= 0x88,		
		cmdClearStatus			= 0x89,
		cmdSectorErase			= 0xF1,
		cmdPageProgram			= 0xF2,
		cmdExtraByteProgram		= 0xF3,
		cmdChipErase			= 0xF4,
	};

	// STATE_TO_SAVE
	int interruptSwitch;
	int command;
	union uStatus
	{
		u16 U16;
		u8 U8[2];
	};
	int Index;
	u32 m_uPosition;
	u32 formatDelay;
	uStatus Status;
	
	//! memory card parameters 
	unsigned int ID;
	unsigned int address;	
	
protected:
	virtual void TransferByte(u8 &byte);
};

void SetMic(bool Value);
bool GetMic();

#endif

