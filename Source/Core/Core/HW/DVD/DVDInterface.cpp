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

#include "Core/Analytics.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DVD/DVDMath.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
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

// DI Status Register
union UDISR
{
  u32 Hex;
  struct
  {
    u32 BREAK : 1;      // Stop the Device + Interrupt
    u32 DEINTMASK : 1;  // Access Device Error Int Mask
    u32 DEINT : 1;      // Access Device Error Int
    u32 TCINTMASK : 1;  // Transfer Complete Int Mask
    u32 TCINT : 1;      // Transfer Complete Int
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
static u32 s_DICMDBUF[3];
static u32 s_DIMAR;
static u32 s_DILENGTH;
static UDICR s_DICR;
static u32 s_DIIMMBUF;
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
static bool s_can_configure_dtk = true;
static bool s_enable_dtk = false;
static u8 s_dtk_buffer_length = 0;  // TODO: figure out how this affects the regular buffer

// Disc drive state
static u32 s_error_code = 0;

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
  p.Do(s_can_configure_dtk);
  p.Do(s_enable_dtk);
  p.Do(s_dtk_buffer_length);

  p.Do(s_error_code);

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

static void DTKStreamingCallback(DIInterruptType interrupt_type, const std::vector<u8>& audio_data,
                                 s64 cycles_late)
{
  // Determine which audio data to read next.
  static const int MAXIMUM_SAMPLES = 48000 / 2000 * 7;  // 3.5ms of 48kHz samples
  u64 read_offset = 0;
  u32 read_length = 0;

  if (interrupt_type == DIInterruptType::TCINT)
  {
    // Send audio to the mixer.
    std::vector<s16> temp_pcm(s_pending_samples * 2, 0);
    ProcessDTKSamples(&temp_pcm, audio_data);
    g_sound_stream->GetMixer()->PushStreamingSamples(temp_pcm.data(), s_pending_samples);

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
    u64 userdata = PackFinishExecutingCommandUserdata(ReplyType::DTK, DIInterruptType::TCINT);
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

  u64 userdata = PackFinishExecutingCommandUserdata(ReplyType::DTK, DIInterruptType::TCINT);
  CoreTiming::ScheduleEvent(0, s_finish_executing_command, userdata);
}

// This doesn't reset any inserted disc or the cover state.
void Reset(bool spinup)
{
  INFO_LOG(DVDINTERFACE, "Reset %s spinup", spinup ? "with" : "without");

  s_DISR.Hex = 0;
  s_DICMDBUF[0] = 0;
  s_DICMDBUF[1] = 0;
  s_DICMDBUF[2] = 0;
  s_DIMAR = 0;
  s_DILENGTH = 0;
  s_DICR.Hex = 0;
  s_DIIMMBUF = 0;
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
  s_can_configure_dtk = true;
  s_enable_dtk = false;
  s_dtk_buffer_length = 0;

  if (!IsDiscInside())
  {
    // ERROR_COVER is used when the cover is open;
    // ERROR_NO_DISK_L is used when the cover is closed but there is no disc.
    // On the Wii, this can only happen if something other than a DVD is inserted into the disc
    // drive (for instance, an audio CD) and only after it attempts to read it.  Otherwise, it will
    // report the cover as opened.
    SetLowError(ERROR_COVER);
  }
  else if (!spinup)
  {
    // Wii hardware tests indicate that this is used when ejecting and inserting a new disc, or
    // performing a reset without spinup.
    SetLowError(ERROR_CHANGE_DISK);
  }
  else
  {
    SetLowError(ERROR_NO_DISKID_L);
  }

  SetHighError(ERROR_NONE);

  // The buffer is empty at start
  s_read_buffer_start_offset = 0;
  s_read_buffer_end_offset = 0;
  s_read_buffer_start_time = 0;
  s_read_buffer_end_time = 0;
}

void Shutdown()
{
  DVDThread::Stop();
}

void SetDisc(std::unique_ptr<DiscIO::VolumeDisc> disc,
             std::optional<std::vector<std::string>> auto_disc_change_paths = {})
{
  bool had_disc = IsDiscInside();
  bool has_disc = static_cast<bool>(disc);

  if (auto_disc_change_paths)
  {
    ASSERT_MSG(DISCIO, (*auto_disc_change_paths).size() != 1,
               "Cannot automatically change between one disc");

    s_auto_disc_change_paths = *auto_disc_change_paths;
    s_auto_disc_change_index = 0;
  }

  // Assume that inserting a disc requires having an empty disc before
  if (had_disc != has_disc)
    ExpansionInterface::g_rtc_flags[ExpansionInterface::RTCFlag::DiscChanged] = true;

  DVDThread::SetDisc(std::move(disc));
  SetLidOpen();

  Reset(false);
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
void EjectDisc(EjectCause cause)
{
  CoreTiming::ScheduleEvent(0, s_eject_disc);
  if (cause == EjectCause::User)
    ExpansionInterface::g_rtc_flags[ExpansionInterface::RTCFlag::EjectButton] = true;
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

  EjectDisc(EjectCause::User);

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
    GenerateDIInterrupt(DIInterruptType::CVRINT);
}

bool UpdateRunningGameMetadata(std::optional<u64> title_id)
{
  if (!DVDThread::HasDisc())
    return false;

  return DVDThread::UpdateRunningGameMetadata(IOS::HLE::Device::DI::GetCurrentPartition(),
                                              title_id);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  mmio->Register(base | DI_STATUS_REGISTER, MMIO::DirectRead<u32>(&s_DISR.Hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   UDISR tmpStatusReg(val);

                   s_DISR.DEINTMASK = tmpStatusReg.DEINTMASK;
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

  // Command registers, which have no special logic
  mmio->Register(base | DI_COMMAND_0, MMIO::DirectRead<u32>(&s_DICMDBUF[0]),
                 MMIO::DirectWrite<u32>(&s_DICMDBUF[0]));
  mmio->Register(base | DI_COMMAND_1, MMIO::DirectRead<u32>(&s_DICMDBUF[1]),
                 MMIO::DirectWrite<u32>(&s_DICMDBUF[1]));
  mmio->Register(base | DI_COMMAND_2, MMIO::DirectRead<u32>(&s_DICMDBUF[2]),
                 MMIO::DirectWrite<u32>(&s_DICMDBUF[2]));

  // DMA related registers. Mostly direct accesses (+ masking for writes to
  // handle things like address alignment) and complex write on the DMA
  // control register that will trigger the DMA.
  mmio->Register(base | DI_DMA_ADDRESS_REGISTER, MMIO::DirectRead<u32>(&s_DIMAR),
                 MMIO::DirectWrite<u32>(&s_DIMAR, ~0x1F));
  mmio->Register(base | DI_DMA_LENGTH_REGISTER, MMIO::DirectRead<u32>(&s_DILENGTH),
                 MMIO::DirectWrite<u32>(&s_DILENGTH, ~0x1F));
  mmio->Register(base | DI_DMA_CONTROL_REGISTER, MMIO::DirectRead<u32>(&s_DICR.Hex),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   s_DICR.Hex = val & 7;
                   if (s_DICR.TSTART)
                   {
                     ExecuteCommand(ReplyType::Interrupt);
                   }
                 }));

  mmio->Register(base | DI_IMMEDIATE_DATA_BUFFER, MMIO::DirectRead<u32>(&s_DIIMMBUF),
                 MMIO::DirectWrite<u32>(&s_DIIMMBUF));

  // DI config register is read only.
  mmio->Register(base | DI_CONFIG_REGISTER, MMIO::DirectRead<u32>(&s_DICFG.Hex),
                 MMIO::InvalidWrite<u32>());
}

void UpdateInterrupts()
{
  const bool set_mask = (s_DISR.DEINT & s_DISR.DEINTMASK) || (s_DISR.TCINT & s_DISR.TCINTMASK) ||
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
  case DIInterruptType::DEINT:
    s_DISR.DEINT = true;
    break;
  case DIInterruptType::TCINT:
    s_DISR.TCINT = true;
    break;
  case DIInterruptType::BRKINT:
    s_DISR.BRKINT = true;
    break;
  case DIInterruptType::CVRINT:
    s_DICVR.CVRINT = true;
    break;
  }

  UpdateInterrupts();
}

void SetInterruptEnabled(DIInterruptType interrupt, bool enabled)
{
  switch (interrupt)
  {
  case DIInterruptType::DEINT:
    s_DISR.DEINTMASK = enabled;
    break;
  case DIInterruptType::TCINT:
    s_DISR.TCINTMASK = enabled;
    break;
  case DIInterruptType::BRKINT:
    s_DISR.BRKINTMASK = enabled;
    break;
  case DIInterruptType::CVRINT:
    s_DICVR.CVRINTMASK = enabled;
    break;
  }
}

void ClearInterrupt(DIInterruptType interrupt)
{
  switch (interrupt)
  {
  case DIInterruptType::DEINT:
    s_DISR.DEINT = false;
    break;
  case DIInterruptType::TCINT:
    s_DISR.TCINT = false;
    break;
  case DIInterruptType::BRKINT:
    s_DISR.BRKINT = false;
    break;
  case DIInterruptType::CVRINT:
    s_DICVR.CVRINT = false;
    break;
  }
}

// Checks the drive state to make sure a read-like command can be performed.
// If false is returned, SetHighError will have been called, and the caller
// should issue a DEINT interrupt.
static bool CheckReadPreconditions()
{
  if (!IsDiscInside())  // Implies ERROR_COVER or ERROR_NO_DISK
  {
    ERROR_LOG(DVDINTERFACE, "No disc inside.");
    SetHighError(ERROR_NO_DISK_H);
    return false;
  }
  if ((s_error_code & LOW_ERROR_MASK) == ERROR_CHANGE_DISK)
  {
    ERROR_LOG(DVDINTERFACE, "Disc changed (motor stopped).");
    SetHighError(ERROR_MEDIUM);
    return false;
  }
  if ((s_error_code & LOW_ERROR_MASK) == ERROR_MOTOR_STOP_L)
  {
    ERROR_LOG(DVDINTERFACE, "Motor stopped.");
    SetHighError(ERROR_MOTOR_STOP_H);
    return false;
  }
  if ((s_error_code & LOW_ERROR_MASK) == ERROR_NO_DISKID_L)
  {
    ERROR_LOG(DVDINTERFACE, "Disc id not read.");
    SetHighError(ERROR_NO_DISKID_H);
    return false;
  }
  return true;
}

// Iff false is returned, ScheduleEvent must be used to finish executing the command
bool ExecuteReadCommand(u64 dvd_offset, u32 output_address, u32 dvd_length, u32 output_length,
                        const DiscIO::Partition& partition, ReplyType reply_type,
                        DIInterruptType* interrupt_type)
{
  if (!CheckReadPreconditions())
  {
    // Disc read fails
    *interrupt_type = DIInterruptType::DEINT;
    return false;
  }
  else
  {
    // Disc read succeeds
    *interrupt_type = DIInterruptType::TCINT;
  }

  if (dvd_length > output_length)
  {
    WARN_LOG(DVDINTERFACE, "Detected an attempt to read more data from the DVD "
                           "than what fits inside the out buffer. Clamping.");
    dvd_length = output_length;
  }

  // Many Wii games intentionally try to read from an offset which is just past the end of a regular
  // DVD but just before the end of a DVD-R, displaying "Error #001" and failing to boot if the read
  // succeeds (see https://wiibrew.org/wiki//dev/di#0x8D_DVDLowUnencryptedRead for more details).
  // It would be nice if we simply could rely on DiscIO for letting us know whether a read is out
  // of bounds, but this unfortunately doesn't work when using a disc image format that doesn't
  // store the original size of the disc, most notably WBFS. Instead, we have a little hack here:
  // reject all non-partition reads that come from IOS that go past the offset 0x50000. IOS only
  // allows non-partition reads if they are before 0x50000 or if they are in one of the two small
  // areas 0x118240000-0x118240020 and 0x1FB4E0000-0x1FB4E0020 (both of which only are used for
  // Error #001 checks), so the only thing we disallow with this hack that actually should be
  // allowed is non-partition reads in the 0x118240000-0x118240020 area on dual-layer discs.
  // In practice, dual-layer games don't attempt to do non-partition reads in that area.
  if (reply_type == ReplyType::IOS && partition == DiscIO::PARTITION_NONE &&
      dvd_offset + dvd_length > 0x50000)
  {
    SetHighError(DVDInterface::ERROR_BLOCK_OOB);
    *interrupt_type = DIInterruptType::DEINT;
    return false;
  }

  ScheduleReads(dvd_offset, dvd_length, partition, output_address, reply_type);
  return true;
}

// When the command has finished executing, callback_event_type
// will be called using CoreTiming::ScheduleEvent,
// with the userdata set to the interrupt type.
void ExecuteCommand(ReplyType reply_type)
{
  DIInterruptType interrupt_type = DIInterruptType::TCINT;
  bool command_handled_by_thread = false;

  // DVDLowRequestError needs access to the error code set by the previous command
  if (static_cast<DICommand>(s_DICMDBUF[0] >> 24) != DICommand::RequestError)
    SetHighError(0);

  switch (static_cast<DICommand>(s_DICMDBUF[0] >> 24))
  {
  // Used by both GC and Wii
  case DICommand::Inquiry:
    // (shuffle2) Taken from my Wii
    Memory::Write_U32(0x00000002, s_DIMAR);      // Revision level, device code
    Memory::Write_U32(0x20060526, s_DIMAR + 4);  // Release date
    Memory::Write_U32(0x41000000, s_DIMAR + 8);  // Version

    INFO_LOG(DVDINTERFACE, "DVDLowInquiry (Buffer 0x%08x, 0x%x)", s_DIMAR, s_DILENGTH);
    break;

  // GC-only patched drive firmware command, used by libogc
  case DICommand::Unknown55:
    INFO_LOG(DVDINTERFACE, "SetExtension");
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // Wii-exclusive
  case DICommand::ReportKey:
    INFO_LOG(DVDINTERFACE, "DVDLowReportKey");
    // Does not work on retail discs/drives
    // Retail games send this command to see if they are running on real retail hw
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // DMA Read from Disc. Only used through direct access on GC; direct use is prohibited by
  // IOS (which uses it internally)
  case DICommand::Read:
    switch (s_DICMDBUF[0] & 0xFF)
    {
    case 0x00:  // Read Sector
    {
      u64 iDVDOffset = static_cast<u64>(s_DICMDBUF[1]) << 2;

      INFO_LOG(DVDINTERFACE,
               "Read: DVDOffset=%08" PRIx64
               ", DMABuffer = %08x, SrcLength = %08x, DMALength = %08x",
               iDVDOffset, s_DIMAR, s_DICMDBUF[2], s_DILENGTH);

      s_can_configure_dtk = false;
      command_handled_by_thread =
          ExecuteReadCommand(iDVDOffset, s_DIMAR, s_DICMDBUF[2], s_DILENGTH, DiscIO::PARTITION_NONE,
                             reply_type, &interrupt_type);
    }
    break;

    case 0x40:  // Read DiscID
      INFO_LOG(DVDINTERFACE, "Read DiscID: buffer %08x", s_DIMAR);
      // TODO: It doesn't make sense to include ERROR_CHANGE_DISK here, as it implies that the drive
      // is not spinning and reading the disc ID shouldn't change it.  However, the Wii Menu breaks
      // without it.
      if ((s_error_code & LOW_ERROR_MASK) == ERROR_NO_DISKID_L ||
          (s_error_code & LOW_ERROR_MASK) == ERROR_CHANGE_DISK)
      {
        SetLowError(ERROR_READY);
      }
      else
      {
        // The first disc ID reading is required before DTK can be configured.
        // If the disc ID is read again (or any other read occurs), it no longer can
        // be configured.
        s_can_configure_dtk = false;
      }

      command_handled_by_thread = ExecuteReadCommand(
          0, s_DIMAR, 0x20, s_DILENGTH, DiscIO::PARTITION_NONE, reply_type, &interrupt_type);
      break;

    default:
      ERROR_LOG(DVDINTERFACE, "Unknown read subcommand: %08x", s_DICMDBUF[0]);
      break;
    }
    break;

  // Used by both GC and Wii
  case DICommand::Seek:
    // Currently unimplemented
    INFO_LOG(DVDINTERFACE, "Seek: offset=%09" PRIx64 " (ignoring)",
             static_cast<u64>(s_DICMDBUF[1]) << 2);
    break;

  // Wii-exclusive
  case DICommand::ReadDVDMetadata:
    switch ((s_DICMDBUF[0] >> 16) & 0xFF)
    {
    case 0:
      ERROR_LOG(DVDINTERFACE, "DVDLowReadDvdPhysical");
      break;
    case 1:
      ERROR_LOG(DVDINTERFACE, "DVDLowReadDvdCopyright");
      break;
    case 2:
      ERROR_LOG(DVDINTERFACE, "DVDLowReadDvdDiscKey");
      break;
    default:
      ERROR_LOG(DVDINTERFACE, "Unknown 0xAD subcommand in %08x", s_DICMDBUF[0]);
      break;
    }
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::ReadDVD:
    ERROR_LOG(DVDINTERFACE, "DVDLowReadDvd");
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::ReadDVDConfig:
    ERROR_LOG(DVDINTERFACE, "DVDLowReadDvdConfig");
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::StopLaser:
    ERROR_LOG(DVDINTERFACE, "DVDLowStopLaser");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_STOP_LASER);
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::Offset:
    ERROR_LOG(DVDINTERFACE, "DVDLowOffset");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_OFFSET);
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::ReadBCA:
    WARN_LOG(DVDINTERFACE, "DVDLowReadDiskBca - supplying dummy data to appease NSMBW");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_READ_DISK_BCA);
    // NSMBW checks that the first 0x33 bytes of the BCA are 0, then it expects a 1.
    // Most (all?) other games have 0x34 0's at the start of the BCA, but don't actually
    // read it.  NSMBW doesn't care about the other 12 bytes (which contain manufacturing data?)

