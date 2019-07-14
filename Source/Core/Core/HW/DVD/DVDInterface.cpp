// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DVD/DVDInterface.h"

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "AudioCommon/AudioCommon.h"

#include "Common/Align.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Logging/Log.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DVD/DVDMath.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/StreamADPCM.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/DI/DI.h"
#include "Core/IOS/IOS.h"
#include "Core/Movie.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWii.h"

#include "VideoCommon/OnScreenDisplay.h"

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
constexpr u64 BUFFER_TRANSFER_RATE = 32 * 1024 * 1024;

namespace DVDInterface
{
// "low" error codes
constexpr u32 ERROR_READY = 0x0000000;          // Ready.
constexpr u32 ERROR_COVER_L = 0x01000000;       // Cover is opened.
constexpr u32 ERROR_CHANGE_DISK = 0x02000000;   // Disk change.
constexpr u32 ERROR_NO_DISK = 0x03000000;       // No disk.
constexpr u32 ERROR_MOTOR_STOP_L = 0x04000000;  // Motor stop.
constexpr u32 ERROR_NO_DISKID_L = 0x05000000;   // Disk ID not read.

// "high" error codes
constexpr u32 ERROR_NONE = 0x000000;          // No error.
constexpr u32 ERROR_MOTOR_STOP_H = 0x020400;  // Motor stopped.
constexpr u32 ERROR_NO_DISKID_H = 0x020401;   // Disk ID not read.
constexpr u32 ERROR_COVER_H = 0x023a00;       // Medium not present / Cover opened.
constexpr u32 ERROR_SEEK_NDONE = 0x030200;    // No seek complete.
constexpr u32 ERROR_READ = 0x031100;          // Unrecovered read error.
constexpr u32 ERROR_PROTOCOL = 0x040800;      // Transfer protocol error.
constexpr u32 ERROR_INV_CMD = 0x052000;       // Invalid command operation code.
constexpr u32 ERROR_AUDIO_BUF = 0x052001;     // Audio Buffer not set.
constexpr u32 ERROR_BLOCK_OOB = 0x052100;     // Logical block address out of bounds.
constexpr u32 ERROR_INV_FIELD = 0x052400;     // Invalid field in command packet.
constexpr u32 ERROR_INV_AUDIO = 0x052401;     // Invalid audio command.
constexpr u32 ERROR_INV_PERIOD = 0x052402;    // Configuration out of permitted period.
constexpr u32 ERROR_END_USR_AREA = 0x056300;  // End of user area encountered on this track.
constexpr u32 ERROR_MEDIUM = 0x062800;        // Medium may have changed.
constexpr u32 ERROR_MEDIUM_REQ = 0x0b5a01;    // Operator medium removal request.

// internal hardware addresses
constexpr u32 DI_STATUS_REGISTER = 0x00;
constexpr u32 DI_COVER_REGISTER = 0x04;
constexpr u32 DI_COMMAND_0 = 0x08;
constexpr u32 DI_COMMAND_1 = 0x0C;
constexpr u32 DI_COMMAND_2 = 0x10;
constexpr u32 DI_DMA_ADDRESS_REGISTER = 0x14;
constexpr u32 DI_DMA_LENGTH_REGISTER = 0x18;
constexpr u32 DI_DMA_CONTROL_REGISTER = 0x1C;
constexpr u32 DI_IMMEDIATE_DATA_BUFFER = 0x20;
constexpr u32 DI_CONFIG_REGISTER = 0x24;

// debug commands which may be ORd
constexpr u32 STOP_DRIVE = 0;
constexpr u32 START_DRIVE = 0x100;
constexpr u32 ACCEPT_COPY = 0x4000;
constexpr u32 DISC_CHECK = 0x8000;

// DI Status Register
union UDISR
{
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
union UDICVR
{
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
union UDILENGTH
{
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
union UDICR
{
  u32 Hex;
  struct
  {
    u32 TSTART : 1;  // w:1 start   r:0 ready
    u32 DMA : 1;     // 1: DMA Mode    0: Immediate Mode (can only do Access Register Command)
    u32 RW : 1;      // 0: Read Command (DVD to Memory)  1: Write Command (Memory to DVD)
    u32 : 29;
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
    u32 : 24;
  };
  UDICFG() { Hex = 0; }
  UDICFG(u32 _hex) { Hex = _hex; }
};

// STATE_TO_SAVE

// Hardware registers
static UDISR s_DISR;
static UDICVR s_DICVR;
static UDICMDBUF s_DICMDBUF[3];
static UDIMAR s_DIMAR;
static UDILENGTH s_DILENGTH;
static UDICR s_DICR;
static UDIIMMBUF s_DIIMMBUF;
static UDICFG s_DICFG;

static StreamADPCM::ADPCMDecoder s_adpcm_decoder;

// DTK
static bool s_stream = false;
static bool s_stop_at_track_end = false;
static u64 s_audio_position;
static u64 s_current_start;
static u32 s_current_length;
static u64 s_next_start;
static u32 s_next_length;
static u32 s_pending_samples;

// Disc drive state
static u32 s_error_code = 0;
static DiscIO::Partition s_current_partition;

// Disc drive timing
static u64 s_read_buffer_start_time;
static u64 s_read_buffer_end_time;
static u64 s_read_buffer_start_offset;
static u64 s_read_buffer_end_offset;

// Disc changing
static std::string s_disc_path_to_insert;
static std::vector<std::string> s_auto_disc_change_paths;
static size_t s_auto_disc_change_index;

// Events
static CoreTiming::EventType* s_finish_executing_command;
static CoreTiming::EventType* s_auto_change_disc;
static CoreTiming::EventType* s_eject_disc;
static CoreTiming::EventType* s_insert_disc;

static void AutoChangeDiscCallback(u64 userdata, s64 cyclesLate);
static void EjectDiscCallback(u64 userdata, s64 cyclesLate);
static void InsertDiscCallback(u64 userdata, s64 cyclesLate);
static void FinishExecutingCommandCallback(u64 userdata, s64 cycles_late);

void SetLidOpen();

void UpdateInterrupts();
void GenerateDIInterrupt(DIInterruptType _DVDInterrupt);

void WriteImmediate(u32 value, u32 output_address, bool reply_to_ios);
bool ExecuteReadCommand(u64 dvd_offset, u32 output_address, u32 dvd_length, u32 output_length,
                        const DiscIO::Partition& partition, ReplyType reply_type,
                        DIInterruptType* interrupt_type);

u64 PackFinishExecutingCommandUserdata(ReplyType reply_type, DIInterruptType interrupt_type);

void ScheduleReads(u64 offset, u32 length, const DiscIO::Partition& partition, u32 output_address,
                   ReplyType reply_type);

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

