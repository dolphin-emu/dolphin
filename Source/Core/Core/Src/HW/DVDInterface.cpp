// Copyright (C) 2003 Dolphin Project.

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

#include "Common.h" // Common
#include "ChunkFile.h"
#include "../ConfigManager.h"
#include "../CoreTiming.h"

#include "StreamADPCM.h" // Core
#include "DVDInterface.h"
#include "../PowerPC/PowerPC.h"
#include "ProcessorInterface.h"
#include "Memmap.h"
#include "Thread.h"
#include "../VolumeHandler.h"

namespace DVDInterface
{

// internal hardware addresses
enum
{
	DI_STATUS_REGISTER			= 0x00,
	DI_COVER_REGISTER			= 0x04,
	DI_COMMAND_0				= 0x08,
	DI_COMMAND_1				= 0x0C,
	DI_COMMAND_2				= 0x10,
	DI_DMA_ADDRESS_REGISTER		= 0x14,
	DI_DMA_LENGTH_REGISTER		= 0x18, 
	DI_DMA_CONTROL_REGISTER		= 0x1C,
	DI_IMMEDIATE_DATA_BUFFER	= 0x20,
	DI_CONFIG_REGISTER			= 0x24
};


// DVD IntteruptTypes
enum DVDInterruptType
{
	INT_DEINT		= 0,
	INT_TCINT		= 1,
	INT_BRKINT		= 2,
	INT_CVRINT      = 3,
};

// debug commands which may be ORd
enum
{
	STOP_DRIVE	= 0,
	START_DRIVE	= 0x100,
	ACCEPT_COPY	= 0x4000,
	DISC_CHECK	= 0x8000,
};

// DI Status Register
union UDISR	
{
	u32 Hex;
	struct
	{
		unsigned BREAK          :  1;	// Stop the Device + Interrupt
		unsigned DEINITMASK     :  1;	// Access Device Error Int Mask
		unsigned DEINT          :  1;	// Access Device Error Int
		unsigned TCINTMASK      :  1;	// Transfer Complete Int Mask
		unsigned TCINT          :  1;	// Transfer Complete Int
		unsigned BRKINTMASK     :  1;
		unsigned BRKINT         :  1;	// w 1: clear brkint
		unsigned                : 25;
	};
	UDISR() {Hex = 0;}
	UDISR(u32 _hex) {Hex = _hex;}
};

// DI Cover Register
union UDICVR
{
	u32 Hex;
	struct
	{
		unsigned CVR            :  1;	// 0: Cover closed	1: Cover open
		unsigned CVRINTMASK	    :  1;	// 1: Interrupt enabled
		unsigned CVRINT         :  1;	// r 1: Interrupt requested w 1: Interrupt clear
		unsigned                : 29;
	};
	UDICVR() {Hex = 0;}
	UDICVR(u32 _hex) {Hex = _hex;}
};

union UDICMDBUF
{
	u32 Hex;
	struct
	{
		u8 CMDBYTE3;
		u8 CMDBYTE2;
		u8 CMDBYTE1;
		u8 CMDBYTE0;
	};
};

// DI DMA Address Register
union UDIMAR
{
	u32 Hex;
	struct
	{
		unsigned Zerobits	:	5; // Must be zero (32byte aligned)
		unsigned			:	27;
	};
	struct
	{
		unsigned Address	:	26;
		unsigned			:	6;
	};
};

// DI DMA Address Length Register
union UDILENGTH
{
	u32 Hex;
	struct
	{
		unsigned Zerobits	:	5; // Must be zero (32byte aligned)
		unsigned			:	27;
	};
	struct
	{
		unsigned Length		:	26;
		unsigned			:	6;
	};
};

// DI DMA Control Register
union UDICR
{
	u32 Hex;
	struct
	{
		unsigned TSTART		:	1;	// w:1 start   r:0 ready
		unsigned DMA		:	1;	// 1: DMA Mode    0: Immediate Mode (can only do Access Register Command)
		unsigned RW			:	1;	// 0: Read Command (DVD to Memory)  1: Write COmmand (Memory to DVD)
		unsigned			:  29;
	};
};

union UDIIMMBUF
{
	u32 Hex;
	struct
	{
		u8 REGVAL3;
		u8 REGVAL2;
		u8 REGVAL1;
		u8 REGVAL0;
	};
};

// DI Config Register
union UDICFG
{
	u32 Hex;
	struct
	{
		unsigned CONFIG		:	8;
		unsigned			:  24;
	};
	UDICFG() {Hex = 0;}
	UDICFG(u32 _hex) {Hex = _hex;}
};


// STATE_TO_SAVE
// hardware registers
static UDISR		m_DISR;
static UDICVR		m_DICVR;
static UDICMDBUF	m_DICMDBUF[3];
static UDIMAR		m_DIMAR;
static UDILENGTH	m_DILENGTH;
static UDICR		m_DICR;
static UDIIMMBUF	m_DIIMMBUF;
static UDICFG		m_DICFG;

static u32			AudioStart;
static u32			AudioPos;
static u32			AudioLength;

u32	 g_ErrorCode = 0;
bool g_bDiscInside = false;
bool g_bStream = false;

// GC-AM only
static unsigned char media_buffer[0x40];

Common::CriticalSection dvdread_section;

static bool g_dvdQuitSignal = false;
static Common::Event g_dvdAlert;
static Common::Thread* g_dvdThread = NULL;

static int changeDisc;
void ChangeDiscCallback(u64 userdata, int cyclesLate);

void DoState(PointerWrap &p)
{
	p.Do(m_DISR);
	p.Do(m_DICVR);
	p.DoArray(m_DICMDBUF, 3);
	p.Do(m_DIMAR);
	p.Do(m_DILENGTH);
	p.Do(m_DICR);
	p.Do(m_DIIMMBUF);
	p.Do(m_DICFG);

	p.Do(AudioStart);
	p.Do(AudioPos);
	p.Do(AudioLength);

	p.Do(g_ErrorCode);
	p.Do(g_bDiscInside);
}

void UpdateInterrupts();
void GenerateDVDInterrupt(DVDInterruptType _DVDInterrupt);
void ExecuteCommand(UDICR& _DICR);

static int et_GenerateDVDInterrupt;

static void GenerateDVDInterruptCallback(u64 userdata, int cyclesLate)
{
	GenerateDVDInterrupt((DVDInterruptType)userdata);
}

static void GenerateDVDInterrupt_Threadsafe(DVDInterruptType type)
{
	CoreTiming::ScheduleEvent_Threadsafe(0, et_GenerateDVDInterrupt, type);
}

static THREAD_RETURN DVDThreadFunc(void* arg)
{
	for (;;)
	{
		g_dvdAlert.Wait();

		if (g_dvdQuitSignal)
			break;

		if (m_DICR.TSTART)
			ExecuteCommand(m_DICR);
	}

	return 0;
}

void Init()
{
	m_DISR.Hex		= 0;
	m_DICVR.Hex		= 0;
	m_DICMDBUF[0].Hex= 0;
	m_DICMDBUF[1].Hex= 0;
	m_DICMDBUF[2].Hex= 0;
	m_DIMAR.Hex		= 0;
	m_DILENGTH.Hex	= 0;
	m_DICR.Hex		= 0;
	m_DIIMMBUF.Hex	= 0;
	m_DICFG.Hex		= 0;
	m_DICFG.CONFIG	= 1; // Disable bootrom descrambler

	AudioStart		= 0;
	AudioPos		= 0;
	AudioLength		= 0;

	et_GenerateDVDInterrupt = CoreTiming::RegisterEvent("DVDint", GenerateDVDInterruptCallback);

	g_dvdAlert.Init();
	g_dvdThread = new Common::Thread(DVDThreadFunc, NULL);

	changeDisc = CoreTiming::RegisterEvent("ChangeDisc", ChangeDiscCallback);
}

void Shutdown()
{
	if (g_dvdThread)
	{
		g_dvdQuitSignal = true;
		g_dvdAlert.Set();
#ifdef _WIN32
		g_dvdThread->WaitForDeath(3000);
#else
		g_dvdThread->WaitForDeath();
#endif

		delete g_dvdThread;
		g_dvdThread = NULL;

		g_dvdAlert.Shutdown();
		g_dvdQuitSignal = false;
	}
}

void SetDiscInside(bool _DiscInside)
{
	g_bDiscInside = _DiscInside;
}

bool IsDiscInside()
{	
	return g_bDiscInside;
}

// Take care of all logic of "swapping discs"
// We want this in the "backend", NOT the gui
// any !empty string will be deleted to ensure
// that the userdata string exists when called
void ChangeDiscCallback(u64 userdata, int cyclesLate)
{
	std::string FileName((const char*)userdata);
	SetDiscInside(false);
	SetLidOpen();

	std::string& SavedFileName = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strFilename;

	if (FileName.empty())
	{
		// Empty the drive
		VolumeHandler::EjectVolume();
	}
	else
	{
		delete [] (char *) userdata;
		if (VolumeHandler::SetVolumeName(FileName))
		{
			// Save the new ISO file name
			SavedFileName = FileName;
		}
		else
		{
			PanicAlert("Invalid file \n %s", FileName.c_str());

			// Put back the old one
			VolumeHandler::SetVolumeName(SavedFileName);
		}
	}

	SetLidOpen(false);
	SetDiscInside(VolumeHandler::IsValid());
}

void ChangeDisc(const char* _FileName)
{
	const char* NoDisc = "";
	CoreTiming::ScheduleEvent_Threadsafe(0, changeDisc, (u64)NoDisc);
	CoreTiming::ScheduleEvent_Threadsafe(500000000, changeDisc, (u64)_FileName);
}

void SetLidOpen(bool _bOpen)
{
	m_DICVR.CVR = _bOpen ? 1 : 0;

	GenerateDVDInterrupt_Threadsafe(INT_CVRINT);
}

bool IsLidOpen()
{	
	return (m_DICVR.CVR == 1);
}

void ClearCoverInterrupt()
{
	m_DICVR.CVRINT = 0;
}

bool DVDRead(u32 _iDVDOffset, u32 _iRamAddress, u32 _iLength)
{
	// We won't need the crit sec when DTK streaming has been rewritten correctly.
	dvdread_section.Enter();
	bool retval = VolumeHandler::ReadToPtr(Memory::GetPointer(_iRamAddress), _iDVDOffset, _iLength);
	dvdread_section.Leave();
	return retval;
}

bool DVDReadADPCM(u8* _pDestBuffer, u32 _iNumSamples)
{
	if (AudioPos == 0)
	{
		//MessageBox(0,"DVD: Trying to stream from 0", "bah", 0);
		memset(_pDestBuffer, 0, _iNumSamples); // probably __AI_SRC_INIT :P
		return false;
	}
	_iNumSamples &= ~31;
	dvdread_section.Enter();
	VolumeHandler::ReadToPtr(_pDestBuffer, AudioPos, _iNumSamples);
	dvdread_section.Leave();

	//
	// FIX THIS
	//
	// loop check
	//
	AudioPos += _iNumSamples;
	if (AudioPos >= AudioStart + AudioLength)
	{
		AudioPos = AudioStart;
		NGCADPCM::InitFilter();
	}

	//WARN_LOG(DVDINTERFACE,"ReadADPCM");
	return true;
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	switch (_iAddress & 0xFF)
	{
	case DI_STATUS_REGISTER:		_uReturnValue = m_DISR.Hex; break;
	case DI_COVER_REGISTER:			_uReturnValue = m_DICVR.Hex; break;
	case DI_COMMAND_0:				_uReturnValue = m_DICMDBUF[0].Hex; break;
	case DI_COMMAND_1:				_uReturnValue = m_DICMDBUF[1].Hex; break;
	case DI_COMMAND_2:				_uReturnValue = m_DICMDBUF[2].Hex; break;
	case DI_DMA_ADDRESS_REGISTER:	_uReturnValue = m_DIMAR.Hex; break;
	case DI_DMA_LENGTH_REGISTER:	_uReturnValue = m_DILENGTH.Hex; break;
	case DI_DMA_CONTROL_REGISTER:	_uReturnValue = m_DICR.Hex; break;
	case DI_IMMEDIATE_DATA_BUFFER:	_uReturnValue = m_DIIMMBUF.Hex; break;
	case DI_CONFIG_REGISTER:		_uReturnValue = m_DICFG.Hex; break;

	default:
		_dbg_assert_(DVDINTERFACE, 0);		
		_uReturnValue = 0;
		break;
	}
	DEBUG_LOG(DVDINTERFACE, "(r32): 0x%08x - 0x%08x", _iAddress, _uReturnValue);
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	DEBUG_LOG(DVDINTERFACE, "(w32): 0x%08x @ 0x%08x", _iValue, _iAddress);

	switch (_iAddress & 0xFF)
	{
	case DI_STATUS_REGISTER:
		{
			UDISR tmpStatusReg(_iValue);

			m_DISR.DEINITMASK	= tmpStatusReg.DEINITMASK;
			m_DISR.TCINTMASK	= tmpStatusReg.TCINTMASK;
			m_DISR.BRKINTMASK	= tmpStatusReg.BRKINTMASK;

			if (tmpStatusReg.DEINT)		m_DISR.DEINT = 0;
			if (tmpStatusReg.TCINT)		m_DISR.TCINT = 0;
			if (tmpStatusReg.BRKINT)	m_DISR.BRKINT = 0;

			if (tmpStatusReg.BREAK) 
			{
				_dbg_assert_(DVDINTERFACE, 0);
			}

			UpdateInterrupts();
		}
		break;

	case DI_COVER_REGISTER:	
		{
			UDICVR tmpCoverReg(_iValue);

			m_DICVR.CVRINTMASK = tmpCoverReg.CVRINTMASK;

			if (tmpCoverReg.CVRINT)	m_DICVR.CVRINT = 0;

			UpdateInterrupts();
		}
		break;

	case DI_COMMAND_0:				m_DICMDBUF[0].Hex = _iValue; break;
	case DI_COMMAND_1:				m_DICMDBUF[1].Hex = _iValue; break;
	case DI_COMMAND_2:				m_DICMDBUF[2].Hex = _iValue; break;

	case DI_DMA_ADDRESS_REGISTER:
		{
			m_DIMAR.Hex = _iValue; 
			_dbg_assert_msg_(DVDINTERFACE, m_DIMAR.Zerobits == 0, "DMA Addr not 32byte aligned!");
		}
		break;
	case DI_DMA_LENGTH_REGISTER:
		{
			m_DILENGTH.Hex = _iValue; 
			_dbg_assert_msg_(DVDINTERFACE, m_DILENGTH.Zerobits == 0, "DMA Length not 32byte aligned!");
		}
		break;
	case DI_DMA_CONTROL_REGISTER:	
		{
			m_DICR.Hex = _iValue;
			// The thread loop checks if TSTART is set, don't need to check here
			g_dvdAlert.Set();
		}
		break;

	case DI_IMMEDIATE_DATA_BUFFER:	m_DIIMMBUF.Hex = _iValue; break;

	case DI_CONFIG_REGISTER:
		{	
			UDICFG tmpConfigReg(_iValue);
			m_DICFG.CONFIG = tmpConfigReg.CONFIG;
			WARN_LOG(DVDINTERFACE, "Write to DICFG, should be read-only");
		}
		break;

	default:
		_dbg_assert_msg_(DVDINTERFACE, 0, "Write to unknown DI address 0x%08x", _iAddress);
		break;
	}
}

void UpdateInterrupts()
{
	if ((m_DISR.DEINT	& m_DISR.DEINITMASK) ||
		(m_DISR.TCINT	& m_DISR.TCINTMASK)  ||
		(m_DISR.BRKINT	& m_DISR.BRKINTMASK) ||
		(m_DICVR.CVRINT	& m_DICVR.CVRINTMASK))
	{
		ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DI, true);
	}
	else
	{
		ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DI, false);
	}
}

