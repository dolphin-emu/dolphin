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
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/DVDThread.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/StreamADPCM.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_DI.h"
#include "Core/Movie.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

// The minimum time it takes for the DVD drive to process a command (in
// microseconds)
constexpr u64 COMMAND_LATENCY_US = 300;

// The size of the streaming buffer.
constexpr u64 STREAMING_BUFFER_SIZE = 1024 * 1024;

// A single DVD disc sector
constexpr u64 DVD_SECTOR_SIZE = 0x800;

// The minimum amount that a drive will read
constexpr u64 DVD_ECC_BLOCK_SIZE = 16 * DVD_SECTOR_SIZE;

// Rate the drive can transfer data to main memory, given the data
// is already buffered. Measured in bytes per second.
constexpr u64 BUFFER_TRANSFER_RATE = 1024 * 1024 * 32;

// The size of the first Wii disc layer in bytes (2294912 sectors per layer)
constexpr u64 WII_DISC_LAYER_SIZE = 2294912 * DVD_SECTOR_SIZE;

// 24 mm
constexpr double DVD_INNER_RADIUS = 0.024;
// 58 mm
constexpr double WII_DVD_OUTER_RADIUS = 0.058;
// 38 mm
constexpr double GC_DVD_OUTER_RADIUS = 0.038;

// Approximate read speeds at the inner and outer locations of Wii and GC
// discs. These speeds are approximations of speeds measured on real Wiis.
constexpr double GC_DISC_INNER_READ_SPEED = 1024 * 1024 * 2.1;    // bytes/s
constexpr double GC_DISC_OUTER_READ_SPEED = 1024 * 1024 * 3.325;  // bytes/s
constexpr double WII_DISC_INNER_READ_SPEED = 1024 * 1024 * 3.5;   // bytes/s
constexpr double WII_DISC_OUTER_READ_SPEED = 1024 * 1024 * 8.45;  // bytes/s

// Experimentally measured seek constants. The time to seek appears to be
// linear, but short seeks appear to be lower velocity.
constexpr double SHORT_SEEK_DISTANCE = 0.001;       // 1mm
constexpr double SHORT_SEEK_CONSTANT = 0.045;       // seconds
constexpr double SHORT_SEEK_VELOCITY_INVERSE = 50;  // inverse: s/m
constexpr double LONG_SEEK_CONSTANT = 0.085;        // seconds
constexpr double LONG_SEEK_VELOCITY_INVERSE = 4.5;  // inverse: s/m

namespace DVDInterface
{
// internal hardware addresses
enum
{
  DI_STATUS_REGISTER = 0x00,
  DI_COVER_REGISTER = 0x04,
  DI_COMMAND_0 = 0x08,
  DI_COMMAND_1 = 0x0C,
  DI_COMMAND_2 = 0x10,
  DI_DMA_ADDRESS_REGISTER = 0x14,
  DI_DMA_LENGTH_REGISTER = 0x18,
  DI_DMA_CONTROL_REGISTER = 0x1C,
  DI_IMMEDIATE_DATA_BUFFER = 0x20,
  DI_CONFIG_REGISTER = 0x24
};

// debug commands which may be ORd
enum
{
  STOP_DRIVE = 0,
  START_DRIVE = 0x100,
  ACCEPT_COPY = 0x4000,
  DISC_CHECK = 0x8000,
};

// DI Status Register
union UDISR {
  u32 Hex;
  struct
  {
    u32 BREAK : 1;       // Stop the Device + Interrupt
    u32 DEINITMASK : 1;  // Access Device Error Int Mask
    u32 DEINT : 1;       // Access Device Error Int
    u32 TCINTMASK : 1;   // Transfer Complete Int Mask
    u32 TCINT : 1;       // Transfer Complete Int
    u32 BRKINTMASK : 1;
    u32 BRKINT : 1;  // w 1: clear brkint
    u32 : 25;
  };
  UDISR() { Hex = 0; }
  UDISR(u32 _hex) { Hex = _hex; }
};

// DI Cover Register
union UDICVR {
  u32 Hex;
  struct
  {
    u32 CVR : 1;         // 0: Cover closed  1: Cover open
    u32 CVRINTMASK : 1;  // 1: Interrupt enabled
    u32 CVRINT : 1;      // r 1: Interrupt requested w 1: Interrupt clear
    u32 : 29;
  };
  UDICVR() { Hex = 0; }
  UDICVR(u32 _hex) { Hex = _hex; }
};

union UDICMDBUF {
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
union UDIMAR {
  u32 Hex;
  struct
  {
    u32 Zerobits : 5;  // Must be zero (32byte aligned)
    u32 : 27;
  };
  struct
  {
    u32 Address : 26;
    u32 : 6;
  };
};

// DI DMA Address Length Register
union UDILENGTH {
  u32 Hex;
  struct
  {
    u32 Zerobits : 5;  // Must be zero (32byte aligned)
    u32 : 27;
  };
  struct
  {
    u32 Length : 26;
    u32 : 6;
  };
};

// DI DMA Control Register
union UDICR {
  u32 Hex;
  struct
  {
    u32 TSTART : 1;  // w:1 start   r:0 ready
    u32 DMA : 1;     // 1: DMA Mode    0: Immediate Mode (can only do Access Register Command)
    u32 RW : 1;      // 0: Read Command (DVD to Memory)  1: Write Command (Memory to DVD)
    u32 : 29;
  };
};

union UDIIMMBUF {
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
union UDICFG {
  u32 Hex;
  struct
  {
    u32 CONFIG : 8;
    u32 : 24;
  };
  UDICFG() { Hex = 0; }
  UDICFG(u32 _hex) { Hex = _hex; }
};

static std::unique_ptr<DiscIO::IVolume> s_inserted_volume;

// STATE_TO_SAVE
// hardware registers
static UDISR s_DISR;
static UDICVR s_DICVR;
static UDICMDBUF s_DICMDBUF[3];
static UDIMAR s_DIMAR;
static UDILENGTH s_DILENGTH;
static UDICR s_DICR;
static UDIIMMBUF s_DIIMMBUF;
static UDICFG s_DICFG;

static u32 s_audio_position;
static u32 s_current_start;
static u32 s_current_length;
static u32 s_next_start;
static u32 s_next_length;

static u32 s_error_code = 0;
static bool s_disc_inside = false;
static bool s_stream = false;
static bool s_stop_at_track_end = false;
static int s_finish_executing_command = 0;
static int s_finish_read = 0;
static int s_dtk = 0;

static u64 s_current_read_dvd_offset;
static u32 s_current_read_length;
static u32 s_current_read_dma_offset;
static bool s_current_read_reply_to_ios;

static u64 s_read_buffer_start_time;
static u64 s_read_buffer_end_time;
static u64 s_read_buffer_start_offset;
static u64 s_read_buffer_end_offset;

// GC-AM only
static unsigned char s_media_buffer[0x40];

static int s_eject_disc;
static int s_insert_disc;

static void EjectDiscCallback(u64 userdata, s64 cyclesLate);
static void InsertDiscCallback(u64 userdata, s64 cyclesLate);
static void FinishRead(u64 userdata, s64 cyclesLate);
static void FinishExecutingCommandCallback(u64 userdata, s64 cycles_late);

void SetLidOpen(bool _bOpen);

void UpdateInterrupts();
void GenerateDIInterrupt(DIInterruptType _DVDInterrupt);

void WriteImmediate(u32 value, u32 output_address, bool reply_to_ios);
bool ExecuteReadCommand(u64 DVD_offset, u32 output_address, u32 DVD_length, u32 output_length,
                        bool decrypt, bool reply_to_ios, DIInterruptType* interrupt_type);

void ScheduleReads(u64 dvd_offset, u32 length, bool decrypt, u32 output_address,
                   bool reply_to_ios);
double CalculatePhysicalDiscPosition(u64 offset);
u64 CalculateSeekTime(u64 offset_from, u64 offset_to);
u64 CalculateRawDiscReadTime(u64 offset, u32 length);

void DoState(PointerWrap& p)
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
  p.Do(s_audio_position);
  p.Do(s_next_length);