    // TODO: Read the .bca file that cleanrip generates, if it exists
    // Memory::CopyToEmu(output_address, bca_data, 0x40);
    Memory::Memset(s_DIMAR, 0, 0x40);
    Memory::Write_U8(1, s_DIMAR + 0x33);
    break;
  // Wii-exclusive
  case DICommand::RequestDiscStatus:
    ERROR_LOG(DVDINTERFACE, "DVDLowRequestDiscStatus");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_REQUEST_DISC_STATUS);
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::RequestRetryNumber:
    ERROR_LOG(DVDINTERFACE, "DVDLowRequestRetryNumber");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_REQUEST_RETRY_NUMBER);
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::SetMaximumRotation:
    ERROR_LOG(DVDINTERFACE, "DVDLowSetMaximumRotation");
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::SerMeasControl:
    ERROR_LOG(DVDINTERFACE, "DVDLowSerMeasControl");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_SER_MEAS_CONTROL);
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // Used by both GC and Wii
  case DICommand::RequestError:
    INFO_LOG(DVDINTERFACE, "Requesting error... (0x%08x)", s_error_code);
    s_DIIMMBUF = s_error_code;
    SetHighError(0);
    break;

  // Audio Stream (Immediate). Only used by some GC games, but does exist on the Wii
  // (command_0 >> 16) & 0xFF = Subcommand
  // command_1 << 2           = Offset on disc
  // command_2                = Length of the stream
  case DICommand::AudioStream:
  {
    if (!CheckReadPreconditions())
    {
      ERROR_LOG(DVDINTERFACE, "Cannot play audio (command %08x)", s_DICMDBUF[0]);
      SetHighError(ERROR_AUDIO_BUF);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }
    if (!s_enable_dtk)
    {
      ERROR_LOG(DVDINTERFACE,
                "Attempted to change playing audio while audio is disabled!  (%08x %08x %08x)",
                s_DICMDBUF[0], s_DICMDBUF[1], s_DICMDBUF[2]);
      SetHighError(ERROR_AUDIO_BUF);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    s_can_configure_dtk = false;

    switch ((s_DICMDBUF[0] >> 16) & 0xFF)
    {
    case 0x00:
    {
      u64 offset = static_cast<u64>(s_DICMDBUF[1]) << 2;
      u32 length = s_DICMDBUF[2];
      INFO_LOG(DVDINTERFACE, "(Audio) Start stream: offset: %08" PRIx64 " length: %08x", offset,
               length);

      if ((offset == 0) && (length == 0))
      {
        s_stop_at_track_end = true;
      }
      else if (!s_stop_at_track_end)
      {
        s_next_start = offset;
        s_next_length = length;
        if (!s_stream)
        {
          s_current_start = s_next_start;
          s_current_length = s_next_length;
          s_audio_position = s_current_start;
          s_adpcm_decoder.ResetFilter();
          s_stream = true;
        }
      }
      break;
    }
    case 0x01:
      INFO_LOG(DVDINTERFACE, "(Audio) Stop stream");
      s_stop_at_track_end = false;
      s_stream = false;
      break;
    default:
      ERROR_LOG(DVDINTERFACE, "Invalid audio command!  (%08x %08x %08x)", s_DICMDBUF[0],
                s_DICMDBUF[1], s_DICMDBUF[2]);
      SetHighError(ERROR_INV_AUDIO);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }
  }
  break;

  // Request Audio Status (Immediate). Only used by some GC games, but does exist on the Wii
  case DICommand::RequestAudioStatus:
  {
    if (!s_enable_dtk)
    {
      ERROR_LOG(DVDINTERFACE, "Attempted to request audio status while audio is disabled!");
      SetHighError(ERROR_AUDIO_BUF);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    switch (s_DICMDBUF[0] >> 16 & 0xFF)
    {
    case 0x00:  // Returns streaming status
      INFO_LOG(DVDINTERFACE,
               "(Audio): Stream Status: Request Audio status "
               "AudioPos:%08" PRIx64 "/%08" PRIx64 " "
               "CurrentStart:%08" PRIx64 " CurrentLength:%08x",
               s_audio_position, s_current_start + s_current_length, s_current_start,
               s_current_length);
      s_DIIMMBUF = (s_stream ? 1 : 0);
      break;
    case 0x01:  // Returns the current offset
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:%08" PRIx64,
               s_audio_position);
      s_DIIMMBUF = static_cast<u32>((s_audio_position & 0xffffffffffff8000ull) >> 2);
      break;
    case 0x02:  // Returns the start offset
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentStart:%08" PRIx64,
               s_current_start);
      s_DIIMMBUF = static_cast<u32>(s_current_start >> 2);
      break;
    case 0x03:  // Returns the total length
      INFO_LOG(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentLength:%08x",
               s_current_length);
      s_DIIMMBUF = s_current_length;
      break;
    default:
      ERROR_LOG(DVDINTERFACE, "Invalid audio status command!  (%08x %08x %08x)", s_DICMDBUF[0],
                s_DICMDBUF[1], s_DICMDBUF[2]);
      SetHighError(ERROR_INV_AUDIO);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }
  }
  break;

  // Used by both GC and Wii
  case DICommand::StopMotor:
  {
    const bool eject = (s_DICMDBUF[0] & (1 << 17));
    const bool kill = (s_DICMDBUF[0] & (1 << 20));
    INFO_LOG(DVDINTERFACE, "DVDLowStopMotor%s%s", eject ? " eject" : "", kill ? " kill!" : "");

    SetLowError(ERROR_MOTOR_STOP_L);
    const bool force_eject = eject && !kill;

    if (Config::Get(Config::MAIN_AUTO_DISC_CHANGE) && !Movie::IsPlayingInput() &&
        DVDThread::IsInsertedDiscRunning() && !s_auto_disc_change_paths.empty())
    {
      CoreTiming::ScheduleEvent(force_eject ? 0 : SystemTimers::GetTicksPerSecond() / 2,
                                s_auto_change_disc);
      OSD::AddMessage("Changing discs automatically...", OSD::Duration::NORMAL);
    }
    else if (force_eject)
    {
      EjectDisc(EjectCause::Software);
    }
    break;
  }

  // DVD Audio Enable/Disable (Immediate). GC uses this, and the Wii can use it to configure GC
  // games.
  case DICommand::AudioBufferConfig:
    // The IPL uses this command to enable or disable DTK audio depending on the value of byte 0x8
    // in the disc header. See http://www.crazynation.org/GC/GC_DD_TECH/GCTech.htm for more info.
    // The link is dead, but you can access the page using the Wayback Machine at archive.org.

    // This command can only be used immediately after reading the disc ID, before any other
    // reads. Too early, and you get ERROR_NO_DISKID.  Too late, and you get ERROR_INV_PERIOD.
    if (!s_can_configure_dtk)
    {
      ERROR_LOG(DVDINTERFACE, "Attempted to change DTK configuration after a read has been made!");
      SetHighError(ERROR_INV_PERIOD);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    AudioBufferConfig((s_DICMDBUF[0] >> 16) & 1, s_DICMDBUF[0] & 0xf);
    break;

  // GC-only patched drive firmware command, used by libogc
  case DICommand::UnknownEE:
    INFO_LOG(DVDINTERFACE, "SetStatus");
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // Debug commands; see yagcd. We don't really care
  // NOTE: commands to stream data will send...a raw data stream
  // This will appear as unknown commands, unless the check is re-instated to catch such data.
  // Can only be used through direct access and only after unlocked.
  case DICommand::Debug:
    ERROR_LOG(DVDINTERFACE, "Unsupported DVD Drive debug command 0x%08x", s_DICMDBUF[0]);
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // Unlock Commands. 1: "MATSHITA" 2: "DVD-GAME"
  // Just for fun
  // Can only be used through direct access.  The unlock command doesn't seem to work on the Wii.
  case DICommand::DebugUnlock:
  {
    if (s_DICMDBUF[0] == 0xFF014D41 && s_DICMDBUF[1] == 0x54534849 && s_DICMDBUF[2] == 0x54410200)
    {
      INFO_LOG(DVDINTERFACE, "Unlock test 1 passed");
    }
    else if (s_DICMDBUF[0] == 0xFF004456 && s_DICMDBUF[1] == 0x442D4741 &&
             s_DICMDBUF[2] == 0x4D450300)
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
    ERROR_LOG(DVDINTERFACE, "Unknown command 0x%08x (Buffer 0x%08x, 0x%x)", s_DICMDBUF[0], s_DIMAR,
              s_DILENGTH);
    PanicAlertT("Unknown DVD command %08x - fatal error", s_DICMDBUF[0]);
    SetHighError(ERROR_INV_CMD);
    interrupt_type = DIInterruptType::DEINT;
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

void PerformDecryptingRead(u32 position, u32 length, u32 output_address,
                           const DiscIO::Partition& partition, ReplyType reply_type)
{
  DIInterruptType interrupt_type = DIInterruptType::TCINT;
  s_can_configure_dtk = false;

  const bool command_handled_by_thread =
      ExecuteReadCommand(static_cast<u64>(position) << 2, output_address, length, length, partition,
                         reply_type, &interrupt_type);

  if (!command_handled_by_thread)
  {
    // TODO: Needs testing to determine if COMMAND_LATENCY_US is accurate for this
    CoreTiming::ScheduleEvent(COMMAND_LATENCY_US * (SystemTimers::GetTicksPerSecond() / 1000000),
                              s_finish_executing_command,
                              PackFinishExecutingCommandUserdata(reply_type, interrupt_type));
  }
}

void AudioBufferConfig(bool enable_dtk, u8 dtk_buffer_length)
{
  s_enable_dtk = enable_dtk;
  s_dtk_buffer_length = dtk_buffer_length;
  if (s_enable_dtk)
    INFO_LOG(DVDINTERFACE, "DTK enabled: buffer size %d", s_dtk_buffer_length);
  else
    INFO_LOG(DVDINTERFACE, "DTK disabled");
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

void SetLowError(u32 low_error)
{
  DEBUG_ASSERT((low_error & HIGH_ERROR_MASK) == 0);
  s_error_code = (s_error_code & HIGH_ERROR_MASK) | (low_error & LOW_ERROR_MASK);
}

void SetHighError(u32 high_error)
{
  DEBUG_ASSERT((high_error & LOW_ERROR_MASK) == 0);
  s_error_code = (s_error_code & LOW_ERROR_MASK) | (high_error & HIGH_ERROR_MASK);
}

void FinishExecutingCommand(ReplyType reply_type, DIInterruptType interrupt_type, s64 cycles_late,
                            const std::vector<u8>& data)
{
  s_DIMAR += s_DILENGTH;
  s_DILENGTH = 0;

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
      GenerateDIInterrupt(interrupt_type);
    }
    break;
  }

  case ReplyType::IOS:
  {
    IOS::HLE::Device::DI::InterruptFromDVDInterface(interrupt_type);
    break;
  }

  case ReplyType::DTK:
  {
    DTKStreamingCallback(interrupt_type, data, cycles_late);
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
