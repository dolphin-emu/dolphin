// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <cmath>

#include "AudioCommon/AudioCommon.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/VolumeHandler.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/StreamADPCM.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/PowerPC.h"

static const double PI = 3.14159265358979323846264338328;

// Rate the drive can transfer data to main memory, given the data
// is already buffered. Measured in bytes per second.
static const u32 BUFFER_TRANSFER_RATE = 1024 * 1024 * 16;

// Disc access time measured in milliseconds
static const u32 DISC_ACCESS_TIME_MS = 50;

// The size of a Wii disc layer in bytes (is this correct?)
static const u64 WII_DISC_LAYER_SIZE = 4699979776;

// By knowing the disc read speed at two locations defined here,
// the program can calulate the speed at arbitrary locations.
// Offsets are in bytes, and speeds are in bytes per second.
//
// These speeds are approximate. Using exact speeds is not possible
// because of how much variation there is between different hardware.

static const u32 GC_DISC_LOCATION_1_OFFSET = 0;             // The beginning of a GC disc
static const u32 GC_DISC_LOCATION_1_READ_SPEED = 1024 * 1024 * 2;
static const u32 GC_DISC_LOCATION_2_OFFSET = 1459978239;    // The end of a GC disc
static const u32 GC_DISC_LOCATION_2_READ_SPEED = (u32)(1024 * 1024 * 3.3);

static const u32 WII_DISC_LOCATION_1_OFFSET = 0;                    // The beginning of a Wii disc
static const u32 WII_DISC_LOCATION_1_READ_SPEED = (u32)(1024 * 1024 * 3.5);
static const u64 WII_DISC_LOCATION_2_OFFSET = WII_DISC_LAYER_SIZE;  // The end of a Wii disc
static const u32 WII_DISC_LOCATION_2_READ_SPEED = 1024 * 1024 * 9;

// These values are used for disc read speed calculations. Calculations
// are done using an arbitrary length unit where the radius of a disc track
// is the same as the read speed at that track in bytes per second.

static const double GC_DISC_AREA_UP_TO_LOCATION_1 =
	PI * GC_DISC_LOCATION_1_READ_SPEED * GC_DISC_LOCATION_1_READ_SPEED;
static const double GC_DISC_AREA_UP_TO_LOCATION_2 =
	PI * GC_DISC_LOCATION_2_READ_SPEED * GC_DISC_LOCATION_2_READ_SPEED;
static const double GC_BYTES_PER_AREA_UNIT =
	(GC_DISC_LOCATION_2_OFFSET - GC_DISC_LOCATION_1_OFFSET) /
	(GC_DISC_AREA_UP_TO_LOCATION_2 - GC_DISC_AREA_UP_TO_LOCATION_1);

static const double WII_DISC_AREA_UP_TO_LOCATION_1 =
	PI * WII_DISC_LOCATION_1_READ_SPEED * WII_DISC_LOCATION_1_READ_SPEED;
static const double WII_DISC_AREA_UP_TO_LOCATION_2 =
	PI * WII_DISC_LOCATION_2_READ_SPEED * WII_DISC_LOCATION_2_READ_SPEED;
static const double WII_BYTES_PER_AREA_UNIT =
	(WII_DISC_LOCATION_2_OFFSET - WII_DISC_LOCATION_1_OFFSET) /
	(WII_DISC_AREA_UP_TO_LOCATION_2 - WII_DISC_AREA_UP_TO_LOCATION_1);