  p.Do(s_error_code);
  p.Do(s_disc_inside);
  p.Do(s_stream);

  p.Do(s_current_start);
  p.Do(s_current_length);

  p.Do(s_current_read_dvd_offset);
  p.Do(s_current_read_length);
  p.Do(s_current_read_dma_offset);
  p.Do(s_current_read_reply_to_ios);
  p.Do(s_read_buffer_start_time);
  p.Do(s_read_buffer_end_time);
  p.Do(s_read_buffer_start_offset);
  p.Do(s_read_buffer_end_offset);

  p.Do(s_stop_at_track_end);

  DVDThread::DoState(p);
}

static void FinishRead(u64 userdata, s64 cyclesLate)
{
  u32 offset = userdata >> 32;
  u32 length = userdata & 0xffffffff;

  bool complete = offset + length == s_current_read_length;

  if (!DVDThread::CompleteRead())
  {
    // Don't attempt to DMA if we failed to read
    if (complete)
    {
      // If this is the last step of the read, return a failure response
      DEBUG_LOG(DVDINTERFACE, "Read failed");
      DVDThread::Cleanup();
      FinishExecutingCommand(s_current_read_reply_to_ios, DVDInterface::INT_DEINT);
    }

    return;
  }

  DVDThread::DMA(offset, s_current_read_dma_offset + offset, length);

  if (complete)
  {
    DEBUG_LOG(DVDINTERFACE, "Read complete");
    DVDThread::Cleanup();
    FinishExecutingCommand(s_current_read_reply_to_ios, DVDInterface::INT_TCINT);
  }
}

static u32 ProcessDTKSamples(short* tempPCM, u32 num_samples)
{
  // TODO: Read audio data using the DVD thread instead of blocking on it?
  DVDThread::WaitUntilIdle();

  u32 samples_processed = 0;
  do
  {
    if (s_audio_position >= s_current_start + s_current_length)
    {
      DEBUG_LOG(DVDINTERFACE, "ProcessDTKSamples: "
                              "NextStart=%08x,NextLength=%08x,CurrentStart=%08x,CurrentLength=%08x,"
                              "AudioPos=%08x",
                s_next_start, s_next_length, s_current_start, s_current_length, s_audio_position);

      s_audio_position = s_next_start;
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
    // TODO: What if we can't read from s_audio_position?
    s_inserted_volume->Read(s_audio_position, sizeof(tempADPCM), tempADPCM, false);
    s_audio_position += sizeof(tempADPCM);
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

static void DTKStreamingCallback(u64 userdata, s64 cyclesLate)
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

  s_DISR.Hex = 0;
  s_DICVR.Hex = 1;  // Disc Channel relies on cover being open when no disc is inserted
  s_DICMDBUF[0].Hex = 0;
  s_DICMDBUF[1].Hex = 0;
  s_DICMDBUF[2].Hex = 0;
  s_DIMAR.Hex = 0;
  s_DILENGTH.Hex = 0;
  s_DICR.Hex = 0;
  s_DIIMMBUF.Hex = 0;
  s_DICFG.Hex = 0;
  s_DICFG.CONFIG = 1;  // Disable bootrom descrambler

  s_audio_position = 0;
  s_next_start = 0;
  s_next_length = 0;
  s_current_start = 0;
  s_current_length = 0;

  s_error_code = 0;
  s_disc_inside = false;
  s_stream = false;
  s_stop_at_track_end = false;

  // No buffer to start
  s_read_buffer_start_offset = 0;
  s_read_buffer_end_offset = 0;
  s_read_buffer_start_time = 0;
  s_read_buffer_end_time = 0;

  s_eject_disc = CoreTiming::RegisterEvent("EjectDisc", EjectDiscCallback);
  s_insert_disc = CoreTiming::RegisterEvent("InsertDisc", InsertDiscCallback);

  s_finish_read = CoreTiming::RegisterEvent("FinishRead", FinishRead);
  s_finish_executing_command =
      CoreTiming::RegisterEvent("FinishExecutingCommand", FinishExecutingCommandCallback);
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

bool SetVolumeDirectory(const std::string& full_path, bool is_wii,
                        const std::string& apploader_path, const std::string& DOL_path)
{
  DVDThread::WaitUntilIdle();
  s_inserted_volume =
      DiscIO::CreateVolumeFromDirectory(full_path, is_wii, apploader_path, DOL_path);
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
static void EjectDiscCallback(u64 userdata, s64 cyclesLate)
{
  DVDThread::WaitUntilIdle();
  s_inserted_volume.reset();
  SetDiscInside(false);
}

static void InsertDiscCallback(u64 userdata, s64 cyclesLate)
{
  std::string& SavedFileName = SConfig::GetInstance().m_strFilename;
  std::string* _FileName = (std::string*)userdata;

  if (!SetVolumeName(*_FileName))
  {
    // Put back the old one
    SetVolumeName(SavedFileName);
    PanicAlertT("Invalid file");
  }
  SetDiscInside(VolumeIsValid());
  delete _FileName;
}

// Can only be called by the host thread
void ChangeDiscAsHost(const std::string& newFileName)
{
  bool was_unpaused = Core::PauseAndLock(true);

  // The host thread is now temporarily the CPU thread
  ChangeDiscAsCPU(newFileName);

  Core::PauseAndLock(false, was_unpaused);
}

// Can only be called by the CPU thread
void ChangeDiscAsCPU(const std::string& newFileName)
{
  std::string* _FileName = new std::string(newFileName);
  CoreTiming::ScheduleEvent(0, s_eject_disc);
  CoreTiming::ScheduleEvent(500000000, s_insert_disc, (u64)_FileName);
  if (Movie::IsRecordingInput())
  {
    std::string fileName = newFileName;
    auto sizeofpath = fileName.find_last_of("/\\") + 1;
    if (fileName.substr(sizeofpath).length() > 40)
    {
      PanicAlertT("The disc change to \"%s\" could not be saved in the .dtm file.\n"
                  "The filename of the disc image must not be longer than 40 characters.",
                  newFileName.c_str());
    }
    Movie::SignalDiscChange(fileName.substr(sizeofpath));
  }
}

void SetLidOpen(bool open)
{
  s_DICVR.CVR = open ? 1 : 0;

  GenerateDIInterrupt(INT_CVRINT);
}

bool ChangePartition(u64 offset)
{
  DVDThread::WaitUntilIdle();
  return s_inserted_volume->ChangePartition(offset);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  mmio->Register(base | DI_STATUS_REGISTER, MMIO::DirectRead<u32>(&s_DISR.Hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   UDISR tmpStatusReg(val);

                   s_DISR.DEINITMASK = tmpStatusReg.DEINITMASK;
                   s_DISR.TCINTMASK = tmpStatusReg.TCINTMASK;
                   s_DISR.BRKINTMASK = tmpStatusReg.BRKINTMASK;
                   s_DISR.BREAK = tmpStatusReg.BREAK;

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
                 }));

  mmio->Register(base | DI_COVER_REGISTER, MMIO::DirectRead<u32>(&s_DICVR.Hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   UDICVR tmpCoverReg(val);

                   s_DICVR.CVRINTMASK = tmpCoverReg.CVRINTMASK;

                   if (tmpCoverReg.CVRINT)
                     s_DICVR.CVRINT = 0;

                   UpdateInterrupts();
                 }));

  // Command registers are very similar and we can register them with a
  // simple loop.
  for (int i = 0; i < 3; ++i)
    mmio->Register(base | (DI_COMMAND_0 + 4 * i), MMIO::DirectRead<u32>(&s_DICMDBUF[i].Hex),
                   MMIO::DirectWrite<u32>(&s_DICMDBUF[i].Hex));

  // DMA related registers. Mostly direct accesses (+ masking for writes to
  // handle things like address alignment) and complex write on the DMA
  // control register that will trigger the DMA.
  mmio->Register(base | DI_DMA_ADDRESS_REGISTER, MMIO::DirectRead<u32>(&s_DIMAR.Hex),
                 MMIO::DirectWrite<u32>(&s_DIMAR.Hex, ~0xFC00001F));
  mmio->Register(base | DI_DMA_LENGTH_REGISTER, MMIO::DirectRead<u32>(&s_DILENGTH.Hex),
                 MMIO::DirectWrite<u32>(&s_DILENGTH.Hex, ~0x1F));
  mmio->Register(base | DI_DMA_CONTROL_REGISTER, MMIO::DirectRead<u32>(&s_DICR.Hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   s_DICR.Hex = val & 7;
                   if (s_DICR.TSTART)
                   {
                     ExecuteCommand(s_DICMDBUF[0].Hex, s_DICMDBUF[1].Hex, s_DICMDBUF[2].Hex,
                                    s_DIMAR.Hex, s_DILENGTH.Hex, false);
                   }
                 }));

  mmio->Register(base | DI_IMMEDIATE_DATA_BUFFER, MMIO::DirectRead<u32>(&s_DIIMMBUF.Hex),
                 MMIO::DirectWrite<u32>(&s_DIIMMBUF.Hex));

  // DI config register is read only.
  mmio->Register(base | DI_CONFIG_REGISTER, MMIO::DirectRead<u32>(&s_DICFG.Hex),
                 MMIO::InvalidWrite<u32>());
}

void UpdateInterrupts()
{
  if ((s_DISR.DEINT & s_DISR.DEINITMASK) || (s_DISR.TCINT & s_DISR.TCINTMASK) ||
      (s_DISR.BRKINT & s_DISR.BRKINTMASK) || (s_DICVR.CVRINT & s_DICVR.CVRINTMASK))
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

void GenerateDIInterrupt(DIInterruptType dvd_interrupt)
{
  switch (dvd_interrupt)
  {
  case INT_DEINT:
    s_DISR.DEINT = 1;
    break;
  case INT_TCINT:
    s_DISR.TCINT = 1;
    break;
  case INT_BRKINT:
    s_DISR.BRKINT = 1;
    break;
  case INT_CVRINT:
    s_DICVR.CVRINT = 1;
    break;
  }

  UpdateInterrupts();
}

void WriteImmediate(u32 value, u32 output_address, bool reply_to_ios)
{
  if (reply_to_ios)
    Memory::Write_U32(value, output_address);
  else
    s_DIIMMBUF.Hex = value;
}

// Iff false is returned, ScheduleEvent must be used to finish executing the command
bool ExecuteReadCommand(u64 DVD_offset, u32 output_address, u32 DVD_length, u32 output_length,
                        bool decrypt, bool reply_to_ios, DIInterruptType* interrupt_type)
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
    WARN_LOG(
        DVDINTERFACE,
        "Detected attempt to read more data from the DVD than fit inside the out buffer. Clamp.");
    DVD_length = output_length;
  }

  DVDThread::StartRead(DVD_offset, DVD_length, decrypt);
  ScheduleReads(DVD_offset, DVD_length, decrypt, output_address, reply_to_ios);
  return true;
}

// When the command has finished executing, callback will be called using CoreTiming::ScheduleEvent,
// with the userdata set to the interrupt type.
void ExecuteCommand(u32 command_0, u32 command_1, u32 command_2, u32 output_address,
                    u32 output_length, bool reply_to_ios)
{
  DIInterruptType interrupt_type = INT_TCINT;
  bool command_handled_by_thread = false;

  bool GCAM = (SConfig::GetInstance().m_SIDevice[0] == SIDEVICE_AM_BASEBOARD) &&
              (SConfig::GetInstance().m_EXIDevice[2] == EXIDEVICE_AM_BASEBOARD);

  // DVDLowRequestError needs access to the error code set by the previous command
  if (command_0 >> 24 != DVDLowRequestError)
    s_error_code = 0;

  if (GCAM)
  {
    ERROR_LOG(DVDINTERFACE, "DVD: %08x, %08x, %08x, DMA=addr:%08x,len:%08x,ctrl:%08x", command_0,
              command_1, command_2, output_address, output_length, s_DICR.Hex);
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
      WriteImmediate(0x21000000, output_address, reply_to_ios);
    }
    else
    {
      // (shuffle2) Taken from my Wii
      Memory::Write_U32(0x00000002, output_address);
      Memory::Write_U32(0x20060526, output_address + 4);
      // This was in the oubuf even though this cmd is only supposed to reply with 64bits
      // However, this and other tests strongly suggest that the buffer is static, and it's never -
      // or rarely cleared.
      Memory::Write_U32(0x41000000, output_address + 8);

      INFO_LOG(DVDINTERFACE, "DVDLowInquiry (Buffer 0x%08x, 0x%x)", output_address, output_length);
    }
    break;

  // Only seems to be used from WII_IPC, not through direct access
  case DVDLowReadDiskID:
    INFO_LOG(DVDINTERFACE, "DVDLowReadDiskID");
    command_handled_by_thread = ExecuteReadCommand(0, output_address, 0x20, output_length, false,
                                                   reply_to_ios, &interrupt_type);
    break;

  // Only used from WII_IPC. This is the only read command that decrypts data
  case DVDLowRead:
    INFO_LOG(DVDINTERFACE, "DVDLowRead: DVDAddr: 0x%09" PRIx64 ", Size: 0x%x", (u64)command_2 << 2,
             command_1);
    command_handled_by_thread =
        ExecuteReadCommand((u64)command_2 << 2, output_address, command_1, output_length, true,
                           reply_to_ios, &interrupt_type);
    break;

  // Probably only used by Wii
  case DVDLowWaitForCoverClose:
    INFO_LOG(DVDINTERFACE, "DVDLowWaitForCoverClose");
    interrupt_type = (DIInterruptType)4;  // ???
    break;

  // "Set Extension"...not sure what it does. GC only?
  case 0x55:
    INFO_LOG(DVDINTERFACE, "SetExtension");
    break;

  // Probably only used though WII_IPC
  case DVDLowGetCoverReg:
    WriteImmediate(s_DICVR.Hex, output_address, reply_to_ios);
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
    WriteImmediate(s_disc_inside ? 2 : 1, output_address, reply_to_ios);
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
    INFO_LOG(DVDINTERFACE, "DVDLowUnencryptedRead: DVDAddr: 0x%09" PRIx64 ", Size: 0x%x",
             (u64)command_2 << 2, command_1);

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
      command_handled_by_thread =
          ExecuteReadCommand((u64)command_2 << 2, output_address, command_1, output_length, false,
                             reply_to_ios, &interrupt_type);
    }
    else
    {
      WARN_LOG(DVDINTERFACE, "DVDLowUnencryptedRead: trying to read out of bounds @ %09" PRIx64,
               (u64)command_2 << 2);
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
    ERROR_LOG(DVDINTERFACE, "Unimplemented BCA command 0x%08x (Buffer 0x%08x, 0x%x)", command_0,
              output_address, output_length);
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
    case 0x00:  // Read Sector
    {
      u64 iDVDOffset = (u64)command_1 << 2;

      INFO_LOG(DVDINTERFACE, "Read: DVDOffset=%08" PRIx64
                             ", DMABuffer = %08x, SrcLength = %08x, DMALength = %08x",
               iDVDOffset, output_address, command_2, output_length);

      if (GCAM)
      {
        if (iDVDOffset & 0x80000000)  // read request to hardware buffer
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
            Memory::Write_U32(0x00000020, output_address);      // DIMM SIZE, LE
            Memory::Write_U32(0x4743414D, output_address + 4);  // GCAM signature
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

      command_handled_by_thread =
          ExecuteReadCommand(iDVDOffset, output_address, command_2, output_length, false,
                             reply_to_ios, &interrupt_type);
    }
    break;

    case 0x40:  // Read DiscID
      INFO_LOG(DVDINTERFACE, "Read DiscID %08x", Memory::Read_U32(output_address));
      command_handled_by_thread = ExecuteReadCommand(0, output_address, 0x20, output_length, false,
                                                     reply_to_ios, &interrupt_type);
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
      ERROR_LOG(DVDINTERFACE, "GC-AM: 0xAA, DMABuffer=%08x, DMALength=%08x", output_address,
                output_length);
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
          ERROR_LOG(DVDINTERFACE, "GC-AM Media Board WRITE (0xAA): %08" PRIx64 ": %08x", iDVDOffset,
                    Memory::Read_U32(addr));
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
          ERROR_LOG(DVDINTERFACE, "GC-AM Media Board WRITE (0xAA): %08" PRIx64 ": %08x", iDVDOffset,
                    Memory::Read_U32(addr));
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
      s_media_buffer[0] = s_media_buffer[0x20];  // ID
      s_media_buffer[2] = s_media_buffer[0x22];
      s_media_buffer[3] = s_media_buffer[0x23] | 0x80;
      int cmd = (s_media_buffer[0x23] << 8) | s_media_buffer[0x22];
      ERROR_LOG(DVDINTERFACE, "GC-AM: execute buffer, cmd=%04x", cmd);
      switch (cmd)
      {
      case 0x00:
        s_media_buffer[4] = 1;
        break;
      case 0x1:
        s_media_buffer[7] = 0x20;  // DIMM Size
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
        s_media_buffer[4] = 3;  // version
        s_media_buffer[5] = 3;
        s_media_buffer[6] = 1;  // xxx
        s_media_buffer[8] = 1;
        s_media_buffer[16] = 0xFF;
        s_media_buffer[17] = 0xFF;
        s_media_buffer[18] = 0xFF;
        s_media_buffer[19] = 0xFF;
        break;
      case 0x102:               // get error code
        s_media_buffer[4] = 1;  // 0: download incomplete (31), 1: corrupted, other error 1
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
      WriteImmediate(0x66556677, output_address, reply_to_ios);  // just a random value that works.
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
    WriteImmediate(s_error_code, output_address, reply_to_ios);
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
      s_audio_position = 0;
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
          s_audio_position = s_current_start;
          StreamADPCM::InitFilter();
          s_stream = true;
        }
      }
    }