  p.Do(s_stream);
  p.Do(s_stop_at_track_end);
  p.Do(s_audio_position);
  p.Do(s_current_start);
  p.Do(s_current_length);
  p.Do(s_next_start);
  p.Do(s_next_length);
  p.Do(s_pending_samples);

  p.Do(s_error_code);
  p.Do(s_current_partition);

  p.Do(s_read_buffer_start_time);
  p.Do(s_read_buffer_end_time);
  p.Do(s_read_buffer_start_offset);
  p.Do(s_read_buffer_end_offset);

  p.Do(s_disc_path_to_insert);

  DVDThread::DoState(p);

  s_adpcm_decoder.DoState(p);
}

static size_t ProcessDTKSamples(std::vector<s16>* temp_pcm, const std::vector<u8>& audio_data)
{
  size_t samples_processed = 0;
  size_t bytes_processed = 0;
  while (samples_processed < temp_pcm->size() / 2 && bytes_processed < audio_data.size())
  {
    s_adpcm_decoder.DecodeBlock(&(*temp_pcm)[samples_processed * 2], &audio_data[bytes_processed]);
    for (size_t i = 0; i < StreamADPCM::SAMPLES_PER_BLOCK * 2; ++i)
    {
      // TODO: Fix the mixer so it can accept non-byte-swapped samples.
      s16* sample = &(*temp_pcm)[samples_processed * 2 + i];
      *sample = Common::swap16(*sample);
    }
    samples_processed += StreamADPCM::SAMPLES_PER_BLOCK;
    bytes_processed += StreamADPCM::ONE_BLOCK_SIZE;
  }
  return samples_processed;
}

static u32 AdvanceDTK(u32 maximum_samples, u32* samples_to_process)
{
  u32 bytes_to_process = 0;
  *samples_to_process = 0;
  while (*samples_to_process < maximum_samples)
  {
    if (s_audio_position >= s_current_start + s_current_length)
    {
      DEBUG_LOG(DVDINTERFACE,
                "AdvanceDTK: NextStart=%08" PRIx64 ", NextLength=%08x, "
                "CurrentStart=%08" PRIx64 ", CurrentLength=%08x, AudioPos=%08" PRIx64,
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

      s_adpcm_decoder.ResetFilter();
    }

    s_audio_position += StreamADPCM::ONE_BLOCK_SIZE;
    bytes_to_process += StreamADPCM::ONE_BLOCK_SIZE;
    *samples_to_process += StreamADPCM::SAMPLES_PER_BLOCK;
  }

  return bytes_to_process;
}

static void DTKStreamingCallback(const std::vector<u8>& audio_data, s64 cycles_late)
{
  // Send audio to the mixer.
  std::vector<s16> temp_pcm(s_pending_samples * 2, 0);
  ProcessDTKSamples(&temp_pcm, audio_data);
  g_sound_stream->GetMixer()->PushStreamingSamples(temp_pcm.data(), s_pending_samples);

  // Determine which audio data to read next.
  static const int MAXIMUM_SAMPLES = 48000 / 2000 * 7;  // 3.5ms of 48kHz samples
  u64 read_offset = 0;
  u32 read_length = 0;
  if (s_stream && AudioInterface::IsPlaying())
  {
    read_offset = s_audio_position;
    read_length = AdvanceDTK(MAXIMUM_SAMPLES, &s_pending_samples);
  }
  else
  {
    read_length = 0;
    s_pending_samples = MAXIMUM_SAMPLES;
  }

  // Read the next chunk of audio data asynchronously.
  s64 ticks_to_dtk = SystemTimers::GetTicksPerSecond() * s64(s_pending_samples) / 48000;
  ticks_to_dtk -= cycles_late;
  if (read_length > 0)
  {
    DVDThread::StartRead(read_offset, read_length, DiscIO::PARTITION_NONE, ReplyType::DTK,
                         ticks_to_dtk);
  }
  else
  {
    // There's nothing to read, so using DVDThread is unnecessary.
    u64 userdata = PackFinishExecutingCommandUserdata(ReplyType::DTK, DIInterruptType::INT_TCINT);
    CoreTiming::ScheduleEvent(ticks_to_dtk, s_finish_executing_command, userdata);
  }
}

void Init()
{
  ASSERT(!IsDiscInside());

  DVDThread::Start();

  Reset();
  s_DICVR.Hex = 1;  // Disc Channel relies on cover being open when no disc is inserted

  s_auto_change_disc = CoreTiming::RegisterEvent("AutoChangeDisc", AutoChangeDiscCallback);
  s_eject_disc = CoreTiming::RegisterEvent("EjectDisc", EjectDiscCallback);
  s_insert_disc = CoreTiming::RegisterEvent("InsertDisc", InsertDiscCallback);

  s_finish_executing_command =
      CoreTiming::RegisterEvent("FinishExecutingCommand", FinishExecutingCommandCallback);

  u64 userdata = PackFinishExecutingCommandUserdata(ReplyType::DTK, DIInterruptType::INT_TCINT);
  CoreTiming::ScheduleEvent(0, s_finish_executing_command, userdata);
}

// This doesn't reset any inserted disc or the cover state.
void Reset()
{
  s_DISR.Hex = 0;
  s_DICMDBUF[0].Hex = 0;
  s_DICMDBUF[1].Hex = 0;
  s_DICMDBUF[2].Hex = 0;
  s_DIMAR.Hex = 0;
  s_DILENGTH.Hex = 0;
  s_DICR.Hex = 0;
  s_DIIMMBUF.Hex = 0;
  s_DICFG.Hex = 0;
  s_DICFG.CONFIG = 1;  // Disable bootrom descrambler

  s_stream = false;
  s_stop_at_track_end = false;
  s_audio_position = 0;
  s_next_start = 0;
  s_next_length = 0;
  s_current_start = 0;
  s_current_length = 0;
  s_pending_samples = 0;

  s_error_code = 0;

  // The buffer is empty at start
  s_read_buffer_start_offset = 0;
  s_read_buffer_end_offset = 0;
  s_read_buffer_start_time = 0;
  s_read_buffer_end_time = 0;

  s_disc_path_to_insert.clear();
}

void Shutdown()
{
  DVDThread::Stop();
}

void SetDisc(std::unique_ptr<DiscIO::VolumeDisc> disc,
             std::optional<std::vector<std::string>> auto_disc_change_paths = {})
{
  if (disc)
    s_current_partition = disc->GetGamePartition();

  if (auto_disc_change_paths)
  {
    ASSERT_MSG(DISCIO, (*auto_disc_change_paths).size() != 1,
               "Cannot automatically change between one disc");

    s_auto_disc_change_paths = *auto_disc_change_paths;
    s_auto_disc_change_index = 0;
  }

  DVDThread::SetDisc(std::move(disc));
  SetLidOpen();
}

bool IsDiscInside()
{
  return DVDThread::HasDisc();
}

static void AutoChangeDiscCallback(u64 userdata, s64 cyclesLate)
{
  AutoChangeDisc();
}

static void EjectDiscCallback(u64 userdata, s64 cyclesLate)
{
  SetDisc(nullptr, {});
}

static void InsertDiscCallback(u64 userdata, s64 cyclesLate)
{
  std::unique_ptr<DiscIO::VolumeDisc> new_disc = DiscIO::CreateDisc(s_disc_path_to_insert);

  if (new_disc)
    SetDisc(std::move(new_disc), {});
  else
    PanicAlertT("The disc that was about to be inserted couldn't be found.");

  s_disc_path_to_insert.clear();
}

// Must only be called on the CPU thread
void EjectDisc()
{
  CoreTiming::ScheduleEvent(0, s_eject_disc);
}

// Must only be called on the CPU thread
void ChangeDisc(const std::vector<std::string>& paths)
{
  ASSERT_MSG(DISCIO, !paths.empty(), "Trying to insert an empty list of discs");

  if (paths.size() > 1)
  {
    s_auto_disc_change_paths = paths;
    s_auto_disc_change_index = 0;
  }

  ChangeDisc(paths[0]);
}

// Must only be called on the CPU thread
void ChangeDisc(const std::string& new_path)
{
  if (!s_disc_path_to_insert.empty())
  {
    PanicAlertT("A disc is already about to be inserted.");
    return;
  }

  EjectDisc();

  s_disc_path_to_insert = new_path;
  CoreTiming::ScheduleEvent(SystemTimers::GetTicksPerSecond(), s_insert_disc);
  Movie::SignalDiscChange(new_path);

  for (size_t i = 0; i < s_auto_disc_change_paths.size(); ++i)
  {
    if (s_auto_disc_change_paths[i] == new_path)
    {
      s_auto_disc_change_index = i;
      return;
    }
  }

  s_auto_disc_change_paths.clear();
}

// Must only be called on the CPU thread
bool AutoChangeDisc()
{
  if (s_auto_disc_change_paths.empty())
    return false;

  s_auto_disc_change_index = (s_auto_disc_change_index + 1) % s_auto_disc_change_paths.size();
  ChangeDisc(s_auto_disc_change_paths[s_auto_disc_change_index]);
  return true;
}

void SetLidOpen()
{
  u32 old_value = s_DICVR.CVR;
  s_DICVR.CVR = IsDiscInside() ? 0 : 1;
  if (s_DICVR.CVR != old_value)
    GenerateDIInterrupt(INT_CVRINT);
}

bool UpdateRunningGameMetadata(std::optional<u64> title_id)
{
  if (!DVDThread::HasDisc())
    return false;

  return DVDThread::UpdateRunningGameMetadata(s_current_partition, title_id);
}

void ChangePartition(const DiscIO::Partition& partition)
{
  s_current_partition = partition;
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
                     DEBUG_ASSERT(0);
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
  const bool set_mask = (s_DISR.DEINT & s_DISR.DEINITMASK) || (s_DISR.TCINT & s_DISR.TCINTMASK) ||
                        (s_DISR.BRKINT & s_DISR.BRKINTMASK) ||
                        (s_DICVR.CVRINT & s_DICVR.CVRINTMASK);

  ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DI, set_mask);

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
bool ExecuteReadCommand(u64 dvd_offset, u32 output_address, u32 dvd_length, u32 output_length,
                        const DiscIO::Partition& partition, ReplyType reply_type,
                        DIInterruptType* interrupt_type)
{
  if (!IsDiscInside())
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

  if (dvd_length > output_length)
  {
    WARN_LOG(DVDINTERFACE, "Detected an attempt to read more data from the DVD "
                           "than what fits inside the out buffer. Clamping.");
    dvd_length = output_length;
  }

  ScheduleReads(dvd_offset, dvd_length, partition, output_address, reply_type);
  return true;
}

// When the command has finished executing, callback_event_type
// will be called using CoreTiming::ScheduleEvent,
// with the userdata set to the interrupt type.
void ExecuteCommand(u32 command_0, u32 command_1, u32 command_2, u32 output_address,
                    u32 output_length, bool reply_to_ios)
{
  ReplyType reply_type = reply_to_ios ? ReplyType::IOS : ReplyType::Interrupt;
  DIInterruptType interrupt_type = INT_TCINT;
  bool command_handled_by_thread = false;

  // DVDLowRequestError needs access to the error code set by the previous command
  if (command_0 >> 24 != DVDLowRequestError)
    s_error_code = 0;

  switch (command_0 >> 24)
  {
  // Seems to be used by both GC and Wii
  case DVDLowInquiry:
    // (shuffle2) Taken from my Wii
    Memory::Write_U32(0x00000002, output_address);
    Memory::Write_U32(0x20060526, output_address + 4);
    // This was in the oubuf even though this cmd is only supposed to reply with 64bits
    // However, this and other tests strongly suggest that the buffer is static, and it's never -
    // or rarely cleared.
    Memory::Write_U32(0x41000000, output_address + 8);

    INFO_LOG(DVDINTERFACE, "DVDLowInquiry (Buffer 0x%08x, 0x%x)", output_address, output_length);
    break;

  // Only seems to be used from WII_IPC, not through direct access
  case DVDLowReadDiskID:
    INFO_LOG(DVDINTERFACE, "DVDLowReadDiskID");
    command_handled_by_thread =
        ExecuteReadCommand(0, output_address, 0x20, output_length, DiscIO::PARTITION_NONE,
                           reply_type, &interrupt_type);
    break;

  // Only used from WII_IPC. This is the only read command that decrypts data
  case DVDLowRead:
    INFO_LOG(DVDINTERFACE, "DVDLowRead: DVDAddr: 0x%09" PRIx64 ", Size: 0x%x", (u64)command_2 << 2,
             command_1);
    command_handled_by_thread =
        ExecuteReadCommand((u64)command_2 << 2, output_address, command_1, output_length,
                           s_current_partition, reply_type, &interrupt_type);
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
    DEBUG_LOG(DVDINTERFACE, "DVDLowGetCoverReg 0x%08x", s_DICVR.Hex);
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
    DEBUG_LOG(DVDINTERFACE, "DVDLowClearCoverInterrupt");
    s_DICVR.CVRINT = 0;
    break;

  // Probably only used by Wii
  case DVDLowGetCoverStatus:
    WriteImmediate(IsDiscInside() ? 2 : 1, output_address, reply_to_ios);
    INFO_LOG(DVDINTERFACE, "DVDLowGetCoverStatus: Disc %sInserted", IsDiscInside() ? "" : "Not ");
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
          ExecuteReadCommand((u64)command_2 << 2, output_address, command_1, output_length,
                             DiscIO::PARTITION_NONE, reply_type, &interrupt_type);
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

      INFO_LOG(DVDINTERFACE,
               "Read: DVDOffset=%08" PRIx64
               ", DMABuffer = %08x, SrcLength = %08x, DMALength = %08x",
               iDVDOffset, output_address, command_2, output_length);

      command_handled_by_thread =
          ExecuteReadCommand(iDVDOffset, output_address, command_2, output_length,
                             DiscIO::PARTITION_NONE, reply_type, &interrupt_type);
    }
    break;

    case 0x40:  // Read DiscID
      INFO_LOG(DVDINTERFACE, "Read DiscID %08x", Memory::Read_U32(output_address));
      command_handled_by_thread =
          ExecuteReadCommand(0, output_address, 0x20, output_length, DiscIO::PARTITION_NONE,
                             reply_type, &interrupt_type);
      break;

    default:
      ERROR_LOG(DVDINTERFACE, "Unknown read subcommand: %08x", command_0);
      break;
    }
    break;

  // Seems to be used by both GC and Wii
  case DVDLowSeek:
    // Currently unimplemented
    INFO_LOG(DVDINTERFACE, "Seek: offset=%09" PRIx64 " (ignoring)", (u64)command_1 << 2);
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
        s_next_start = static_cast<u64>(command_1) << 2;
        s_next_length = command_2;
        if (!s_stream)
        {
          s_current_start = s_next_start;
          s_current_length = s_next_length;
          s_audio_position = s_current_start;
          s_adpcm_decoder.ResetFilter();
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
      INFO_LOG(DVDINTERFACE,
               "(Audio): Stream Status: Request Audio status "
               "AudioPos:%08" PRIx64 "/%08" PRIx64 " "
               "CurrentStart:%08" PRIx64 " CurrentLength:%08x",
               s_audio_position, s_current_start + s_current_length, s_current_start,
               s_current_length);
      WriteImmediate(s_stream ? 1 : 0, output_address, reply_to_ios);
      break;
    case 0x01:  // Returns the current offset
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:%08" PRIx64,
               s_audio_position);
      WriteImmediate(static_cast<u32>((s_audio_position & 0xffffffffffff8000ull) >> 2),
                     output_address, reply_to_ios);
      break;
    case 0x02:  // Returns the start offset
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentStart:%08" PRIx64,
               s_current_start);
      WriteImmediate(static_cast<u32>(s_current_start >> 2), output_address, reply_to_ios);
      break;
    case 0x03:  // Returns the total length
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentLength:%08x",
               s_current_length);
      WriteImmediate(static_cast<u32>(s_current_length >> 2), output_address, reply_to_ios);
      break;
    default:
      INFO_LOG(DVDINTERFACE, "(Audio): Subcommand: %02x  Request Audio status %s",
               command_0 >> 16 & 0xFF, s_stream ? "on" : "off");
      break;
    }
  }
  break;

  case DVDLowStopMotor:
  {
    INFO_LOG(DVDINTERFACE, "DVDLowStopMotor %s %s", command_1 ? "eject" : "",
             command_2 ? "kill!" : "");

    const bool force_eject = command_1 && !command_2;

    if (Config::Get(Config::MAIN_AUTO_DISC_CHANGE) && !Movie::IsPlayingInput() &&
        DVDThread::IsInsertedDiscRunning() && !s_auto_disc_change_paths.empty())
    {
      CoreTiming::ScheduleEvent(force_eject ? 0 : SystemTimers::GetTicksPerSecond() / 2,
                                s_auto_change_disc);
      OSD::AddMessage("Changing discs automatically...", OSD::Duration::NORMAL);
    }
    else if (force_eject)
    {
      EjectDiscCallback(0, 0);
    }
    break;
  }

  // DVD Audio Enable/Disable (Immediate). GC uses this, and apparently Wii also does...?
  case DVDLowAudioBufferConfig:
    // The IPL uses this command to enable or disable DTK audio depending on the value of byte 0x8
    // in the disc header. See http://www.crazynation.org/GC/GC_DD_TECH/GCTech.htm for more info.
    // The link is dead, but you can access the page using the Wayback Machine at archive.org.

    // TODO: Dolphin doesn't prevent the game from using DTK when the IPL doesn't enable it.
    // Should we be failing with an error code when the game tries to use the 0xE1 command?
    // (Not that this should matter normally, since games that use DTK set the header byte to 1)

    if ((command_0 >> 16) & 0xFF)
      INFO_LOG(DVDINTERFACE, "DTK enabled");
    else
      INFO_LOG(DVDINTERFACE, "DTK disabled");
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

  if (!command_handled_by_thread)
  {
    // TODO: Needs testing to determine if COMMAND_LATENCY_US is accurate for this
    CoreTiming::ScheduleEvent(COMMAND_LATENCY_US * (SystemTimers::GetTicksPerSecond() / 1000000),
                              s_finish_executing_command,
                              PackFinishExecutingCommandUserdata(reply_type, interrupt_type));
  }
}

