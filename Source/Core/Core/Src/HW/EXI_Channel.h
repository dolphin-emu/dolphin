// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _EXICHANNEL_H
#define _EXICHANNEL_H

#include "Common.h"

#include "EXI_Device.h"

#ifdef _WIN32
#pragma warning(disable:4201)
#endif

class CEXIChannel
{
private:

	enum
	{
		EXI_STATUS		= 0,
		EXI_DMAADDR		= 1,
		EXI_DMALENGTH	= 2,
		EXI_DMACONTROL	= 3,
		EXI_IMMDATA		= 4
	};
	const char* Debug_GetRegisterName(u32 _register)
	{
		switch (_register)
		{
		case EXI_STATUS:		return "STATUS";
		case EXI_DMAADDR:		return "DMAADDR";
		case EXI_DMALENGTH:		return "DMALENGTH";
		case EXI_DMACONTROL:	return "DMACONTROL";
		case EXI_IMMDATA:		return "IMMDATA";
		default:				return "!!! Unknown EXI Register !!!";
		}
	}

	// EXI Status Register - "Channel Parameter Register"
	union UEXI_STATUS
	{
		u32 Hex;
		// DO NOT obey the warning and give this struct a name. Things will fail.
		struct
		{
		// Indentation Meaning:
		//	Channels 0, 1, 2
		//		Channels 0, 1 only
		//			Channel 0 only
			u32 EXIINTMASK		: 1;
			u32 EXIINT			: 1;
			u32 TCINTMASK		: 1;
			u32 TCINT			: 1;
			u32 CLK				: 3;
			u32 CHIP_SELECT		: 3; // CS1 and CS2 are Channel 0 only
				u32 EXTINTMASK	: 1;
				u32 EXTINT		: 1;
				u32 EXT			: 1; // External Insertion Status (1: External EXI device present)
					u32 ROMDIS	: 1; // ROM Disable
			u32					:18;
		};
		UEXI_STATUS() {Hex = 0;}
		UEXI_STATUS(u32 _hex) {Hex = _hex;}
	};

	// EXI Control Register
	union UEXI_CONTROL
	{
		u32 Hex;
		struct
		{
			u32 TSTART		: 1;
			u32 DMA			: 1;
			u32 RW			: 2;
			u32 TLEN		: 2;
			u32				:26;
		};
	};

	// STATE_TO_SAVE
	UEXI_STATUS		m_Status;
	u32				m_DMAMemoryAddress;
	u32				m_DMALength;
	UEXI_CONTROL	m_Control;
	u32				m_ImmData;

	// Devices
	enum
	{
		NUM_DEVICES = 3
	};

	IEXIDevice* m_pDevices[NUM_DEVICES];

	// Since channels operate a bit differently from each other
	u32 m_ChannelId;

	int updateInterrupts;

	static void UpdateInterrupts(u64 userdata, int cyclesLate);
public:
	// get device
	IEXIDevice* GetDevice(const u8 _CHIP_SELECT);
	IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex=-1);

	CEXIChannel(u32 ChannelId);
	~CEXIChannel();

	void AddDevice(const TEXIDevices device_type, const int device_num);
	void AddDevice(IEXIDevice* pDevice, const int device_num, bool notifyPresenceChanged=true);

	// Remove all devices
	void RemoveDevices();

	void Read32(u32& _uReturnValue, const u32 _iRegister);
	void Write32(const u32 _iValue, const u32 _iRegister);

	void Update();
	bool IsCausingInterrupt();
	void DoState(PointerWrap &p);
	void PauseAndLock(bool doLock, bool unpauseOnUnlock);

	// This should only be used to transition interrupts from SP1 to Channel 2
	void SetEXIINT(bool exiint) { m_Status.EXIINT = !!exiint; }
};

#endif