    INFO_LOG(DVDINTERFACE, "(Audio) Stream cmd: %08x offset: %08" PRIx64 " length: %08x", command_0,
             (u64)command_1 << 2, command_2);
  }
  break;

  // Request Audio Status (Immediate). Only seems to be used by some GC games
  case 0xE2:
  {
    switch (command_0 >> 16 & 0xFF)
    {
    case 0x00:  // Returns streaming status
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:%08x/%08x "
                             "CurrentStart:%08x CurrentLength:%08x",
               s_audio_position, s_current_start + s_current_length, s_current_start,
               s_current_length);
      WriteImmediate(s_stream ? 1 : 0, output_address, reply_to_ios);
      break;
    case 0x01:  // Returns the current offset
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:%08x",
               s_audio_position);
      WriteImmediate(s_audio_position >> 2, output_address, reply_to_ios);
      break;
    case 0x02:  // Returns the start offset
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentStart:%08x",
               s_current_start);
      WriteImmediate(s_current_start >> 2, output_address, reply_to_ios);
      break;
    case 0x03:  // Returns the total length
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentLength:%08x",
               s_current_length);
      WriteImmediate(s_current_length >> 2, output_address, reply_to_ios);
      break;
    default:
      WARN_LOG(DVDINTERFACE, "(Audio): Subcommand: %02x  Request Audio status %s",
               command_0 >> 16 & 0xFF, s_stream ? "on" : "off");
      break;
    }
  }
  break;

  case DVDLowStopMotor:
    INFO_LOG(DVDINTERFACE, "DVDLowStopMotor %s %s", command_1 ? "eject" : "",
             command_2 ? "kill!" : "");

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
    if (command_0 == 0xFF014D41 && command_1 == 0x54534849 && command_2 == 0x54410200)
    {
      INFO_LOG(DVDINTERFACE, "Unlock test 1 passed");
    }
    else if (command_0 == 0xFF004456 && command_1 == 0x442D4741 && command_2 == 0x4D450300)
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
    ERROR_LOG(DVDINTERFACE, "Unknown command 0x%08x (Buffer 0x%08x, 0x%x)", command_0,
              output_address, output_length);
    PanicAlertT("Unknown DVD command %08x - fatal error", command_0);
    break;
  }

  // The command will finish executing after a delay
  // to simulate the speed of a real disc drive
  if (!command_handled_by_thread)
  {
    u64 userdata = (static_cast<u64>(reply_to_ios) << 32) + static_cast<u32>(interrupt_type);
    // This is an arbitrary delay - needs testing to determine the appropriate value
    CoreTiming::ScheduleEvent(SystemTimers::GetTicksPerSecond() / 15000, s_finish_executing_command, userdata);
  }
}

