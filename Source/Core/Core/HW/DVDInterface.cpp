// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <cmath>
#include <memory>
#include <string>

#include "AudioCommon/AudioCommon.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/DVDThread.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/StreamADPCM.h"
#include "Core/HW/SystemTimers.h"

#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

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
// These speeds are approximations of speeds measured on real Wiis.

static const u32 GC_DISC_LOCATION_1_OFFSET = 0;             // The beginning of a GC disc - 48 mm
static const u32 GC_DISC_LOCATION_1_READ_SPEED = (u32)(1024 * 1024 * 2.1);
static const u32 GC_DISC_LOCATION_2_OFFSET = 1459978239;    // The end of a GC disc - 76 mm
static const u32 GC_DISC_LOCATION_2_READ_SPEED = (u32)(1024 * 1024 * 3.325);

static const u32 WII_DISC_LOCATION_1_OFFSET = 0;                    // The beginning of a Wii disc - 48 mm
static const u32 WII_DISC_LOCATION_1_READ_SPEED = (u32)(1024 * 1024 * 3.5);
static const u64 WII_DISC_LOCATION_2_OFFSET = WII_DISC_LAYER_SIZE;  // The end of a Wii disc - 116 mm
static const u32 WII_DISC_LOCATION_2_READ_SPEED = (u32)(1024 * 1024 * 8.45);

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

static std::unique_ptr<DiscIO::IVolume> s_inserted_volume;

// STATE_TO_SAVE
// hardware registers
static UDISR     s_DISR;
static UDICVR    s_DICVR;
static UDICMDBUF s_DICMDBUF[3];
static UDIMAR    s_DIMAR;
static UDILENGTH s_DILENGTH;
static UDICR     s_DICR;
static UDIIMMBUF s_DIIMMBUF;
static UDICFG    s_DICFG;

static u32 s_audio_pos;
static u32 s_current_start;
static u32 s_current_length;
static u32 s_next_start;
static u32 s_next_length;


static u32  s_error_code = 0;
static bool s_disc_inside = false;
static bool s_stream = false;
static bool s_stop_at_track_end = false;
static int  s_finish_execute_command = 0;
static int  s_dtk = 0;

static u64 s_last_read_offset;
static u64 s_last_read_time;

// GC-AM only
static unsigned char s_media_buffer[0x40];

static int s_eject_disc;
static int s_insert_disc;

void EjectDiscCallback(u64 userdata, int cyclesLate);
void InsertDiscCallback(u64 userdata, int cyclesLate);

void SetLidOpen(bool _bOpen);

void UpdateInterrupts();
void GenerateDIInterrupt(DIInterruptType _DVDInterrupt);

void WriteImmediate(u32 value, u32 output_address, bool write_to_DIIMMBUF);
bool ExecuteReadCommand(u64 DVD_offset, u32 output_address, u32 DVD_length, u32 output_length, bool decrypt,
                        int callback_event_type, DIInterruptType* interrupt_type, u64* ticks_until_completion);

u64 SimulateDiscReadTime(u64 offset, u32 length);
s64 CalculateRawDiscReadTime(u64 offset, s64 length);

void DoState(PointerWrap &p)
{
	p.DoPOD(s_DISR);
	p.DoPOD(s_DICVR);
	p.DoArray(s_DICMDBUF);
	p.Do(s_DIMAR);
	p.Do(s_DILENGTH);
	p.Do(s_DICR);
	p.Do(s_DIIMMBUF);
	p.DoPOD(s_DICFG);

	p.Do(s_next_start);
	p.Do(s_audio_pos);
	p.Do(s_next_length);

	p.Do(s_error_code);
	p.Do(s_disc_inside);
	p.Do(s_stream);

	p.Do(s_current_start);
	p.Do(s_current_length);

	p.Do(s_last_read_offset);
	p.Do(s_last_read_time);

	p.Do(s_stop_at_track_end);

	DVDThread::DoState(p);
}

static void FinishExecuteCommand(u64 userdata, int cyclesLate)
{
	if (s_DICR.TSTART)
	{
		s_DICR.TSTART = 0;
		s_DILENGTH.Length = 0;
		GenerateDIInterrupt((DIInterruptType)userdata);
	}
}