namespace DVDInterface
{

// internal hardware addresses
enum
{
	DI_STATUS_REGISTER       = 0x00,
	DI_COVER_REGISTER        = 0x04,
	DI_COMMAND_0             = 0x08,
	DI_COMMAND_1             = 0x0C,
	DI_COMMAND_2             = 0x10,
	DI_DMA_ADDRESS_REGISTER  = 0x14,
	DI_DMA_LENGTH_REGISTER   = 0x18,
	DI_DMA_CONTROL_REGISTER  = 0x1C,
	DI_IMMEDIATE_DATA_BUFFER = 0x20,
	DI_CONFIG_REGISTER       = 0x24
};


// DVD IntteruptTypes
enum DI_InterruptType
{
	INT_DEINT  = 0,
	INT_TCINT  = 1,
	INT_BRKINT = 2,
	INT_CVRINT = 3,
};

// debug commands which may be ORd
enum
{
	STOP_DRIVE  = 0,
	START_DRIVE = 0x100,
	ACCEPT_COPY = 0x4000,
	DISC_CHECK  = 0x8000,
};

// DI Status Register
union UDISR
{
	u32 Hex;
	struct
	{
		u32 BREAK      :  1; // Stop the Device + Interrupt
		u32 DEINITMASK :  1; // Access Device Error Int Mask
		u32 DEINT      :  1; // Access Device Error Int
		u32 TCINTMASK  :  1; // Transfer Complete Int Mask
		u32 TCINT      :  1; // Transfer Complete Int
		u32 BRKINTMASK :  1;
		u32 BRKINT     :  1; // w 1: clear brkint
		u32            : 25;
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
		u32 CVR        :  1; // 0: Cover closed  1: Cover open
		u32 CVRINTMASK :  1; // 1: Interrupt enabled
		u32 CVRINT     :  1; // r 1: Interrupt requested w 1: Interrupt clear
		u32            : 29;
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
		u32 Zerobits : 5; // Must be zero (32byte aligned)
		u32          : 27;
	};
	struct
	{
		u32 Address : 26;
		u32         : 6;
	};
};

// DI DMA Address Length Register
union UDILENGTH
{
	u32 Hex;
	struct
	{
		u32 Zerobits : 5; // Must be zero (32byte aligned)
		u32          : 27;
	};
	struct
	{
		u32 Length : 26;
		u32        : 6;
	};
};

// DI DMA Control Register
union UDICR
{
	u32 Hex;
	struct
	{
		u32 TSTART : 1; // w:1 start   r:0 ready
		u32 DMA    : 1; // 1: DMA Mode    0: Immediate Mode (can only do Access Register Command)
		u32 RW     : 1; // 0: Read Command (DVD to Memory)  1: Write Command (Memory to DVD)
		u32        : 29;
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
		u32 CONFIG : 8;
		u32        : 24;
	};
	UDICFG() {Hex = 0;}
	UDICFG(u32 _hex) {Hex = _hex;}
};


// STATE_TO_SAVE
// hardware registers
static UDISR     m_DISR;
static UDICVR    m_DICVR;
static UDICMDBUF m_DICMDBUF[3];
static UDIMAR    m_DIMAR;
static UDILENGTH m_DILENGTH;
static UDICR     m_DICR;
static UDIIMMBUF m_DIIMMBUF;
static UDICFG    m_DICFG;

static u32 AudioPos;
static u32 CurrentStart;
static u32 CurrentLength;
static u32 NextStart;
static u32 NextLength;


static u32  g_ErrorCode = 0;
static bool g_bDiscInside = false;
bool g_bStream = false;
static bool g_bStopAtTrackEnd = false;
static int  tc = 0;
static int  dtk = 0;

static u64 g_last_read_offset;
static u64 g_last_read_time;

// GC-AM only
static unsigned char media_buffer[0x40];

static int ejectDisc;
static int insertDisc;

void EjectDiscCallback(u64 userdata, int cyclesLate);
void InsertDiscCallback(u64 userdata, int cyclesLate);

void UpdateInterrupts();
void GenerateDIInterrupt(DI_InterruptType _DVDInterrupt);
void ExecuteCommand();
void FinishExecuteRead();
u64 SimulateDiscReadTime();
s64 CalculateRawDiscReadTime(u64 offset, s64 length);

void DoState(PointerWrap &p)
{
	p.DoPOD(m_DISR);
	p.DoPOD(m_DICVR);
	p.DoArray(m_DICMDBUF, 3);
	p.Do(m_DIMAR);
	p.Do(m_DILENGTH);
	p.Do(m_DICR);
	p.Do(m_DIIMMBUF);
	p.DoPOD(m_DICFG);

	p.Do(NextStart);
	p.Do(AudioPos);
	p.Do(NextLength);

	p.Do(g_ErrorCode);
	p.Do(g_bDiscInside);
	p.Do(g_bStream);

	p.Do(CurrentStart);
	p.Do(CurrentLength);

	p.Do(g_last_read_offset);
	p.Do(g_last_read_time);

	p.Do(g_bStopAtTrackEnd);
}

static void TransferComplete(u64 userdata, int cyclesLate)
{
	if (m_DICR.TSTART)
	{
		m_DICR.TSTART = 0;
		m_DILENGTH.Length = 0;
		GenerateDIInterrupt(INT_TCINT);
		g_ErrorCode = 0;
	}
}

static u32 ProcessDTKSamples(short *tempPCM, u32 num_samples)
{
	u32 samples_processed = 0;
	do
	{
		if (AudioPos >= CurrentStart + CurrentLength)
		{
			DEBUG_LOG(DVDINTERFACE,
				"ProcessDTKSamples: NextStart=%08x,NextLength=%08x,CurrentStart=%08x,CurrentLength=%08x,AudioPos=%08x",
				NextStart, NextLength, CurrentStart, CurrentLength, AudioPos);

			AudioPos = NextStart;
			CurrentStart = NextStart;
			CurrentLength = NextLength;

			if (g_bStopAtTrackEnd)
			{
				g_bStopAtTrackEnd = false;
				g_bStream = false;
				break;
			}

			NGCADPCM::InitFilter();
		}

		u8 tempADPCM[NGCADPCM::ONE_BLOCK_SIZE];
		// TODO: What if we can't read from AudioPos?
		VolumeHandler::ReadToPtr(tempADPCM, AudioPos, sizeof(tempADPCM));
		AudioPos += sizeof(tempADPCM);
		NGCADPCM::DecodeBlock(tempPCM + samples_processed * 2, tempADPCM);
		samples_processed += NGCADPCM::SAMPLES_PER_BLOCK;
	} while (samples_processed < num_samples);
	for (unsigned i = 0; i < samples_processed * 2; ++i)
	{
		// TODO: Fix the mixer so it can accept non-byte-swapped samples.
		tempPCM[i] = Common::swap16(tempPCM[i]);
	}
	return samples_processed;
}

static void DTKStreamingCallback(u64 userdata, int cyclesLate)
{
	// Send audio to the mixer.
	static const int NUM_SAMPLES = 48000 / 2000 * 7;  // 3.5ms of 48kHz samples
	short tempPCM[NUM_SAMPLES * 2];
	unsigned samples_processed;
	if (g_bStream && AudioInterface::IsPlaying())
	{
		samples_processed = ProcessDTKSamples(tempPCM, NUM_SAMPLES);
	}
	else
	{
		memset(tempPCM, 0, sizeof(tempPCM));
		samples_processed = NUM_SAMPLES;
	}
	g_sound_stream->GetMixer()->PushStreamingSamples(tempPCM, samples_processed);

	int ticks_to_dtk = int(SystemTimers::GetTicksPerSecond() * u64(samples_processed) / 48000);
	CoreTiming::ScheduleEvent(ticks_to_dtk - cyclesLate, dtk);
}

void Init()
{
	m_DISR.Hex        = 0;
	m_DICVR.Hex       = 0;
	m_DICMDBUF[0].Hex = 0;
	m_DICMDBUF[1].Hex = 0;
	m_DICMDBUF[2].Hex = 0;
	m_DIMAR.Hex       = 0;
	m_DILENGTH.Hex    = 0;
	m_DICR.Hex        = 0;
	m_DIIMMBUF.Hex    = 0;
	m_DICFG.Hex       = 0;
	m_DICFG.CONFIG    = 1; // Disable bootrom descrambler

	AudioPos = 0;
	NextStart = 0;
	NextLength = 0;
	CurrentStart = 0;
	CurrentLength = 0;

	g_ErrorCode = 0;
	g_bDiscInside = false;
	g_bStream = false;
	g_bStopAtTrackEnd = false;

	g_last_read_offset = 0;
	g_last_read_time = 0;

	ejectDisc = CoreTiming::RegisterEvent("EjectDisc", EjectDiscCallback);
	insertDisc = CoreTiming::RegisterEvent("InsertDisc", InsertDiscCallback);

	tc = CoreTiming::RegisterEvent("TransferComplete", TransferComplete);
	dtk = CoreTiming::RegisterEvent("StreamingTimer", DTKStreamingCallback);

	CoreTiming::ScheduleEvent(0, dtk);
}

void Shutdown()
{
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
void EjectDiscCallback(u64 userdata, int cyclesLate)
{
	// Empty the drive
	SetDiscInside(false);
	SetLidOpen();
	VolumeHandler::EjectVolume();
}

void InsertDiscCallback(u64 userdata, int cyclesLate)
{
	std::string& SavedFileName = SConfig::GetInstance().m_LocalCoreStartupParameter.m_strFilename;
	std::string *_FileName = (std::string *)userdata;

	if (!VolumeHandler::SetVolumeName(*_FileName))
	{
		// Put back the old one
		VolumeHandler::SetVolumeName(SavedFileName);
		PanicAlertT("Invalid file");
	}
	SetLidOpen(false);
	SetDiscInside(VolumeHandler::IsValid());
	delete _FileName;
}

void ChangeDisc(const std::string& newFileName)
{
	std::string* _FileName = new std::string(newFileName);
	CoreTiming::ScheduleEvent_Threadsafe(0, ejectDisc);
	CoreTiming::ScheduleEvent_Threadsafe(500000000, insertDisc, (u64)_FileName);
	if (Movie::IsRecordingInput())
	{
		Movie::g_bDiscChange = true;
		std::string fileName = newFileName;
		auto sizeofpath = fileName.find_last_of("/\\") + 1;
		if (fileName.substr(sizeofpath).length() > 40)
		{
			PanicAlert("Saving iso filename to .dtm failed; max file name length is 40 characters.");
		}
		Movie::g_discChange = fileName.substr(sizeofpath);
	}
}

void SetLidOpen(bool _bOpen)
{
	m_DICVR.CVR = _bOpen ? 1 : 0;

	GenerateDIInterrupt(INT_CVRINT);
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
	return VolumeHandler::ReadToPtr(Memory::GetPointer(_iRamAddress), _iDVDOffset, _iLength);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
	mmio->Register(base | DI_STATUS_REGISTER,
		MMIO::DirectRead<u32>(&m_DISR.Hex),
		MMIO::ComplexWrite<u32>([](u32, u32 val) {
			UDISR tmpStatusReg(val);

			m_DISR.DEINITMASK = tmpStatusReg.DEINITMASK;
			m_DISR.TCINTMASK  = tmpStatusReg.TCINTMASK;
			m_DISR.BRKINTMASK = tmpStatusReg.BRKINTMASK;
			m_DISR.BREAK      = tmpStatusReg.BREAK;

			if (tmpStatusReg.DEINT)
				m_DISR.DEINT = 0;

			if (tmpStatusReg.TCINT)
				m_DISR.TCINT = 0;

			if (tmpStatusReg.BRKINT)
				m_DISR.BRKINT = 0;

			if (m_DISR.BREAK)
			{
				_dbg_assert_(DVDINTERFACE, 0);
			}

			UpdateInterrupts();
		})
	);

	mmio->Register(base | DI_COVER_REGISTER,
		MMIO::DirectRead<u32>(&m_DICVR.Hex),
		MMIO::ComplexWrite<u32>([](u32, u32 val) {
			UDICVR tmpCoverReg(val);

			m_DICVR.CVRINTMASK = tmpCoverReg.CVRINTMASK;

			if (tmpCoverReg.CVRINT)
				m_DICVR.CVRINT = 0;

			UpdateInterrupts();
		})
	);

	// Command registers are very similar and we can register them with a
	// simple loop.
	for (int i = 0; i < 3; ++i)
		mmio->Register(base | (DI_COMMAND_0 + 4 * i),
			MMIO::DirectRead<u32>(&m_DICMDBUF[i].Hex),
			MMIO::DirectWrite<u32>(&m_DICMDBUF[i].Hex)
		);

	// DMA related registers. Mostly direct accesses (+ masking for writes to
	// handle things like address alignment) and complex write on the DMA
	// control register that will trigger the DMA.
	mmio->Register(base | DI_DMA_ADDRESS_REGISTER,
		MMIO::DirectRead<u32>(&m_DIMAR.Hex),
		MMIO::DirectWrite<u32>(&m_DIMAR.Hex, ~0xFC00001F)
	);
	mmio->Register(base | DI_DMA_LENGTH_REGISTER,
		MMIO::DirectRead<u32>(&m_DILENGTH.Hex),
		MMIO::DirectWrite<u32>(&m_DILENGTH.Hex, ~0x1F)
	);
	mmio->Register(base | DI_DMA_CONTROL_REGISTER,
		MMIO::DirectRead<u32>(&m_DICR.Hex),
		MMIO::ComplexWrite<u32>([](u32, u32 val) {
			m_DICR.Hex = val & 7;
			if (m_DICR.TSTART)
			{
				ExecuteCommand();
			}
		})
	);

	mmio->Register(base | DI_IMMEDIATE_DATA_BUFFER,
		MMIO::DirectRead<u32>(&m_DIIMMBUF.Hex),
		MMIO::DirectWrite<u32>(&m_DIIMMBUF.Hex)
	);

	// DI config register is read only.
	mmio->Register(base | DI_CONFIG_REGISTER,
		MMIO::DirectRead<u32>(&m_DICFG.Hex),
		MMIO::InvalidWrite<u32>()
	);
}

void UpdateInterrupts()
{
	if ((m_DISR.DEINT   & m_DISR.DEINITMASK) ||
	    (m_DISR.TCINT   & m_DISR.TCINTMASK)  ||
	    (m_DISR.BRKINT  & m_DISR.BRKINTMASK) ||
	    (m_DICVR.CVRINT & m_DICVR.CVRINTMASK))
	{
		ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DI, true);
	}
	else
	{
		ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DI, false);
	}