void FinishExecutingCommandCallback(u64 userdata, s64 cycles_late)
{
  bool reply_to_ios = userdata >> 32 != 0;
  DIInterruptType interrupt_type = static_cast<DIInterruptType>(userdata & 0xFFFFFFFF);
  FinishExecutingCommand(reply_to_ios, interrupt_type);
}

void FinishExecutingCommand(bool reply_to_ios, DIInterruptType interrupt_type)
{
  if (reply_to_ios)
  {
    std::shared_ptr<IWII_IPC_HLE_Device> di = WII_IPC_HLE_Interface::GetDeviceByName("/dev/di");
    if (di)
      std::static_pointer_cast<CWII_IPC_HLE_Device_di>(di)->FinishIOCtl(interrupt_type);

    // If di == nullptr, IOS was probably shut down, so the command shouldn't be completed
  }
  else if (s_DICR.TSTART)
  {
    s_DICR.TSTART = 0;
    s_DILENGTH.Length = 0;
    GenerateDIInterrupt(interrupt_type);
  }
}

// Determines from a given read request how much of the request is buffered,
// and how much is required to be read from disc.
void ScheduleReads(u64 dvd_offset, u32 dvd_length, bool decrypt, u32 output_address,
                   bool reply_to_ios)
{
  // The drive continues to read 1MB beyond the last read position when
  // idle. If a future read falls within this window, part of the read may
  // be returned from the buffer. Data can be transferred from the buffer at
  // up to 16 MiB/s.
  //
  // Metroid Prime is a good example of a game that's sensitive these disc
  // timing details; if there isn't enough latency in the right places, doors
  // open too quickly, and if there's too much latency in the wrong places,
  // the video before the save-file select screen lags.

  // We'll use this data in our callback
  s_current_read_dvd_offset = dvd_offset;
  s_current_read_dma_offset = output_address;
  s_current_read_length = dvd_length;
  s_current_read_reply_to_ios = reply_to_ios;

  u64 current_time = CoreTiming::GetTicks();

  // Where the DVD read head is (usually parked at the end of the buffer,
  // unless we've interrupted it mid-buffer-read).
  u64 head_position;

  // Compute the start (inclusive) and end (exclusize) of the buffer. If we
  // fall within its bounds, we get DMA-speed reads.
  u64 buffer_start, buffer_end;

  if (SConfig::GetInstance().bFastDiscSpeed)
  {
    // SUDTR assumes all reads are buffered
    buffer_start = std::numeric_limits<u64>::min();
    buffer_end = std::numeric_limits<u64>::max();
    head_position = 0;
  }
  else
  {
    buffer_start = s_read_buffer_end_offset > STREAMING_BUFFER_SIZE ?
                       s_read_buffer_end_offset - STREAMING_BUFFER_SIZE :
                       0;

    if (s_read_buffer_start_time == s_read_buffer_end_time)
    {
      // No buffer
      buffer_start = buffer_end = 0;
      head_position = 0;
    }
    else
    {
      DEBUG_LOG(DVDINTERFACE,
                "Buffer: now=0x%" PRIx64 " start time=0x%" PRIx64 " end time=0x%" PRIx64,
                current_time, s_read_buffer_start_time, s_read_buffer_end_time);
      if (current_time >= s_read_buffer_end_time)
      {
        // Buffer is fully read
        buffer_end = s_read_buffer_end_offset;
      }
      else
      {
        // The amount of data the buffer contains *right now*, rounded to
        // a DVD ECC block.
        u64 buffer_read_size = s_read_buffer_end_offset - s_read_buffer_start_offset;
        buffer_end = s_read_buffer_start_offset +
                     ROUND_DOWN((current_time - s_read_buffer_start_time) * buffer_read_size /
                                    (s_read_buffer_end_time - s_read_buffer_start_time),
                                DVD_ECC_BLOCK_SIZE);
      }
      head_position = buffer_end;

      // Reading before the buffer is not only unbuffered, but also destroys
      // the buffer for this and future reads.
      u64 rounded_offset = ROUND_DOWN(dvd_offset, DVD_ECC_BLOCK_SIZE);
      if (rounded_offset < buffer_start)
      {
        // Kill the buffer, but maintain the head position for seeks.
        buffer_start = buffer_end = 0;
      }
    }
  }

  DEBUG_LOG(DVDINTERFACE, "Buffer: start=0x%" PRIx64 " end=0x%" PRIx64 " avail=0x%" PRIx64,
            buffer_start, buffer_end, buffer_end - buffer_start);

  u32 offset = 0;

  DEBUG_LOG(DVDINTERFACE,
            "Schedule reads: offset=0x%" PRIx64 " length=0x%" PRIx32 " address=0x%" PRIx32,
            dvd_offset, dvd_length, output_address);

  // The DVD drive's minimum turnaround time on a command, based on hwtest.
  u64 ticks_until_completion = COMMAND_LATENCY_US * (SystemTimers::GetTicksPerSecond() / 1000000);

  int buffered_blocks = 0;
  int unbuffered_blocks = 0;

  while (dvd_length > 0)
  {
    // Where the read actually takes place on disc
    u64 rounded_offset = ROUND_DOWN(dvd_offset, DVD_ECC_BLOCK_SIZE);

    // The length of this read - "+1" so that if this read is already
    // aligned to an ECC block we'll read the entire block.
    u32 length = (u32)(ROUND_UP(dvd_offset + 1, DVD_ECC_BLOCK_SIZE) - dvd_offset);

    // The last read may be short
    if (length > dvd_length)
      length = dvd_length;

    if (rounded_offset >= buffer_start && rounded_offset < buffer_end)
    {
      // Number of ticks it takes to transfer the data from the buffer to memory.
      ticks_until_completion +=
          (static_cast<u64>(length) * SystemTimers::GetTicksPerSecond()) / BUFFER_TRANSFER_RATE;
      buffered_blocks++;
    }
    else
    {
      // In practice we'll only ever seek if this is the first time
      // through this loop.
      if (rounded_offset != head_position)
      {
        // Unbuffered seek+read
        ticks_until_completion += CalculateSeekTime(head_position, rounded_offset);
        DEBUG_LOG(DVDINTERFACE, "Seek+read 0x%" PRIx32 " bytes @ 0x%" PRIx64 " ticks=%" PRId64,
                  length, rounded_offset, ticks_until_completion);
      }
      else
      {
        // Unbuffered read
        ticks_until_completion += CalculateRawDiscReadTime(rounded_offset, DVD_ECC_BLOCK_SIZE);
      }

      unbuffered_blocks++;
      head_position = rounded_offset + DVD_ECC_BLOCK_SIZE;
    }

    // Schedule this read to complete at the appropriate time
    CoreTiming::ScheduleEvent(ticks_until_completion, s_finish_read,
                              (static_cast<u64>(offset) << 32) | length);

    // Advance the read window
    offset += length;
    dvd_offset += length;
    dvd_length -= length;
  }

  // Update the buffer based on this read. Based on experimental testing, we
  // will only reuse the old buffer while reading forward. Note that the
  // buffer start we calculate here is not the actual start of the buffer -
  // it is just the start of the portion we need to read.
  u64 last_block = ROUND_UP(dvd_offset, DVD_ECC_BLOCK_SIZE);
  if (last_block == buffer_start + DVD_ECC_BLOCK_SIZE && buffer_start != buffer_end)
  {
    // Special case: reading less than one block at the start of the
    // buffer won't change the buffer state
  }
  else
  {
    if (last_block > buffer_end)
      // Full buffer read
      s_read_buffer_start_offset = last_block;
    else
      // Partial buffer read
      s_read_buffer_start_offset = buffer_end;

    s_read_buffer_end_offset = last_block + STREAMING_BUFFER_SIZE - DVD_ECC_BLOCK_SIZE;
    // Assume the buffer starts reading from the end of the last operation
    s_read_buffer_start_time = current_time + ticks_until_completion;
    s_read_buffer_end_time =
        current_time + ticks_until_completion +
        CalculateRawDiscReadTime(s_read_buffer_start_offset,
                                 (u32)(s_read_buffer_end_offset - s_read_buffer_start_offset));
  }

  DEBUG_LOG(DVDINTERFACE, "Schedule reads: ecc blocks unbuffered=%d buffered=%d, ticks=%" PRId64
                          ", time=%" PRId64 "us",
            unbuffered_blocks, buffered_blocks, ticks_until_completion,
            ticks_until_completion * 1000000 / SystemTimers::GetTicksPerSecond());
}