u64 PackFinishExecutingCommandUserdata(ReplyType reply_type, DIInterruptType interrupt_type)
{
  return (static_cast<u64>(reply_type) << 32) + static_cast<u32>(interrupt_type);
}

void FinishExecutingCommandCallback(u64 userdata, s64 cycles_late)
{
  ReplyType reply_type = static_cast<ReplyType>(userdata >> 32);
  DIInterruptType interrupt_type = static_cast<DIInterruptType>(userdata & 0xFFFFFFFF);
  FinishExecutingCommand(reply_type, interrupt_type, cycles_late);
}

void FinishExecutingCommand(ReplyType reply_type, DIInterruptType interrupt_type, s64 cycles_late,
                            const std::vector<u8>& data)
{
  switch (reply_type)
  {
  case ReplyType::NoReply:
  {
    break;
  }

  case ReplyType::Interrupt:
  {
    if (s_DICR.TSTART)
    {
      s_DICR.TSTART = 0;
      s_DILENGTH.Length = 0;
      GenerateDIInterrupt(interrupt_type);
    }
    break;
  }

  case ReplyType::IOS:
  {
    auto di = IOS::HLE::GetIOS()->GetDeviceByName("/dev/di");
    if (di)
      std::static_pointer_cast<IOS::HLE::Device::DI>(di)->FinishIOCtl(interrupt_type);
    break;
  }

  case ReplyType::DTK:
  {
    DTKStreamingCallback(data, cycles_late);
    break;
  }
  }
}