	// Required for Summoner: A Goddess Reborn
	CoreTiming::ForceExceptionCheck(50);
}

void GenerateDIInterrupt(DI_InterruptType _DVDInterrupt)
{
	switch (_DVDInterrupt)
	{
	case INT_DEINT:  m_DISR.DEINT   = 1; break;
	case INT_TCINT:  m_DISR.TCINT   = 1; break;
	case INT_BRKINT: m_DISR.BRKINT  = 1; break;
	case INT_CVRINT: m_DICVR.CVRINT = 1; break;
	}

	UpdateInterrupts();
}

void ExecuteCommand()
{
	// This variable is used to simulate the time is takes to execute a command.
	// 1 / 15000 seconds is just some arbitrary default value.
	// Commands that implement more precise timing are supposed to overwrite this.
	u64 ticks_until_TC = SystemTimers::GetTicksPerSecond() / 15000;

	// _dbg_assert_(DVDINTERFACE, _DICR.RW == 0); // only DVD to Memory
	int GCAM = ((SConfig::GetInstance().m_SIDevice[0] == SIDEVICE_AM_BASEBOARD) &&
	            (SConfig::GetInstance().m_EXIDevice[2] == EXIDEVICE_AM_BASEBOARD))
	           ? 1 : 0;

	if (GCAM)
	{
		ERROR_LOG(DVDINTERFACE,
			"DVD: %08x, %08x, %08x, DMA=addr:%08x,len:%08x,ctrl:%08x",
			m_DICMDBUF[0].Hex, m_DICMDBUF[1].Hex, m_DICMDBUF[2].Hex,
			m_DIMAR.Hex, m_DILENGTH.Hex, m_DICR.Hex);
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

					DEBUG_LOG(DVDINTERFACE, "Read: DVDOffset=%08x, DMABuffer=%08x, SrcLength=%08x, DMALength=%08x",
						iDVDOffset, m_DIMAR.Address, m_DICMDBUF[2].Hex, m_DILENGTH.Length);
					_dbg_assert_(DVDINTERFACE, m_DICMDBUF[2].Hex == m_DILENGTH.Length);

					if (GCAM)
					{
						if (iDVDOffset & 0x80000000) // read request to hardware buffer
						{
							u32 len = m_DILENGTH.Length / 4;
							switch (iDVDOffset)
							{
							case 0x80000000:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD STATUS (80000000)");
								for (u32 i = 0; i < len; i++)
									Memory::Write_U32(0, m_DIMAR.Address + i * 4);
								break;
							case 0x80000040:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD STATUS (2) (80000040)");
								for (u32 i = 0; i < len; i++)
									Memory::Write_U32(~0, m_DIMAR.Address + i * 4);
								Memory::Write_U32(0x00000020, m_DIMAR.Address); // DIMM SIZE, LE
								Memory::Write_U32(0x4743414D, m_DIMAR.Address + 4); // GCAM signature
								break;
							case 0x80000120:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ FIRMWARE STATUS (80000120)");
								for (u32 i = 0; i < len; i++)
									Memory::Write_U32(0x01010101, m_DIMAR.Address + i * 4);
								break;
							case 0x80000140:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ FIRMWARE STATUS (80000140)");
								for (u32 i = 0; i < len; i++)
									Memory::Write_U32(0x01010101, m_DIMAR.Address + i * 4);
								break;
							case 0x84000020:
								ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD STATUS (1) (84000020)");
								for (u32 i = 0; i < len; i++)
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
							u8* source = media_buffer + iDVDOffset - 0x1f900000;
							Memory::CopyToEmu(m_DIMAR.Address, source, m_DILENGTH.Length);
							for (u32 i = 0; i < m_DILENGTH.Length; i += 4)
								ERROR_LOG(DVDINTERFACE, "GC-AM: %08x", Memory::Read_U32(m_DIMAR.Address + i));
							break;
						}
					}

					ticks_until_TC = SimulateDiscReadTime();

					// Here is the actual disc reading
					if (!DVDRead(iDVDOffset, m_DIMAR.Address, m_DILENGTH.Length))
					{
						PanicAlertT("Can't read from DVD_Plugin - DVD-Interface: Fatal Error");
					}
				}
				break;

			case 0x40: // Read DiscID
				_dbg_assert_(DVDINTERFACE, m_DICMDBUF[1].Hex == 0);
				_dbg_assert_(DVDINTERFACE, m_DICMDBUF[2].Hex == m_DILENGTH.Length);
				_dbg_assert_(DVDINTERFACE, m_DILENGTH.Length == 0x20);
				if (!DVDRead(m_DICMDBUF[1].Hex, m_DIMAR.Address, m_DILENGTH.Length))
					PanicAlertT("Can't read from DVD_Plugin - DVD-Interface: Fatal Error");
				WARN_LOG(DVDINTERFACE, "Read DiscID %08x", Memory::Read_U32(m_DIMAR.Address));
				break;

			default:
				_dbg_assert_msg_(DVDINTERFACE, 0, "Unknown Read Subcommand");
				break;
			}
		}
		else
		{
			// there is no disc to read
			m_DICR.TSTART = 0;
			m_DILENGTH.Length = 0;
			g_ErrorCode = ERROR_NO_DISK | ERROR_COVER_H;
			GenerateDIInterrupt(INT_DEINT);
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
				ERROR_LOG(DVDINTERFACE, "Firmware upload");
			}
			else*/
			if ((offset < 0) || ((offset + len) > 0x40) || len > 0x40)
			{
				u32 addr = m_DIMAR.Address;
				if (iDVDOffset == 0x84800000)
				{
					ERROR_LOG(DVDINTERFACE, "FIRMWARE UPLOAD");
				}
				else
				{
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
				u32 addr = m_DIMAR.Address;
				Memory::CopyFromEmu(media_buffer + offset, addr, len);
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
					// urgh
					static int percentage = 0;
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
	// m_DICMDBUF[0].CMDBYTE1 = Subcommand
	// m_DICMDBUF[1].Hex << 2 = Offset on disc
	// m_DICMDBUF[2].Hex      = Length of the stream
	case 0xE1:
		{
			u8 cancel_stream = m_DICMDBUF[0].CMDBYTE1;
			if (cancel_stream)
			{
				g_bStopAtTrackEnd = false;
				g_bStream = false;
				AudioPos = 0;
				NextStart = 0;
				NextLength = 0;
				CurrentStart = 0;
				CurrentLength = 0;
			}
			else
			{
				u32 pos = m_DICMDBUF[1].Hex << 2;
				u32 length = m_DICMDBUF[2].Hex;

				if ((pos == 0) && (length == 0))
				{
					g_bStopAtTrackEnd = true;
				}
				else if (!g_bStopAtTrackEnd)
				{
					NextStart = pos;
					NextLength = length;
					if (!g_bStream)
					{
						CurrentStart = NextStart;
						CurrentLength = NextLength;
						AudioPos = CurrentStart;
						NGCADPCM::InitFilter();
						g_bStream = true;
					}
				}
			}


			WARN_LOG(DVDINTERFACE, "(Audio) Stream subcmd = %08x offset = %08x length=%08x",
				m_DICMDBUF[0].Hex, m_DICMDBUF[1].Hex << 2, m_DICMDBUF[2].Hex);
		}
		break;

	// Request Audio Status (Immediate)
	case 0xE2:
		{
			switch (m_DICMDBUF[0].CMDBYTE1)
			{
			case 0x00: // Returns streaming status
				DEBUG_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:%08x/%08x CurrentStart:%08x CurrentLength:%08x", AudioPos, CurrentStart + CurrentLength, CurrentStart, CurrentLength);
				m_DIIMMBUF.REGVAL0 = 0;
				m_DIIMMBUF.REGVAL1 = 0;
				m_DIIMMBUF.REGVAL2 = 0;
				m_DIIMMBUF.REGVAL3 = (g_bStream) ? 1 : 0;
				break;
			case 0x01: // Returns the current offset
				DEBUG_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:%08x", AudioPos);
				m_DIIMMBUF.Hex = AudioPos >> 2;
				break;
			case 0x02: // Returns the start offset
				DEBUG_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentStart:%08x", CurrentStart);
				m_DIIMMBUF.Hex = CurrentStart >> 2;
				break;
			case 0x03: // Returns the total length
				DEBUG_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentLength:%08x", CurrentLength);
				m_DIIMMBUF.Hex = CurrentLength;
				break;
			default:
				WARN_LOG(DVDINTERFACE, "(Audio): Subcommand: %02x  Request Audio status %s", m_DICMDBUF[0].CMDBYTE1, g_bStream? "on":"off");
				break;
			}
		}
		break;

	case DVDLowStopMotor:
		DEBUG_LOG(DVDINTERFACE, "Stop motor");
		break;

	// DVD Audio Enable/Disable (Immediate)
	case DVDLowAudioBufferConfig:
		if (m_DICMDBUF[0].CMDBYTE1 == 1)
		{
			// TODO: What is this actually supposed to do?
			g_bStream = true;
			WARN_LOG(DVDINTERFACE, "(Audio): Audio enabled");
		}
		else
		{
			// TODO: What is this actually supposed to do?
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
			if (m_DICMDBUF[0].Hex == 0xFF014D41 &&
			    m_DICMDBUF[1].Hex == 0x54534849 &&
			    m_DICMDBUF[2].Hex == 0x54410200)
			{
				INFO_LOG(DVDINTERFACE, "Unlock test 1 passed");
			}
			else if (m_DICMDBUF[0].Hex == 0xFF004456 &&
			         m_DICMDBUF[1].Hex == 0x442D4741 &&
			         m_DICMDBUF[2].Hex == 0x4D450300)
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
		PanicAlertT("Unknown DVD command %08x - fatal error", m_DICMDBUF[0].Hex);
		_dbg_assert_(DVDINTERFACE, 0);
		break;
	}

	// The transfer is finished after a delay
	CoreTiming::ScheduleEvent((int)ticks_until_TC, tc);
}

// Simulates the timing aspects of reading data from a disc.
// Sets g_last_read_offset and g_last_read_time, and returns ticks_until_TC.
u64 SimulateDiscReadTime()
{
	u64 DVD_offset = (u64)m_DICMDBUF[1].Hex << 2;
	u64 current_time = CoreTiming::GetTicks();
	u64 ticks_until_TC;

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bFastDiscSpeed)
	{
		// Make sure fast disc speed performs "instant" reads; in addition
		// to being used to speed up games, fast disc speed is used as a
		// workaround for crashes in certain games, including Star Wars
		// Rogue Leader.
		ticks_until_TC = 0;
		g_last_read_time = current_time;
	}
	else
	{
		// The drive buffers 1 MiB (?) of data after every read request;
		// if a read request is covered by this buffer (or if it's
		// faster to wait for the data to be buffered), the drive
		// doesn't seek; it returns buffered data.  Data can be
		// transferred from the buffer at up to 16 MiB/s.
		//
		// If the drive has to seek, the time this takes varies a lot.
		// A short seek is around 50 ms; a long seek is around 150 ms.
		// However, the time isn't purely dependent on the distance; the
		// pattern of previous seeks seems to matter in a way I'm
		// not sure how to explain.
		//
		// Metroid Prime is a good example of a game that's sensitive to
		// all of these details; if there isn't enough latency in the
		// right places, doors open too quickly, and if there's too
		// much latency in the wrong places, the video before the
		// save-file select screen lags.
		//
		// For now, just use a very rough approximation: 50 ms seek
		// for reads outside 1 MiB, accelerated reads within 1 MiB.
		// We can refine this if someone comes up with a more complete
		// model for seek times.

		// Number of ticks it takes to seek and read directly from the disk.
		u64 disk_read_duration = CalculateRawDiscReadTime(DVD_offset, m_DILENGTH.Length) +
			SystemTimers::GetTicksPerSecond() / 1000 * DISC_ACCESS_TIME_MS;

		if (DVD_offset + m_DILENGTH.Length - g_last_read_offset > 1024 * 1024)
		{
			// No buffer; just use the simple seek time + read time.
			DEBUG_LOG(DVDINTERFACE, "Seeking %" PRId64 " bytes",
				s64(g_last_read_offset) - s64(DVD_offset));
			ticks_until_TC = disk_read_duration;
			g_last_read_time = current_time + ticks_until_TC;
		}
		else
		{
			// Possibly buffered; use the buffer if it saves time.
			// It's not proven that the buffer actually behaves like this, but
			// it appears to be a decent approximation.

			// Time at which the buffer will contain the data we need.
			u64 buffer_fill_time = g_last_read_time +
				CalculateRawDiscReadTime(g_last_read_offset,
				DVD_offset + m_DILENGTH.Length - g_last_read_offset);
			// Number of ticks it takes to transfer the data from the buffer to memory.
			u64 buffer_read_duration = m_DILENGTH.Length *
				(SystemTimers::GetTicksPerSecond() / BUFFER_TRANSFER_RATE);

			if (current_time > buffer_fill_time)
			{
				DEBUG_LOG(DVDINTERFACE, "Fast buffer read at %" PRId64, s64(DVD_offset));
				ticks_until_TC = buffer_read_duration;
				g_last_read_time = buffer_fill_time;
			}
			else if (current_time + disk_read_duration > buffer_fill_time)
			{
				DEBUG_LOG(DVDINTERFACE, "Slow buffer read at %" PRId64, s64(DVD_offset));
				ticks_until_TC = std::max(buffer_fill_time - current_time,
				                          buffer_read_duration);
				g_last_read_time = buffer_fill_time;
			}
			else
			{
				DEBUG_LOG(DVDINTERFACE, "Short seek %" PRId64 " bytes",
					s64(g_last_read_offset) - s64(DVD_offset));
				ticks_until_TC = disk_read_duration;
				g_last_read_time = current_time + ticks_until_TC;
			}
		}
	}

	g_last_read_offset = (DVD_offset + m_DILENGTH.Length - 2048) & ~2047;

	return ticks_until_TC;
}