static u32 ProcessDTKSamples(short *tempPCM, u32 num_samples)
{
	// TODO: Read audio data using the DVD thread instead of blocking on it?
	DVDThread::WaitUntilIdle();

	u32 samples_processed = 0;
	do
	{
		if (s_audio_pos >= s_current_start + s_current_length)
		{
			DEBUG_LOG(DVDINTERFACE,
			          "ProcessDTKSamples: NextStart=%08x,NextLength=%08x,CurrentStart=%08x,CurrentLength=%08x,AudioPos=%08x",
			          s_next_start, s_next_length, s_current_start, s_current_length, s_audio_pos);

			s_audio_pos = s_next_start;
			s_current_start = s_next_start;
			s_current_length = s_next_length;

			if (s_stop_at_track_end)
			{
				s_stop_at_track_end = false;
				s_stream = false;
				break;
			}

			StreamADPCM::InitFilter();
		}

		u8 tempADPCM[StreamADPCM::ONE_BLOCK_SIZE];
		// TODO: What if we can't read from s_audio_pos?
		s_inserted_volume->Read(s_audio_pos, sizeof(tempADPCM), tempADPCM, false);
		s_audio_pos += sizeof(tempADPCM);
		StreamADPCM::DecodeBlock(tempPCM + samples_processed * 2, tempADPCM);
		samples_processed += StreamADPCM::SAMPLES_PER_BLOCK;
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
	if (s_stream && AudioInterface::IsPlaying())
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
	CoreTiming::ScheduleEvent(ticks_to_dtk - cyclesLate, s_dtk);
}

void Init()
{
	DVDThread::Start();

	s_DISR.Hex        = 0;
	s_DICVR.Hex       = 1; // Disc Channel relies on cover being open when no disc is inserted
	s_DICMDBUF[0].Hex = 0;
	s_DICMDBUF[1].Hex = 0;
	s_DICMDBUF[2].Hex = 0;
	s_DIMAR.Hex       = 0;
	s_DILENGTH.Hex    = 0;
	s_DICR.Hex        = 0;
	s_DIIMMBUF.Hex    = 0;
	s_DICFG.Hex       = 0;
	s_DICFG.CONFIG    = 1; // Disable bootrom descrambler

	s_audio_pos = 0;
	s_next_start = 0;
	s_next_length = 0;
	s_current_start = 0;
	s_current_length = 0;

	s_error_code = 0;
	s_disc_inside = false;
	s_stream = false;
	s_stop_at_track_end = false;

	s_last_read_offset = 0;
	s_last_read_time = 0;

	s_eject_disc = CoreTiming::RegisterEvent("EjectDisc", EjectDiscCallback);
	s_insert_disc = CoreTiming::RegisterEvent("InsertDisc", InsertDiscCallback);

	s_finish_execute_command = CoreTiming::RegisterEvent("FinishExecuteCommand", FinishExecuteCommand);
	s_dtk = CoreTiming::RegisterEvent("StreamingTimer", DTKStreamingCallback);

	CoreTiming::ScheduleEvent(0, s_dtk);
}

void Shutdown()
{
	DVDThread::Stop();
	s_inserted_volume.reset();
}

const DiscIO::IVolume& GetVolume()
{
	return *s_inserted_volume;
}

bool SetVolumeName(const std::string& disc_path)
{
	DVDThread::WaitUntilIdle();
	s_inserted_volume = DiscIO::CreateVolumeFromFilename(disc_path);
	return VolumeIsValid();
}

bool SetVolumeDirectory(const std::string& full_path, bool is_wii, const std::string& apploader_path, const std::string& DOL_path)
{
	DVDThread::WaitUntilIdle();
	s_inserted_volume = DiscIO::CreateVolumeFromDirectory(full_path, is_wii, apploader_path, DOL_path);
	return VolumeIsValid();
}

bool VolumeIsValid()
{
	return s_inserted_volume != nullptr;
}

void SetDiscInside(bool disc_inside)
{
	if (s_disc_inside != disc_inside)
		SetLidOpen(!disc_inside);

	s_disc_inside = disc_inside;
}

bool IsDiscInside()
{
	return s_disc_inside;
}

// Take care of all logic of "swapping discs"
// We want this in the "backend", NOT the gui
// any !empty string will be deleted to ensure
// that the userdata string exists when called
void EjectDiscCallback(u64 userdata, int cyclesLate)
{
	DVDThread::WaitUntilIdle();
	s_inserted_volume.reset();
	SetDiscInside(false);
}

void InsertDiscCallback(u64 userdata, int cyclesLate)
{
	std::string& SavedFileName = SConfig::GetInstance().m_strFilename;
	std::string *_FileName = (std::string *)userdata;

	if (!SetVolumeName(*_FileName))
	{
		// Put back the old one
		SetVolumeName(SavedFileName);
		PanicAlertT("Invalid file");
	}
	SetDiscInside(VolumeIsValid());
	delete _FileName;
}

void ChangeDisc(const std::string& newFileName)
{
	bool is_cpu = Core::IsCPUThread();
	bool was_unpaused = is_cpu ? false : Core::PauseAndLock(true);
	std::string* _FileName = new std::string(newFileName);
	CoreTiming::ScheduleEvent(0, s_eject_disc);
	CoreTiming::ScheduleEvent(500000000, s_insert_disc, (u64)_FileName);
	if (Movie::IsRecordingInput())
	{
		Movie::g_bDiscChange = true;
		std::string fileName = newFileName;
		auto sizeofpath = fileName.find_last_of("/\\") + 1;
		if (fileName.substr(sizeofpath).length() > 40)
		{
			PanicAlertT("The disc change to \"%s\" could not be saved in the .dtm file.\n"
			            "The filename of the disc image must not be longer than 40 characters.", newFileName.c_str());
		}
		Movie::g_discChange = fileName.substr(sizeofpath);
	}
	if (!is_cpu)
		Core::PauseAndLock(false, was_unpaused);
}

void SetLidOpen(bool _bOpen)
{
	s_DICVR.CVR = _bOpen ? 1 : 0;

	GenerateDIInterrupt(INT_CVRINT);
}

bool ChangePartition(u64 offset)
{
	DVDThread::WaitUntilIdle();
	return s_inserted_volume->ChangePartition(offset);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
	mmio->Register(base | DI_STATUS_REGISTER,
		MMIO::DirectRead<u32>(&s_DISR.Hex),
		MMIO::ComplexWrite<u32>([](u32, u32 val) {
			UDISR tmpStatusReg(val);

			s_DISR.DEINITMASK = tmpStatusReg.DEINITMASK;
			s_DISR.TCINTMASK  = tmpStatusReg.TCINTMASK;
			s_DISR.BRKINTMASK = tmpStatusReg.BRKINTMASK;
			s_DISR.BREAK      = tmpStatusReg.BREAK;

			if (tmpStatusReg.DEINT)
				s_DISR.DEINT = 0;

			if (tmpStatusReg.TCINT)
				s_DISR.TCINT = 0;

			if (tmpStatusReg.BRKINT)
				s_DISR.BRKINT = 0;

			if (s_DISR.BREAK)
			{
				_dbg_assert_(DVDINTERFACE, 0);
			}

			UpdateInterrupts();
		})
	);

	mmio->Register(base | DI_COVER_REGISTER,
		MMIO::DirectRead<u32>(&s_DICVR.Hex),
		MMIO::ComplexWrite<u32>([](u32, u32 val) {
			UDICVR tmpCoverReg(val);

			s_DICVR.CVRINTMASK = tmpCoverReg.CVRINTMASK;

			if (tmpCoverReg.CVRINT)
				s_DICVR.CVRINT = 0;

			UpdateInterrupts();
		})
	);

	// Command registers are very similar and we can register them with a
	// simple loop.
	for (int i = 0; i < 3; ++i)
		mmio->Register(base | (DI_COMMAND_0 + 4 * i),
			MMIO::DirectRead<u32>(&s_DICMDBUF[i].Hex),
			MMIO::DirectWrite<u32>(&s_DICMDBUF[i].Hex)
		);

	// DMA related registers. Mostly direct accesses (+ masking for writes to
	// handle things like address alignment) and complex write on the DMA
	// control register that will trigger the DMA.
	mmio->Register(base | DI_DMA_ADDRESS_REGISTER,
		MMIO::DirectRead<u32>(&s_DIMAR.Hex),
		MMIO::DirectWrite<u32>(&s_DIMAR.Hex, ~0xFC00001F)
	);
	mmio->Register(base | DI_DMA_LENGTH_REGISTER,
		MMIO::DirectRead<u32>(&s_DILENGTH.Hex),
		MMIO::DirectWrite<u32>(&s_DILENGTH.Hex, ~0x1F)
	);
	mmio->Register(base | DI_DMA_CONTROL_REGISTER,
		MMIO::DirectRead<u32>(&s_DICR.Hex),
		MMIO::ComplexWrite<u32>([](u32, u32 val) {
			s_DICR.Hex = val & 7;
			if (s_DICR.TSTART)
			{
				ExecuteCommand(s_DICMDBUF[0].Hex, s_DICMDBUF[1].Hex, s_DICMDBUF[2].Hex,
				               s_DIMAR.Hex, s_DILENGTH.Hex, true, s_finish_execute_command);
			}
		})
	);

	mmio->Register(base | DI_IMMEDIATE_DATA_BUFFER,
		MMIO::DirectRead<u32>(&s_DIIMMBUF.Hex),
		MMIO::DirectWrite<u32>(&s_DIIMMBUF.Hex)
	);

	// DI config register is read only.
	mmio->Register(base | DI_CONFIG_REGISTER,
		MMIO::DirectRead<u32>(&s_DICFG.Hex),
		MMIO::InvalidWrite<u32>()
	);
}

void UpdateInterrupts()
{
	if ((s_DISR.DEINT   & s_DISR.DEINITMASK) ||
	    (s_DISR.TCINT   & s_DISR.TCINTMASK)  ||
	    (s_DISR.BRKINT  & s_DISR.BRKINTMASK) ||
	    (s_DICVR.CVRINT & s_DICVR.CVRINTMASK))
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

void GenerateDIInterrupt(DIInterruptType _DVDInterrupt)
{
	switch (_DVDInterrupt)
	{
	case INT_DEINT:  s_DISR.DEINT   = 1; break;
	case INT_TCINT:  s_DISR.TCINT   = 1; break;
	case INT_BRKINT: s_DISR.BRKINT  = 1; break;
	case INT_CVRINT: s_DICVR.CVRINT = 1; break;
	}

	UpdateInterrupts();
}

void WriteImmediate(u32 value, u32 output_address, bool write_to_DIIMMBUF)
{
	if (write_to_DIIMMBUF)
		s_DIIMMBUF.Hex = value;
	else
		Memory::Write_U32(value, output_address);
}

// Iff false is returned, ScheduleEvent must be used to finish executing the command
bool ExecuteReadCommand(u64 DVD_offset, u32 output_address, u32 DVD_length, u32 output_length, bool decrypt,
                        int callback_event_type, DIInterruptType* interrupt_type, u64* ticks_until_completion)
{
	if (!s_disc_inside)
	{
		// Disc read fails
		s_error_code = ERROR_NO_DISK | ERROR_COVER_H;
		*interrupt_type = INT_DEINT;
		return false;
	}
	else
	{
		// Disc read succeeds
		*interrupt_type = INT_TCINT;
	}

	if (DVD_length > output_length)
	{
		WARN_LOG(DVDINTERFACE, "Detected attempt to read more data from the DVD than fit inside the out buffer. Clamp.");
		DVD_length = output_length;
	}

	if (SConfig::GetInstance().bFastDiscSpeed)
		// An optional hack to speed up loading times
		*ticks_until_completion = output_length * (SystemTimers::GetTicksPerSecond() / BUFFER_TRANSFER_RATE);
	else
		*ticks_until_completion = SimulateDiscReadTime(DVD_offset, DVD_length);

	DVDThread::StartRead(DVD_offset, output_address, DVD_length, decrypt,
	                     callback_event_type, (int)*ticks_until_completion);
	return true;
}

// When the command has finished executing, callback_event_type
// will be called using CoreTiming::ScheduleEvent,
// with the userdata set to the interrupt type.
void ExecuteCommand(u32 command_0, u32 command_1, u32 command_2, u32 output_address, u32 output_length,
                    bool write_to_DIIMMBUF, int callback_event_type)
{
	DIInterruptType interrupt_type = INT_TCINT;
	u64 ticks_until_completion = SystemTimers::GetTicksPerSecond() / 15000;
	bool command_handled_by_thread = false;

	bool GCAM = (SConfig::GetInstance().m_SIDevice[0] == SIDEVICE_AM_BASEBOARD) &&
	            (SConfig::GetInstance().m_EXIDevice[2] == EXIDEVICE_AM_BASEBOARD);

	// DVDLowRequestError needs access to the error code set by the previous command
	if (command_0 >> 24 != DVDLowRequestError)
		s_error_code = 0;

	if (GCAM)
	{
		ERROR_LOG(DVDINTERFACE, "DVD: %08x, %08x, %08x, DMA=addr:%08x,len:%08x,ctrl:%08x",
		          command_0, command_1, command_2, output_address, output_length, s_DICR.Hex);
		// decrypt command. But we have a zero key, that simplifies things a lot.
		// If you get crazy dvd command errors, make sure 0x80000000 - 0x8000000c is zero'd
		command_0 <<= 24;
	}

	switch (command_0 >> 24)
	{
	// Seems to be used by both GC and Wii
	case DVDLowInquiry:
		if (GCAM)
		{
			// 0x29484100...
			// was 21 i'm not entirely sure about this, but it works well.
			WriteImmediate(0x21000000, output_address, write_to_DIIMMBUF);
		}
		else
		{
			// (shuffle2) Taken from my Wii
			Memory::Write_U32(0x00000002, output_address);
			Memory::Write_U32(0x20060526, output_address + 4);
			// This was in the oubuf even though this cmd is only supposed to reply with 64bits
			// However, this and other tests strongly suggest that the buffer is static, and it's never - or rarely cleared.
			Memory::Write_U32(0x41000000, output_address + 8);

			INFO_LOG(DVDINTERFACE, "DVDLowInquiry (Buffer 0x%08x, 0x%x)",
			         output_address, output_length);
		}
		break;

	// Only seems to be used from WII_IPC, not through direct access
	case DVDLowReadDiskID:
		INFO_LOG(DVDINTERFACE, "DVDLowReadDiskID");
		command_handled_by_thread = ExecuteReadCommand(0, output_address, 0x20, output_length, false,
		                                               callback_event_type, &interrupt_type, &ticks_until_completion);
		break;

	// Only used from WII_IPC. This is the only read command that decrypts data
	case DVDLowRead:
		INFO_LOG(DVDINTERFACE, "DVDLowRead: DVDAddr: 0x%09" PRIx64 ", Size: 0x%x", (u64)command_2 << 2, command_1);
		command_handled_by_thread = ExecuteReadCommand((u64)command_2 << 2, output_address, command_1, output_length, true,
		                                               callback_event_type, &interrupt_type, &ticks_until_completion);
		break;

	// Probably only used by Wii
	case DVDLowWaitForCoverClose:
		INFO_LOG(DVDINTERFACE, "DVDLowWaitForCoverClose");
		interrupt_type = (DIInterruptType)4; // ???
		break;

	// "Set Extension"...not sure what it does. GC only?
	case 0x55:
		INFO_LOG(DVDINTERFACE, "SetExtension");
		break;

	// Probably only used though WII_IPC
	case DVDLowGetCoverReg:
		WriteImmediate(s_DICVR.Hex, output_address, write_to_DIIMMBUF);
		INFO_LOG(DVDINTERFACE, "DVDLowGetCoverReg 0x%08x", s_DICVR.Hex);
		break;

	// Probably only used by Wii
	case DVDLowNotifyReset:
		ERROR_LOG(DVDINTERFACE, "DVDLowNotifyReset");
		PanicAlert("DVDLowNotifyReset");
		break;
	// Probably only used by Wii
	case DVDLowReadDvdPhysical:
		ERROR_LOG(DVDINTERFACE, "DVDLowReadDvdPhysical");
		PanicAlert("DVDLowReadDvdPhysical");
		break;
	// Probably only used by Wii
	case DVDLowReadDvdCopyright:
		ERROR_LOG(DVDINTERFACE, "DVDLowReadDvdCopyright");
		PanicAlert("DVDLowReadDvdCopyright");
		break;
	// Probably only used by Wii
	case DVDLowReadDvdDiscKey:
		ERROR_LOG(DVDINTERFACE, "DVDLowReadDvdDiscKey");
		PanicAlert("DVDLowReadDvdDiscKey");
		break;

	// Probably only used by Wii
	case DVDLowClearCoverInterrupt:
		INFO_LOG(DVDINTERFACE, "DVDLowClearCoverInterrupt");
		s_DICVR.CVRINT = 0;
		break;

	// Probably only used by Wii
	case DVDLowGetCoverStatus:
		WriteImmediate(s_disc_inside ? 2 : 1, output_address, write_to_DIIMMBUF);
		INFO_LOG(DVDINTERFACE, "DVDLowGetCoverStatus: Disc %sInserted", s_disc_inside ? "" : "Not ");
		break;

	// Probably only used by Wii
	case DVDLowReset:
		INFO_LOG(DVDINTERFACE, "DVDLowReset");
		break;

	// Probably only used by Wii
	case DVDLowClosePartition:
		INFO_LOG(DVDINTERFACE, "DVDLowClosePartition");
		break;

	// Probably only used by Wii
	case DVDLowUnencryptedRead:
		INFO_LOG(DVDINTERFACE, "DVDLowUnencryptedRead: DVDAddr: 0x%09" PRIx64 ", Size: 0x%x", (u64)command_2 << 2, command_1);

		// We must make sure it is in a valid area! (#001 check)
		// Are these checks correct? They seem to mix 32-bit offsets and 8-bit lengths
		// * 0x00000000 - 0x00014000 (limit of older IOS versions)
		// * 0x460a0000 - 0x460a0008
		// * 0x7ed40000 - 0x7ed40008
		if (((command_2 > 0x00000000 && command_2 < 0x00014000) ||
			(((command_2 + command_1) > 0x00000000) && (command_2 + command_1) < 0x00014000) ||
			(command_2 > 0x460a0000 && command_2 < 0x460a0008) ||
			(((command_2 + command_1) > 0x460a0000) && (command_2 + command_1) < 0x460a0008) ||
			(command_2 > 0x7ed40000 && command_2 < 0x7ed40008) ||
			(((command_2 + command_1) > 0x7ed40000) && (command_2 + command_1) < 0x7ed40008)))
		{
			command_handled_by_thread = ExecuteReadCommand((u64)command_2 << 2, output_address, command_1, output_length, false,
			                                               callback_event_type, &interrupt_type, &ticks_until_completion);
		}
		else
		{
			WARN_LOG(DVDINTERFACE, "DVDLowUnencryptedRead: trying to read out of bounds @ %09" PRIx64, (u64)command_2 << 2);
			s_error_code = ERROR_READY | ERROR_BLOCK_OOB;
			// Should cause software to call DVDLowRequestError
			interrupt_type = INT_BRKINT;
		}

		break;

	// Probably only used by Wii
	case DVDLowEnableDvdVideo:
		ERROR_LOG(DVDINTERFACE, "DVDLowEnableDvdVideo");
		break;

	// New Super Mario Bros. Wii sends these commands,
	// but it seems we don't need to implement anything.
	// Probably only used by Wii
	case 0x95:
	case 0x96:
		ERROR_LOG(DVDINTERFACE, "Unimplemented BCA command 0x%08x (Buffer 0x%08x, 0x%x)",
		          command_0, output_address, output_length);
		break;

	// Probably only used by Wii
	case DVDLowReportKey:
		INFO_LOG(DVDINTERFACE, "DVDLowReportKey");
		// Does not work on retail discs/drives
		// Retail games send this command to see if they are running on real retail hw
		s_error_code = ERROR_READY | ERROR_INV_CMD;
		interrupt_type = INT_BRKINT;
		break;

	// DMA Read from Disc. Only seems to be used through direct access, not WII_IPC
	case 0xA8:
		switch (command_0 & 0xFF)
		{
		case 0x00: // Read Sector
			{
				u64 iDVDOffset = (u64)command_1 << 2;

				INFO_LOG(DVDINTERFACE, "Read: DVDOffset=%08" PRIx64 ", DMABuffer = %08x, SrcLength = %08x, DMALength = %08x",
					        iDVDOffset, output_address, command_2, output_length);

				if (GCAM)
				{
					if (iDVDOffset & 0x80000000) // read request to hardware buffer
					{
						switch (iDVDOffset)
						{
						case 0x80000000:
							ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD STATUS (80000000)");
							for (u32 i = 0; i < output_length; i += 4)
								Memory::Write_U32(0, output_address + i);
							break;
						case 0x80000040:
							ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD STATUS (2) (80000040)");
							for (u32 i = 0; i < output_length; i += 4)
								Memory::Write_U32(~0, output_address + i);
							Memory::Write_U32(0x00000020, output_address); // DIMM SIZE, LE
							Memory::Write_U32(0x4743414D, output_address + 4); // GCAM signature
							break;
						case 0x80000120:
							ERROR_LOG(DVDINTERFACE, "GC-AM: READ FIRMWARE STATUS (80000120)");
							for (u32 i = 0; i < output_length; i += 4)
								Memory::Write_U32(0x01010101, output_address + i);
							break;
						case 0x80000140:
							ERROR_LOG(DVDINTERFACE, "GC-AM: READ FIRMWARE STATUS (80000140)");
							for (u32 i = 0; i < output_length; i += 4)
								Memory::Write_U32(0x01010101, output_address + i);
							break;
						case 0x84000020:
							ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD STATUS (1) (84000020)");
							for (u32 i = 0; i < output_length; i += 4)
								Memory::Write_U32(0x00000000, output_address + i);
							break;
						default:
							ERROR_LOG(DVDINTERFACE, "GC-AM: UNKNOWN MEDIA BOARD LOCATION %" PRIx64, iDVDOffset);
							break;
						}
						break;
					}
					else if ((iDVDOffset == 0x1f900000) || (iDVDOffset == 0x1f900020))
					{
						ERROR_LOG(DVDINTERFACE, "GC-AM: READ MEDIA BOARD COMM AREA (1f900020)");
						u8* source = s_media_buffer + iDVDOffset - 0x1f900000;
						Memory::CopyToEmu(output_address, source, output_length);
						for (u32 i = 0; i < output_length; i += 4)
							ERROR_LOG(DVDINTERFACE, "GC-AM: %08x", Memory::Read_U32(output_address + i));
						break;
					}
				}

				command_handled_by_thread = ExecuteReadCommand(iDVDOffset, output_address, command_2, output_length, false,
				                                               callback_event_type, &interrupt_type, &ticks_until_completion);
			}
			break;

		case 0x40: // Read DiscID
			INFO_LOG(DVDINTERFACE, "Read DiscID %08x", Memory::Read_U32(output_address));
			command_handled_by_thread = ExecuteReadCommand(0, output_address, 0x20, output_length, false,
			                                               callback_event_type, &interrupt_type, &ticks_until_completion);
			break;

		default:
			ERROR_LOG(DVDINTERFACE, "Unknown read subcommand: %08x", command_0);
			break;
		}
		break;

	// GC-AM only
	case 0xAA:
		if (GCAM)
		{
			ERROR_LOG(DVDINTERFACE, "GC-AM: 0xAA, DMABuffer=%08x, DMALength=%08x", output_address, output_length);
			u64 iDVDOffset = (u64)command_1 << 2;
			u32 len = output_length;
			s64 offset = iDVDOffset - 0x1F900000;
			/*
			if (iDVDOffset == 0x84800000)
			{
				ERROR_LOG(DVDINTERFACE, "Firmware upload");
			}
			else*/
			if ((offset < 0) || ((offset + len) > 0x40) || len > 0x40)
			{
				u32 addr = output_address;
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
					ERROR_LOG(DVDINTERFACE, "GC-AM Media Board WRITE (0xAA): %08" PRIx64 ": %08x", iDVDOffset, Memory::Read_U32(addr));
					addr += 4;
					len -= 4;
					iDVDOffset += 4;
				}
			}
			else
			{
				u32 addr = s_DIMAR.Address;
				Memory::CopyFromEmu(s_media_buffer + offset, addr, len);
				while (len >= 4)
				{
					ERROR_LOG(DVDINTERFACE, "GC-AM Media Board WRITE (0xAA): %08" PRIx64 ": %08x", iDVDOffset, Memory::Read_U32(addr));
					addr += 4;
					len -= 4;
					iDVDOffset += 4;
				}
			}
		}
		break;

	// Seems to be used by both GC and Wii
	case DVDLowSeek:
		if (!GCAM)
		{
			// Currently unimplemented
			INFO_LOG(DVDINTERFACE, "Seek: offset=%09" PRIx64 " (ignoring)", (u64)command_1 << 2);
		}
		else
		{
			memset(s_media_buffer, 0, 0x20);
			s_media_buffer[0] = s_media_buffer[0x20]; // ID
			s_media_buffer[2] = s_media_buffer[0x22];
			s_media_buffer[3] = s_media_buffer[0x23] | 0x80;
			int cmd = (s_media_buffer[0x23]<<8)|s_media_buffer[0x22];
			ERROR_LOG(DVDINTERFACE, "GC-AM: execute buffer, cmd=%04x", cmd);
			switch (cmd)
			{
			case 0x00:
				s_media_buffer[4] = 1;
				break;
			case 0x1:
				s_media_buffer[7] = 0x20; // DIMM Size
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
					s_media_buffer[4] = status;
					/* status:
					0 - "Initializing media board. Please wait.."
					1 - "Checking network. Please wait..."
					2 - "Found a system disc. Insert a game disc"
					3 - "Testing a game program. %d%%"
					4 - "Loading a game program. %d%%"
					5  - go
					6  - error xx
					*/
					s_media_buffer[8] = percentage;
					s_media_buffer[4] = 0x05;
					s_media_buffer[8] = 0x64;
					break;
				}
			case 0x101:
				s_media_buffer[4] = 3; // version
				s_media_buffer[5] = 3;
				s_media_buffer[6] = 1; // xxx
				s_media_buffer[8] = 1;
				s_media_buffer[16] = 0xFF;
				s_media_buffer[17] = 0xFF;
				s_media_buffer[18] = 0xFF;
				s_media_buffer[19] = 0xFF;
				break;
			case 0x102:  // get error code
				s_media_buffer[4] = 1; // 0: download incomplete (31), 1: corrupted, other error 1
				s_media_buffer[5] = 0;
				break;
			case 0x103:
				memcpy(s_media_buffer + 4, "A89E27A50364511", 15);  // serial
				break;
#if 0
			case 0x301: // unknown
				memcpy(s_media_buffer + 4, s_media_buffer + 0x24, 0x1c);
				break;
			case 0x302:
				break;
#endif
			default:
				ERROR_LOG(DVDINTERFACE, "GC-AM: execute buffer (unknown)");
				break;
			}
			memset(s_media_buffer + 0x20, 0, 0x20);
			WriteImmediate(0x66556677, output_address, write_to_DIIMMBUF); // just a random value that works.
		}
		break;

	// Probably only used by Wii
	case DVDLowReadDvd:
		ERROR_LOG(DVDINTERFACE, "DVDLowReadDvd");
		break;
	// Probably only used by Wii
	case DVDLowReadDvdConfig:
		ERROR_LOG(DVDINTERFACE, "DVDLowReadDvdConfig");
		break;
	// Probably only used by Wii
	case DVDLowStopLaser:
		ERROR_LOG(DVDINTERFACE, "DVDLowStopLaser");
		break;
	// Probably only used by Wii
	case DVDLowOffset:
		ERROR_LOG(DVDINTERFACE, "DVDLowOffset");
		break;
	// Probably only used by Wii
	case DVDLowReadDiskBca:
		WARN_LOG(DVDINTERFACE, "DVDLowReadDiskBca");
		Memory::Write_U32(1, output_address + 0x30);
		break;
	// Probably only used by Wii
	case DVDLowRequestDiscStatus:
		ERROR_LOG(DVDINTERFACE, "DVDLowRequestDiscStatus");
		break;
	// Probably only used by Wii
	case DVDLowRequestRetryNumber:
		ERROR_LOG(DVDINTERFACE, "DVDLowRequestRetryNumber");
		break;
	// Probably only used by Wii
	case DVDLowSetMaximumRotation:
		ERROR_LOG(DVDINTERFACE, "DVDLowSetMaximumRotation");
		break;
	// Probably only used by Wii
	case DVDLowSerMeasControl:
		ERROR_LOG(DVDINTERFACE, "DVDLowSerMeasControl");
		break;

	// Used by both GC and Wii
	case DVDLowRequestError:
		INFO_LOG(DVDINTERFACE, "Requesting error... (0x%08x)", s_error_code);
		WriteImmediate(s_error_code, output_address, write_to_DIIMMBUF);
		s_error_code = 0;
		break;

	// Audio Stream (Immediate). Only seems to be used by some GC games
	// (command_0 >> 16) & 0xFF = Subcommand
	// command_1 << 2           = Offset on disc
	// command_2                = Length of the stream
	case 0xE1:
		{
			u8 cancel_stream = (command_0 >> 16) & 0xFF;
			if (cancel_stream)
			{
				s_stop_at_track_end = false;
				s_stream = false;
				s_audio_pos = 0;
				s_next_start = 0;
				s_next_length = 0;
				s_current_start = 0;
				s_current_length = 0;
			}
			else
			{
				if ((command_1 == 0) && (command_2 == 0))
				{
					s_stop_at_track_end = true;
				}
				else if (!s_stop_at_track_end)
				{
					// Setting s_next_start (a u32) like this discards two bits,
					// but GC games can't be 4 GiB big, so it shouldn't matter
					s_next_start = command_1 << 2;
					s_next_length = command_2;
					if (!s_stream)
					{
						s_current_start = s_next_start;
						s_current_length = s_next_length;
						s_audio_pos = s_current_start;
						StreamADPCM::InitFilter();
						s_stream = true;
					}
				}
			}

			INFO_LOG(DVDINTERFACE, "(Audio) Stream cmd: %08x offset: %08" PRIx64 " length: %08x",
			         command_0, (u64)command_1 << 2, command_2);
		}
		break;

	// Request Audio Status (Immediate). Only seems to be used by some GC games
	case 0xE2:
		{
			switch (command_0 >> 16 & 0xFF)
			{
			case 0x00: // Returns streaming status
				INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:%08x/%08x CurrentStart:%08x CurrentLength:%08x",
				         s_audio_pos, s_current_start + s_current_length, s_current_start, s_current_length);
				WriteImmediate(s_stream ? 1 : 0, output_address, write_to_DIIMMBUF);
				break;
			case 0x01: // Returns the current offset
				INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:%08x", s_audio_pos);
				WriteImmediate(s_audio_pos >> 2, output_address, write_to_DIIMMBUF);
				break;
			case 0x02: // Returns the start offset
				INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentStart:%08x", s_current_start);
				WriteImmediate(s_current_start >> 2, output_address, write_to_DIIMMBUF);
				break;
			case 0x03: // Returns the total length
				INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentLength:%08x", s_current_length);
				WriteImmediate(s_current_length >> 2, output_address, write_to_DIIMMBUF);
				break;
			default:
				WARN_LOG(DVDINTERFACE, "(Audio): Subcommand: %02x  Request Audio status %s", command_0 >> 16 & 0xFF, s_stream ? "on" : "off");
				break;
			}
		}
		break;

	case DVDLowStopMotor:
		INFO_LOG(DVDINTERFACE, "DVDLowStopMotor %s %s",
		         command_1 ? "eject" : "", command_2 ? "kill!" : "");

		if (command_1 && !command_2)
			EjectDiscCallback(0, 0);
		break;

	// DVD Audio Enable/Disable (Immediate). GC uses this, and apparently Wii also does...?
	case DVDLowAudioBufferConfig:
		// For more information: http://www.crazynation.org/GC/GC_DD_TECH/GCTech.htm (dead link?)
		//
		// Upon Power up or reset , 2 commands must be issued for proper use of audio streaming:
		// DVDReadDiskID A8000040,00000000,00000020
		// DVDLowAudioBufferConfig E4xx00yy,00000000,00000020
		//
		// xx=byte 8 [0 or 1] from the disk header retrieved from DVDReadDiskID
		// yy=0 (if xx=0) or 0xA (if xx=1)

		if ((command_0 >> 16) & 0xFF)
		{
			// TODO: What is this actually supposed to do?
			s_stream = true;
			WARN_LOG(DVDINTERFACE, "(Audio): Audio enabled");
		}
		else
		{
			// TODO: What is this actually supposed to do?
			s_stream = false;
			WARN_LOG(DVDINTERFACE, "(Audio): Audio disabled");
		}
		break;

	// yet another (GC?) command we prolly don't care about
	case 0xEE:
		INFO_LOG(DVDINTERFACE, "SetStatus");
		break;

	// Debug commands; see yagcd. We don't really care
	// NOTE: commands to stream data will send...a raw data stream
	// This will appear as unknown commands, unless the check is re-instated to catch such data.
	// Can probably only be used through direct access
	case 0xFE:
		ERROR_LOG(DVDINTERFACE, "Unsupported DVD Drive debug command 0x%08x", command_0);
		break;

	// Unlock Commands. 1: "MATSHITA" 2: "DVD-GAME"
	// Just for fun
	// Can probably only be used through direct access
	case 0xFF:
		{
			if (command_0 == 0xFF014D41 &&
			    command_1 == 0x54534849 &&
			    command_2 == 0x54410200)
			{
				INFO_LOG(DVDINTERFACE, "Unlock test 1 passed");
			}
			else if (command_0 == 0xFF004456 &&
			         command_1 == 0x442D4741 &&
			         command_2 == 0x4D450300)
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
		ERROR_LOG(DVDINTERFACE, "Unknown command 0x%08x (Buffer 0x%08x, 0x%x)",
		          command_0, output_address, output_length);
		PanicAlertT("Unknown DVD command %08x - fatal error", command_0);
		break;
	}

	// The command will finish executing after a delay
	// to simulate the speed of a real disc drive
	if (!command_handled_by_thread)
		CoreTiming::ScheduleEvent((int)ticks_until_completion, callback_event_type, interrupt_type);
}