void GenerateDVDInterrupt(DVDInterruptType _DVDInterrupt)
{
	switch(_DVDInterrupt) 
	{
	case INT_DEINT:		m_DISR.DEINT	= 1; break;
	case INT_TCINT:		m_DISR.TCINT	= 1; break;
	case INT_BRKINT:	m_DISR.BRKINT	= 1; break;
	case INT_CVRINT:	m_DICVR.CVRINT	= 1; break;
	}

	UpdateInterrupts();
}

void ExecuteCommand(UDICR& _DICR)
{
//	_dbg_assert_(DVDINTERFACE, _DICR.RW == 0); // only DVD to Memory
	int GCAM = ((SConfig::GetInstance().m_SIDevice[0] == SI_AM_BASEBOARD)
		&& (SConfig::GetInstance().m_EXIDevice[2] == EXIDEVICE_AM_BASEBOARD))
		? 1 : 0;

	if (GCAM)
	{
		ERROR_LOG(DVDINTERFACE, "DVD: %08x, %08x, %08x, DMA=addr:%08x,len:%08x,ctrl:%08x",
			m_DICMDBUF[0], m_DICMDBUF[1], m_DICMDBUF[2], m_DIMAR, m_DILENGTH, m_DICR);
		// decrypt command. But we have a zero key, that simplifies things a lot.
		// If you get crazy dvd command errors, make sure 0x80000000 - 0x8000000c is zero'd
		m_DICMDBUF[0].Hex <<= 24;
	}


	switch (m_DICMDBUF[0].CMDBYTE0)
	{
	case DVDLowInquiry:
		if (GCAM)
		{
			// 0x29484100...
			// was 21 i'm not entirely sure about this, but it works well.
			m_DIIMMBUF.Hex = 0x21000000;
		}
		else
		{
			// small safety check, dunno if it's needed
			if ((m_DICMDBUF[1].Hex == 0) && (m_DILENGTH.Length == 0x20))
			{
				u8* driveInfo = Memory::GetPointer(m_DIMAR.Address);
				// gives the correct output in GCOS - 06 2001/08 (61)
				// there may be other stuff missing ?
				driveInfo[4] = 0x20;
				driveInfo[5] = 0x01;
				driveInfo[6] = 0x06;
				driveInfo[7] = 0x08;
				driveInfo[8] = 0x61;

				// Just for fun
				INFO_LOG(DVDINTERFACE, "Drive Info: %02x %02x%02x/%02x (%02x)",
					driveInfo[6], driveInfo[4], driveInfo[5], driveInfo[7], driveInfo[8]);
			}
		}
		break;

	// "Set Extension"...not sure what it does
	case 0x55:
		INFO_LOG(DVDINTERFACE, "SetExtension");
		break;

	// DMA Read from Disc
	case 0xA8:
		if (g_bDiscInside)
		{
			switch (m_DICMDBUF[0].CMDBYTE3)
			{
			case 0x00: // Read Sector
				{
					u32 iDVDOffset = m_DICMDBUF[1].Hex << 2;
					u32 iSrcLength = m_DICMDBUF[2].Hex;

					DEBUG_LOG(DVDINTERFACE, "Read: DVDOffset=%08x, DMABuffer=%08x, SrcLength=%08x, DMALength=%08x",
						iDVDOffset, m_DIMAR.Address, iSrcLength, m_DILENGTH.Length);
					_dbg_assert_(DVDINTERFACE, iSrcLength == m_DILENGTH.Length);

					if (GCAM)
					{
						if (iDVDOffset & 0x80000000) // read request to hardware buffer
						{
							switch (iDVDOffset)
							{
							case 0x80000000:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD STATUS (80000000)");
								for (unsigned int i = 0; i < m_DILENGTH.Length / 4; i++)
									Memory::Write_U32(0, m_DIMAR.Address + i * 4);
								break;
							case 0x80000040:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD STATUS (2) (80000040)");
								for (unsigned int i = 0; i < m_DILENGTH.Length / 4; i++)
									Memory::Write_U32(~0, m_DIMAR.Address + i * 4);
								Memory::Write_U32(0x00000020, m_DIMAR.Address); // DIMM SIZE, LE
								Memory::Write_U32(0x4743414D, m_DIMAR.Address + 4); // GCAM signature
								break;
							case 0x80000120:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ FIRMWARE STATUS (80000120)");
								for (unsigned int i = 0; i < m_DILENGTH.Length / 4; i++)
									Memory::Write_U32(0x01010101, m_DIMAR.Address + i * 4);
								break;
							case 0x80000140:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ FIRMWARE STATUS (80000140)");
								for (unsigned int i = 0; i < m_DILENGTH.Length / 4; i++)
									Memory::Write_U32(0x01010101, m_DIMAR.Address + i * 4);
								break;
							case 0x84000020:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD STATUS (1) (84000020)");
								for (unsigned int i = 0; i < m_DILENGTH.Length / 4; i++)
									Memory::Write_U32(0x00000000, m_DIMAR.Address + i * 4);
								break;
							default:
								ERROR_LOG(DVDINTERFACE, "GC-AM: UNKNOWN MEDIA BOARD LOCATION %x", iDVDOffset);
								break;
							}
							break;
						}
						else if ((iDVDOffset == 0x1f900000) || (iDVDOffset == 0x1f900020))
						{
							ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD COMM AREA (1f900020)");
							memcpy(Memory::GetPointer(m_DIMAR.Address), media_buffer + iDVDOffset - 0x1f900000, m_DILENGTH.Length);
							unsigned int i;
							for (i = 0; i < m_DILENGTH.Length; i += 4)
								ERROR_LOG(DVDINTERFACE, "GC-AM: %08x", Memory::Read_U32(m_DIMAR.Address + i));
							break;
						}
					}

					// Here is the actual Disk Reading
					if (!DVDRead(iDVDOffset, m_DIMAR.Address, m_DILENGTH.Length))
					{
						PanicAlert("Cant read from DVD_Plugin - DVD-Interface: Fatal Error");
					}
				}
				break;

			case 0x40: // Read DiscID
				_dbg_assert_(DVDINTERFACE, m_DICMDBUF[1].Hex == 0);
				_dbg_assert_(DVDINTERFACE, m_DICMDBUF[1].Hex == m_DILENGTH.Length);
				_dbg_assert_(DVDINTERFACE, m_DILENGTH.Length == 0x20);
				if (!DVDRead(m_DICMDBUF[1].Hex, m_DIMAR.Address, m_DILENGTH.Length))
					PanicAlert("Cant read from DVD_Plugin - DVD-Interface: Fatal Error");
				break;

			default:
				_dbg_assert_msg_(DVDINTERFACE, 0, "Unknown Read Subcommand");
				break;
			}
		}
		else
		{
			// there is no disc to read
			_DICR.TSTART = 0;
			m_DILENGTH.Length = 0;
			g_ErrorCode = ERROR_NO_DISK | ERROR_COVER_H;
			GenerateDVDInterrupt_Threadsafe(INT_DEINT);
			return;
		}
		break;

	// GC-AM
	case 0xAA:
		if (GCAM)
		{
			ERROR_LOG(DVDINTERFACE, "GC-AM: 0xAA, DMABuffer=%08x, DMALength=%08x", m_DIMAR.Address, m_DILENGTH.Length);
			u32 iDVDOffset = m_DICMDBUF[1].Hex << 2;
			unsigned int len = m_DILENGTH.Length; 
			int offset = iDVDOffset - 0x1F900000;
			/*
			if (iDVDOffset == 0x84800000)
			{
				ERROR_LOG(DVDINTERFACE, "firmware upload");
			}
			else*/
			if ((offset < 0) || ((offset + len) > 0x40) || len > 0x40)
			{
				unsigned long addr = m_DIMAR.Address;
				if (iDVDOffset == 0x84800000) {
					ERROR_LOG(DVDINTERFACE, "FIRMWARE UPLOAD");
				} else {
					ERROR_LOG(DVDINTERFACE, "ILLEGAL MEDIA WRITE");
				}
				while (len >= 4)
				{
					ERROR_LOG(DVDINTERFACE, "GC-AM Media Board WRITE (0xAA): %08x: %08x", iDVDOffset, Memory::Read_U32(addr));
					addr += 4;
					len -= 4;
					iDVDOffset += 4;
				}
			}
			else
			{
				unsigned long addr = m_DIMAR.Address;
				memcpy(media_buffer + offset, Memory::GetPointer(addr), len);
				while (len >= 4)
				{
					ERROR_LOG(DVDINTERFACE, "GC-AM Media Board WRITE (0xAA): %08x: %08x", iDVDOffset, Memory::Read_U32(addr));
					addr += 4;
					len -= 4;
					iDVDOffset += 4;
				}
			}
		}
		break;

	// Seek (immediate)
	case DVDLowSeek:
		if (!GCAM)
		{
			// We don't care :)
			DEBUG_LOG(DVDINTERFACE, "Seek: offset=%08x (ignoring)", m_DICMDBUF[1].Hex << 2);
		}
		else
		{
			memset(media_buffer, 0, 0x20);
			media_buffer[0] = media_buffer[0x20]; // ID
			media_buffer[2] = media_buffer[0x22];
			media_buffer[3] = media_buffer[0x23] | 0x80;
			int cmd = (media_buffer[0x23]<<8)|media_buffer[0x22];
			ERROR_LOG(DVDINTERFACE, "GC-AM: execute buffer, cmd=%04x", cmd);
			switch (cmd)
			{
			case 0x00:
				media_buffer[4] = 1;
				break;
			case 0x1:
				media_buffer[7] = 0x20; // DIMM Size
				break;
			case 0x100:
				{
					static int percentage;
					static int status = 0;
					percentage++;
					if (percentage > 100)
					{
						status++;
						percentage = 0;
					}
					media_buffer[4] = status;
					/* status:
					0 - "Initializing media board. Please wait.."
					1 - "Checking network. Please wait..."
					2 - "Found a system disc. Insert a game disc"
					3 - "Testing a game program. %d%%"
					4 - "Loading a game program. %d%%"
					5  - go
					6  - error xx 
					*/
					media_buffer[8] = percentage;
					media_buffer[4] = 0x05;
					media_buffer[8] = 0x64;
					break;
				}
			case 0x101:
				media_buffer[4] = 3; // version
				media_buffer[5] = 3; 
				media_buffer[6] = 1; // xxx
				media_buffer[8] = 1;
				media_buffer[16] = 0xFF;
				media_buffer[17] = 0xFF;
				media_buffer[18] = 0xFF;
				media_buffer[19] = 0xFF;
				break;
			case 0x102:  // get error code
				media_buffer[4] = 1; // 0: download incomplete (31), 1: corrupted, other error 1
				media_buffer[5] = 0;
				break;
			case 0x103:
				memcpy(media_buffer + 4, "A89E27A50364511", 15);  // serial
				break;
#if 0
			case 0x301: // unknown
				memcpy(media_buffer + 4, media_buffer + 0x24, 0x1c);
				break;
			case 0x302:
				break;
#endif
			default:
				ERROR_LOG(DVDINTERFACE, "GC-AM: execute buffer (unknown)");
				break;
			}
			memset(media_buffer + 0x20, 0, 0x20);
			m_DIIMMBUF.Hex = 0x66556677; // just a random value that works.
		}
		break;

	case DVDLowOffset:
		DEBUG_LOG(DVDINTERFACE, "DVDLowOffset: ignoring...");
		break;

	// Request Error Code
	case DVDLowRequestError:
		ERROR_LOG(DVDINTERFACE, "Requesting error... (0x%08x)", g_ErrorCode);
		m_DIIMMBUF.Hex = g_ErrorCode;
		break;

	// Audio Stream (Immediate)
	//	m_DICMDBUF[0].CMDBYTE1		= subcommand
	//	m_DICMDBUF[1].Hex << 2		= offset on disc 
	//	m_DICMDBUF[2].Hex			= Length of the stream
	case 0xE1:
		{
			// ugly hack to catch the disable command
			if (m_DICMDBUF[1].Hex != 0)
			{
				AudioPos	= m_DICMDBUF[1].Hex << 2;
				AudioStart	= AudioPos;
				AudioLength	= m_DICMDBUF[2].Hex;
				NGCADPCM::InitFilter();			

				g_bStream = true;

				WARN_LOG(DVDINTERFACE, "(Audio) Stream subcmd = %02x offset = %08x length=%08x",
					m_DICMDBUF[0].CMDBYTE1, AudioPos, AudioLength);
			}
			else
				WARN_LOG(DVDINTERFACE, "(Audio) Off?");
		}
		break;

	// Request Audio Status (Immediate)
	case 0xE2:
		m_DIIMMBUF.Hex = g_bStream ? 1 : 0;
		WARN_LOG(DVDINTERFACE, "(Audio): Request Audio status %s", g_bStream? "on":"off");
		break;

	case DVDLowStopMotor:
		DEBUG_LOG(DVDINTERFACE, "Stop motor");
		break;

	// DVD Audio Enable/Disable (Immediate)
	case DVDLowAudioBufferConfig:
		if (m_DICMDBUF[0].CMDBYTE1 == 1)
		{
			g_bStream = true;
			WARN_LOG(DVDINTERFACE, "(Audio): Audio enabled");
		}
		else
		{
			g_bStream = false;
			WARN_LOG(DVDINTERFACE, "(Audio): Audio disabled");
		}
		break;

	// yet another command we prolly don't care about
	case 0xEE:
		DEBUG_LOG(DVDINTERFACE, "SetStatus - Unimplemented");
		break;

	// Debug commands; see yagcd. We don't really care
	// NOTE: commands to stream data will send...a raw data stream
	// This will appear as unknown commands, unless the check is re-instated to catch such data.
	case 0xFE:
		INFO_LOG(DVDINTERFACE, "Unsupported DVD Drive debug command 0x%08x", m_DICMDBUF[0].Hex);
		break;

	// Unlock Commands. 1: "MATSHITA" 2: "DVD-GAME"
	// Just for fun
	case 0xFF:
		{
			if (m_DICMDBUF[0].Hex == 0xFF014D41
				&& m_DICMDBUF[1].Hex == 0x54534849
				&& m_DICMDBUF[2].Hex == 0x54410200)
			{
				INFO_LOG(DVDINTERFACE, "Unlock test 1 passed");
			}
			else if (m_DICMDBUF[0].Hex == 0xFF004456
				&& m_DICMDBUF[1].Hex == 0x442D4741
				&& m_DICMDBUF[2].Hex == 0x4D450300)
			{
				INFO_LOG(DVDINTERFACE, "Unlock test 2 passed");
			}
			else
			{
				INFO_LOG(DVDINTERFACE, "Unlock test failed");
			}
		}
		break;

	default:
		PanicAlert("Unknown DVD command %08x - fatal error", m_DICMDBUF[0].Hex);
		_dbg_assert_(DVDINTERFACE, 0);
		break;
	}

	// transfer is done
	_DICR.TSTART = 0;
	m_DILENGTH.Length = 0;
	GenerateDVDInterrupt_Threadsafe(INT_TCINT);
	g_ErrorCode = 0;
}

}  // namespace