// We can approximate the relationship between a byte offset on disc and its
// radial distance from the center by using an approximation for the length of
// a rolled material, which is the area of the material divided by the pitch
// (ie: assume that you can squish and deform the area of the disc into a
// rectangle as thick as the track pitch).
//
// In practice this yields good-enough numbers as a more exact formula
// involving the integral over a polar equation (too complex here to describe)
// or the approximation of a DVD as a set of concentric circles (which is a
// better approximation, but makes futher derivations more complicated than
// they need to be).
//
// From the area approximation, we end up with this formula:
//
// L = pi*(r.outer^2-r.inner^2)/pitch
//
// Where:
//   L = the data track's physical length
//   r.{inner,outer} = the inner/outer radii (24mm and 58mm)
//   pitch = the track pitch (.74um)
//
// We can then use this equation to compute the radius for a given sector in
// the disc by mapping it along the length to a linear position and inverting
// the equation and solving for r.outer (using the DVD's r.inner and pitch)
// given that linear position:
//
// r.outer = sqrt(P * pitch / pi + r.inner^2)
//
// Where:
//   P = the offset's linear position, as offset/density
//
// The data density of the disc is just the number of bytes addressable on a
// DVD, divided by the spiral length holding that data. offset/density yields
// the linear position for a given offset.
//
// When we put it all together and simplify, we can compute the radius for a
// given byte offset as a drastically simplified:
//
// r = sqrt(offset/total_bytes*(r.outer^2-r.inner^2) + r.inner^2)
double CalculatePhysicalDiscPosition(u64 offset)
{
  // Assumption: the layout on the second disc layer is opposite of the first, ie
  // layer 2 starts where layer 1 ends and goes backwards.
  if (offset > WII_DISC_LAYER_SIZE)
  {
    offset = WII_DISC_LAYER_SIZE * 2 - offset;
  }

  // The track pitch here is 0.74um, but it cancels out and we don't need it

  // Note that because Wii and GC discs have identical data densities we
  // just use the Wii numbers and this works for both
  return std::sqrt(
      (double)offset / WII_DISC_LAYER_SIZE *
          (WII_DVD_OUTER_RADIUS * WII_DVD_OUTER_RADIUS - DVD_INNER_RADIUS * DVD_INNER_RADIUS) +
      DVD_INNER_RADIUS * DVD_INNER_RADIUS);
}