// Returns the number of ticks it takes to read an amount of
// data from a disc, ignoring factors such as seek times.
// The result will be negative if the length is negative.
s64 CalculateRawDiscReadTime(u64 offset, s64 length)
{
	// The speed will be calculated using the average offset. This is a bit
	// inaccurate since the speed doesn't increase linearly with the offset,
	// but since reads only span a small part of the disc, it's insignificant.
	u64 average_offset = offset + (length / 2);

	// Here, addresses on the second layer of Wii discs are replaced with equivalent
	// addresses on the first layer so that the speed calculation works correctly.
	// This is wrong for reads spanning two layers, but those should be rare.
	average_offset %= WII_DISC_LAYER_SIZE;

	// The area on the disc between position 1 and the arbitrary position X is:
	// LOCATION_X_SPEED * LOCATION_X_SPEED * pi - AREA_UP_TO_LOCATION_1
	//
	// The number of bytes between position 1 and position X is:
	// LOCATION_X_OFFSET - LOCATION_1_OFFSET
	//
	// This means that the following equation is true:
	// (LOCATION_X_SPEED * LOCATION_X_SPEED * pi - AREA_UP_TO_LOCATION_1) *
	// BYTES_PER_AREA_UNIT = LOCATION_X_OFFSET - LOCATION_1_OFFSET
	//
	// Solving this equation for LOCATION_X_SPEED results in this:
	// LOCATION_X_SPEED = sqrt(((LOCATION_X_OFFSET - LOCATION_1_OFFSET) /
	// BYTES_PER_AREA_UNIT + AREA_UP_TO_LOCATION_1) / pi)
	//
	// Note that the speed at a track (in bytes per second) is the same as
	// the radius of that track because of the length unit used.
	double speed;
	if (VolumeHandler::IsWii())
	{
		speed = std::sqrt(((average_offset - WII_DISC_LOCATION_1_OFFSET) /
			WII_BYTES_PER_AREA_UNIT + WII_DISC_AREA_UP_TO_LOCATION_1) / PI);
	}
	else
	{
		speed = std::sqrt(((average_offset - GC_DISC_LOCATION_1_OFFSET) /
			GC_BYTES_PER_AREA_UNIT + GC_DISC_AREA_UP_TO_LOCATION_1) / PI);
	}

	return (s64)(SystemTimers::GetTicksPerSecond() / speed * length);
}

}  // namespace