// Simulates the timing aspects of reading data from a disc.
// Returns the amount of ticks needed to finish executing the command,
// and sets some state that is used the next time this function runs.
u64 SimulateDiscReadTime(u64 offset, u32 length)
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

	u64 current_time = CoreTiming::GetTicks();
	u64 ticks_until_completion;

	// Number of ticks it takes to seek and read directly from the disk.
	u64 disk_read_duration = CalculateRawDiscReadTime(offset, length) +
		SystemTimers::GetTicksPerSecond() / 1000 * DISC_ACCESS_TIME_MS;

	if (offset + length - s_last_read_offset > 1024 * 1024)
	{
		// No buffer; just use the simple seek time + read time.
		DEBUG_LOG(DVDINTERFACE, "Seeking %" PRId64 " bytes",
		          s64(s_last_read_offset) - s64(offset));
		ticks_until_completion = disk_read_duration;
		s_last_read_time = current_time + ticks_until_completion;
	}
	else
	{
		// Possibly buffered; use the buffer if it saves time.
		// It's not proven that the buffer actually behaves like this, but
		// it appears to be a decent approximation.

		// Time at which the buffer will contain the data we need.
		u64 buffer_fill_time = s_last_read_time +
		                       CalculateRawDiscReadTime(s_last_read_offset,
		                       offset + length - s_last_read_offset);
		// Number of ticks it takes to transfer the data from the buffer to memory.
		u64 buffer_read_duration = length *
			(SystemTimers::GetTicksPerSecond() / BUFFER_TRANSFER_RATE);

		if (current_time > buffer_fill_time)
		{
			DEBUG_LOG(DVDINTERFACE, "Fast buffer read at %" PRIx64, offset);
			ticks_until_completion = buffer_read_duration;
			s_last_read_time = buffer_fill_time;
		}
		else if (current_time + disk_read_duration > buffer_fill_time)
		{
			DEBUG_LOG(DVDINTERFACE, "Slow buffer read at %" PRIx64, offset);
			ticks_until_completion = std::max(buffer_fill_time - current_time,
			                                  buffer_read_duration);
			s_last_read_time = buffer_fill_time;
		}
		else
		{
			DEBUG_LOG(DVDINTERFACE, "Short seek %" PRId64 " bytes",
			          s64(s_last_read_offset) - s64(offset));
			ticks_until_completion = disk_read_duration;
			s_last_read_time = current_time + ticks_until_completion;
		}
	}

	s_last_read_offset = ROUND_DOWN(offset + length - 2048, 2048);

	return ticks_until_completion;
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
	if (s_inserted_volume->GetVolumeType() == DiscIO::IVolume::WII_DISC)
	{
		speed = std::sqrt(((average_offset - WII_DISC_LOCATION_1_OFFSET) /
			WII_BYTES_PER_AREA_UNIT + WII_DISC_AREA_UP_TO_LOCATION_1) / PI);
	}
	else
	{
		speed = std::sqrt(((average_offset - GC_DISC_LOCATION_1_OFFSET) /
			GC_BYTES_PER_AREA_UNIT + GC_DISC_AREA_UP_TO_LOCATION_1) / PI);
	}
	DEBUG_LOG(DVDINTERFACE, "Disc speed: %f MiB/s", speed / 1024 / 1024);

	return (s64)(SystemTimers::GetTicksPerSecond() / speed * length);
}

}  // namespace