// Returns the number of ticks to move the read head from one offset to
// another. Based on hardware testing, this appears to be a function of the
// linear distance between the radius of the first and second positions on the
// disc, though the head speed varies depending on the length of the seek.
u64 CalculateSeekTime(u64 offset_from, u64 offset_to)
{
  double position_from = CalculatePhysicalDiscPosition(offset_from);
  double position_to = CalculatePhysicalDiscPosition(offset_to);

  // Seek time is roughly linear based on head distance travelled
  double distance = fabs(position_from - position_to);

  double time;
  if (distance < SHORT_SEEK_DISTANCE)
    time = (SHORT_SEEK_CONSTANT + distance * SHORT_SEEK_VELOCITY_INVERSE) *
           SystemTimers::GetTicksPerSecond();
  else
    time = (LONG_SEEK_CONSTANT + distance * LONG_SEEK_VELOCITY_INVERSE) *
           SystemTimers::GetTicksPerSecond();

  return static_cast<u64>(time);
}

// Returns the number of ticks it takes to read an amount of data from a disc,
// ignoring factors such as seek times. This is the streaming rate of the
// drive and varies between ~3-8MiB/s for Wii discs. Note that there is technically
// a DMA delay on top of this, but we model that as part of this read time.
u64 CalculateRawDiscReadTime(u64 offset, u32 length)
{
  // The Wii/GC have a CAV drive and the data has a constant pit length
  // regardless of location on disc. This means we can linearly interpolate
  // speed from the inner to outer radius. This matches a hardware test.
  // We're just picking a point halfway into the read as our benchmark for
  // read speed as speeds don't change materially in this small window.
  double physical_offset = CalculatePhysicalDiscPosition(offset + length / 2);

  double speed;
  if (s_inserted_volume->GetVolumeType() == DiscIO::Platform::WII_DISC)
  {
    speed = (physical_offset - DVD_INNER_RADIUS) / (WII_DVD_OUTER_RADIUS - DVD_INNER_RADIUS) *
                (WII_DISC_OUTER_READ_SPEED - WII_DISC_INNER_READ_SPEED) +
            WII_DISC_INNER_READ_SPEED;
  }
  else
  {
    speed = (physical_offset - DVD_INNER_RADIUS) / (GC_DVD_OUTER_RADIUS - DVD_INNER_RADIUS) *
                (GC_DISC_OUTER_READ_SPEED - GC_DISC_INNER_READ_SPEED) +
            GC_DISC_INNER_READ_SPEED;
  }

  DEBUG_LOG(DVDINTERFACE, "Read 0x%" PRIx32 " @ 0x%" PRIx64 " @%lfmm: %lf us, %lf MiB/s", length,
            offset, physical_offset * 1000, length / speed * 1000 * 1000, speed / 1024 / 1024);

  // (ticks/second) / (bytes/second) * bytes = ticks
  double ticks = (double)SystemTimers::GetTicksPerSecond() * length / speed;

  return static_cast<u64>(ticks);
}

}  // namespace