// Determines from a given read request how much of the request is buffered,
// and how much is required to be read from disc.
void ScheduleReads(u64 offset, u32 length, const DiscIO::Partition& partition, u32 output_address,
                   ReplyType reply_type)
{
  // The drive continues to read 1 MiB beyond the last read position when idle.
  // If a future read falls within this window, part of the read may be returned
  // from the buffer. Data can be transferred from the buffer at up to 32 MiB/s.

  // Metroid Prime is a good example of a game that's sensitive to disc timing
  // details; if there isn't enough latency in the right places, doors can open
  // faster than on real hardware, and if there's too much latency in the wrong
  // places, the video before the save-file select screen lags.

  const u64 current_time = CoreTiming::GetTicks();
  const u32 ticks_per_second = SystemTimers::GetTicksPerSecond();
  const bool wii_disc = DVDThread::GetDiscType() == DiscIO::Platform::WiiDisc;

  // Where the DVD read head is (usually parked at the end of the buffer,
  // unless we've interrupted it mid-buffer-read).
  u64 head_position;

  // Compute the start (inclusive) and end (exclusive) of the buffer.
  // If we fall within its bounds, we get DMA-speed reads.
  u64 buffer_start, buffer_end;

  // The variable offset uses the same addressing as games do.
  // The variable dvd_offset tracks the actual offset on the DVD
  // that the disc drive starts reading at, which differs in two ways:
  // It's rounded to a whole ECC block and never uses Wii partition addressing.
  u64 dvd_offset = DVDThread::PartitionOffsetToRawOffset(offset, partition);
  dvd_offset = Common::AlignDown(dvd_offset, DVD_ECC_BLOCK_SIZE);

  if (SConfig::GetInstance().bFastDiscSpeed)
  {
    // The SUDTR setting makes us act as if all reads are buffered
    buffer_start = std::numeric_limits<u64>::min();
    buffer_end = std::numeric_limits<u64>::max();
    head_position = 0;
  }
  else
  {
    if (s_read_buffer_start_time == s_read_buffer_end_time)
    {
      // No buffer
      buffer_start = buffer_end = head_position = 0;
    }
    else
    {
      buffer_start = s_read_buffer_end_offset > STREAMING_BUFFER_SIZE ?
                         s_read_buffer_end_offset - STREAMING_BUFFER_SIZE :
                         0;

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
        // The amount of data the buffer contains *right now*, rounded to a DVD ECC block.
        buffer_end = s_read_buffer_start_offset +
                     Common::AlignDown((current_time - s_read_buffer_start_time) *
                                           (s_read_buffer_end_offset - s_read_buffer_start_offset) /
                                           (s_read_buffer_end_time - s_read_buffer_start_time),
                                       DVD_ECC_BLOCK_SIZE);
      }
      head_position = buffer_end;

      // Reading before the buffer is not only unbuffered,
      // but also destroys the old buffer for future reads.
      if (dvd_offset < buffer_start)
      {
        // Kill the buffer, but maintain the head position for seeks.
        buffer_start = buffer_end = 0;
      }
    }
  }

  DEBUG_LOG(DVDINTERFACE, "Buffer: start=0x%" PRIx64 " end=0x%" PRIx64 " avail=0x%" PRIx64,
            buffer_start, buffer_end, buffer_end - buffer_start);

  DEBUG_LOG(DVDINTERFACE,
            "Schedule reads: offset=0x%" PRIx64 " length=0x%" PRIx32 " address=0x%" PRIx32, offset,
            length, output_address);

  // The DVD drive's minimum turnaround time on a command, based on a hardware test.
  s64 ticks_until_completion = COMMAND_LATENCY_US * (SystemTimers::GetTicksPerSecond() / 1000000);

  u32 buffered_blocks = 0;
  u32 unbuffered_blocks = 0;

  const u32 bytes_per_chunk =
      partition != DiscIO::PARTITION_NONE && DVDThread::IsEncryptedAndHashed() ?
          DiscIO::VolumeWii::BLOCK_DATA_SIZE :
          DVD_ECC_BLOCK_SIZE;

  do
  {
    // The length of this read - "+1" so that if this read is already
    // aligned to a block we'll read the entire block.
    u32 chunk_length = static_cast<u32>(Common::AlignUp(offset + 1, bytes_per_chunk) - offset);

    // The last chunk may be short
    chunk_length = std::min(chunk_length, length);

    // TODO: If the emulated software requests 0 bytes of data, should we seek or not?

    if (dvd_offset >= buffer_start && dvd_offset < buffer_end)
    {
      // Number of ticks it takes to transfer the data from the buffer to memory.
      // TODO: This calculation is slightly wrong when decrypt is true - it uses the size of
      // the copy from IOS to PPC but is supposed to model the copy from the disc drive to IOS.
      ticks_until_completion +=
          static_cast<u64>(chunk_length) * ticks_per_second / BUFFER_TRANSFER_RATE;
      buffered_blocks++;
    }
    else
    {
      // In practice we'll only ever seek if this is the first time
      // through this loop.
      if (dvd_offset != head_position)
      {
        // Unbuffered seek+read
        ticks_until_completion += static_cast<u64>(
            ticks_per_second * DVDMath::CalculateSeekTime(head_position, dvd_offset));

        // TODO: The above emulates seeking and then reading one ECC block of data,
        // and then the below emulates the rotational latency. The rotational latency
        // should actually happen before reading data from the disc.

        const double time_after_seek =
            (CoreTiming::GetTicks() + ticks_until_completion) / ticks_per_second;
        ticks_until_completion += ticks_per_second * DVDMath::CalculateRotationalLatency(
                                                         dvd_offset, time_after_seek, wii_disc);

        DEBUG_LOG(DVDINTERFACE, "Seek+read 0x%" PRIx32 " bytes @ 0x%" PRIx64 " ticks=%" PRId64,
                  chunk_length, offset, ticks_until_completion);
      }
      else
      {
        // Unbuffered read
        ticks_until_completion +=
            static_cast<u64>(ticks_per_second * DVDMath::CalculateRawDiscReadTime(
                                                    dvd_offset, DVD_ECC_BLOCK_SIZE, wii_disc));
      }

      unbuffered_blocks++;
      head_position = dvd_offset + DVD_ECC_BLOCK_SIZE;
    }

    // Schedule this read to complete at the appropriate time
    const ReplyType chunk_reply_type = chunk_length == length ? reply_type : ReplyType::NoReply;
    DVDThread::StartReadToEmulatedRAM(output_address, offset, chunk_length, partition,
                                      chunk_reply_type, ticks_until_completion);

    // Advance the read window
    output_address += chunk_length;
    offset += chunk_length;
    length -= chunk_length;
    dvd_offset += DVD_ECC_BLOCK_SIZE;
  } while (length > 0);

  // Update the buffer based on this read. Based on experimental testing,
  // we will only reuse the old buffer while reading forward. Note that the
  // buffer start we calculate here is not the actual start of the buffer -
  // it is just the start of the portion we need to read.
  const u64 last_block = dvd_offset;
  if (last_block == buffer_start + DVD_ECC_BLOCK_SIZE && buffer_start != buffer_end)
  {
    // Special case: reading less than one block at the start of the
    // buffer won't change the buffer state
  }
  else
  {
    if (last_block >= buffer_end)
      // Full buffer read
      s_read_buffer_start_offset = last_block;
    else
      // Partial buffer read
      s_read_buffer_start_offset = buffer_end;

    s_read_buffer_end_offset = last_block + STREAMING_BUFFER_SIZE - DVD_ECC_BLOCK_SIZE;
    // Assume the buffer starts reading right after the end of the last operation
    s_read_buffer_start_time = current_time + ticks_until_completion;
    s_read_buffer_end_time =
        s_read_buffer_start_time +
        static_cast<u64>(ticks_per_second *
                         DVDMath::CalculateRawDiscReadTime(
                             s_read_buffer_start_offset,
                             s_read_buffer_end_offset - s_read_buffer_start_offset, wii_disc));
  }

  DEBUG_LOG(DVDINTERFACE,
            "Schedule reads: ECC blocks unbuffered=%d, buffered=%d, "
            "ticks=%" PRId64 ", time=%" PRId64 " us",
            unbuffered_blocks, buffered_blocks, ticks_until_completion,
            ticks_until_completion * 1000000 / SystemTimers::GetTicksPerSecond());
}

}  // namespace DVDInterface
