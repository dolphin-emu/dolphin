// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DVD/DVDInterface.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "AudioCommon/AudioCommon.h"

#include "Common/Align.h"
#include "Common/BitField.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Logging/Log.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/SessionSettings.h"
#include "Core/CoreTiming.h"
#include "Core/DolphinAnalytics.h"
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
#include "Core/System.h"

#include "DiscIO/Blob.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Enums.h"
#include "DiscIO/VolumeDisc.h"
#include "DiscIO/VolumeWii.h"

#include "VideoCommon/OnScreenDisplay.h"

// The minimum time it takes for the DVD drive to process a command (in microseconds)
constexpr u64 MINIMUM_COMMAND_LATENCY_US = 300;

// The time it takes for a read command to start (in microseconds)
constexpr u64 READ_COMMAND_LATENCY_US = 600;

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
  u32 Hex = 0;

  BitField<0, 1, u32> BREAK;      // Stop the Device + Interrupt
  BitField<1, 1, u32> DEINTMASK;  // Access Device Error Int Mask
  BitField<2, 1, u32> DEINT;      // Access Device Error Int
  BitField<3, 1, u32> TCINTMASK;  // Transfer Complete Int Mask
  BitField<4, 1, u32> TCINT;      // Transfer Complete Int
  BitField<5, 1, u32> BRKINTMASK;
  BitField<6, 1, u32> BRKINT;  // w 1: clear brkint
  BitField<7, 25, u32> reserved;

  UDISR() = default;
  explicit UDISR(u32 hex) : Hex{hex} {}
};

// DI Cover Register
union UDICVR
{
  u32 Hex = 0;

  BitField<0, 1, u32> CVR;         // 0: Cover closed  1: Cover open
  BitField<1, 1, u32> CVRINTMASK;  // 1: Interrupt enabled
  BitField<2, 1, u32> CVRINT;      // r 1: Interrupt requested w 1: Interrupt clear
  BitField<3, 29, u32> reserved;

  UDICVR() = default;
  explicit UDICVR(u32 hex) : Hex{hex} {}
};

// DI DMA Control Register
union UDICR
{
  u32 Hex = 0;

  BitField<0, 1, u32> TSTART;  // w:1 start   r:0 ready
  BitField<1, 1, u32> DMA;     // 1: DMA Mode
                               // 0: Immediate Mode (can only do Access Register Command)
  BitField<2, 1, u32> RW;      // 0: Read Command (DVD to Memory)  1: Write Command (Memory to DVD)
  BitField<3, 29, u32> reserved;
};

// DI Config Register
union UDICFG
{
  u32 Hex = 0;

  BitField<0, 8, u32> CONFIG;
  BitField<8, 24, u32> reserved;

  UDICFG() = default;
  explicit UDICFG(u32 hex) : Hex{hex} {}
};

struct DVDInterfaceState::Data
{
  // Hardware registers
  UDISR DISR;
  UDICVR DICVR;
  u32 DICMDBUF[3];
  u32 DIMAR;
  u32 DILENGTH;
  UDICR DICR;
  u32 DIIMMBUF;
  UDICFG DICFG;

  StreamADPCM::ADPCMDecoder adpcm_decoder;

  // DTK
  bool stream = false;
  bool stop_at_track_end = false;
  u64 audio_position;
  u64 current_start;
  u32 current_length;
  u64 next_start;
  u32 next_length;
  u32 pending_samples;
  bool enable_dtk = false;
  u8 dtk_buffer_length = 0;  // TODO: figure out how this affects the regular buffer

  // Disc drive state
  DriveState drive_state;
  DriveError error_code;
  u64 disc_end_offset;

  // Disc drive timing
  u64 read_buffer_start_time;
  u64 read_buffer_end_time;
  u64 read_buffer_start_offset;
  u64 read_buffer_end_offset;

  // Disc changing
  std::string disc_path_to_insert;
  std::vector<std::string> auto_disc_change_paths;
  size_t auto_disc_change_index;

  // Events
  CoreTiming::EventType* finish_executing_command;
  CoreTiming::EventType* auto_change_disc;
  CoreTiming::EventType* eject_disc;
  CoreTiming::EventType* insert_disc;
};

DVDInterfaceState::DVDInterfaceState() : m_data(std::make_unique<Data>())
{
}

DVDInterfaceState::~DVDInterfaceState() = default;

static void AutoChangeDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate);
static void EjectDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate);
static void InsertDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate);
static void FinishExecutingCommandCallback(Core::System& system, u64 userdata, s64 cycles_late);

static void SetLidOpen();

static void UpdateInterrupts();
static void GenerateDIInterrupt(DIInterruptType dvd_interrupt);

static bool ExecuteReadCommand(u64 dvd_offset, u32 output_address, u32 dvd_length,
                               u32 output_length, const DiscIO::Partition& partition,
                               ReplyType reply_type, DIInterruptType* interrupt_type);

static u64 PackFinishExecutingCommandUserdata(ReplyType reply_type, DIInterruptType interrupt_type);

static void ScheduleReads(u64 offset, u32 length, const DiscIO::Partition& partition,
                          u32 output_address, ReplyType reply_type);

void DoState(PointerWrap& p)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();

  p.Do(state.DISR);
  p.Do(state.DICVR);
  p.DoArray(state.DICMDBUF);
  p.Do(state.DIMAR);
  p.Do(state.DILENGTH);
  p.Do(state.DICR);
  p.Do(state.DIIMMBUF);
  p.Do(state.DICFG);

  p.Do(state.stream);
  p.Do(state.stop_at_track_end);
  p.Do(state.audio_position);
  p.Do(state.current_start);
  p.Do(state.current_length);
  p.Do(state.next_start);
  p.Do(state.next_length);
  p.Do(state.pending_samples);
  p.Do(state.enable_dtk);
  p.Do(state.dtk_buffer_length);

  p.Do(state.drive_state);
  p.Do(state.error_code);

  p.Do(state.read_buffer_start_time);
  p.Do(state.read_buffer_end_time);
  p.Do(state.read_buffer_start_offset);
  p.Do(state.read_buffer_end_offset);

  p.Do(state.disc_path_to_insert);

  DVDThread::DoState(p);

  state.adpcm_decoder.DoState(p);
}

static size_t ProcessDTKSamples(std::vector<s16>* temp_pcm, const std::vector<u8>& audio_data)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();

  size_t samples_processed = 0;
  size_t bytes_processed = 0;
  while (samples_processed < temp_pcm->size() / 2 && bytes_processed < audio_data.size())
  {
    state.adpcm_decoder.DecodeBlock(&(*temp_pcm)[samples_processed * 2],
                                    &audio_data[bytes_processed]);
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
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();

  u32 bytes_to_process = 0;
  *samples_to_process = 0;
  while (*samples_to_process < maximum_samples)
  {
    if (state.audio_position >= state.current_start + state.current_length)
    {
      DEBUG_LOG_FMT(DVDINTERFACE,
                    "AdvanceDTK: NextStart={:08x}, NextLength={:08x}, "
                    "CurrentStart={:08x}, CurrentLength={:08x}, AudioPos={:08x}",
                    state.next_start, state.next_length, state.current_start, state.current_length,
                    state.audio_position);

      state.audio_position = state.next_start;
      state.current_start = state.next_start;
      state.current_length = state.next_length;

      if (state.stop_at_track_end)
      {
        state.stop_at_track_end = false;
        state.stream = false;
        break;
      }

      state.adpcm_decoder.ResetFilter();
    }

    state.audio_position += StreamADPCM::ONE_BLOCK_SIZE;
    bytes_to_process += StreamADPCM::ONE_BLOCK_SIZE;
    *samples_to_process += StreamADPCM::SAMPLES_PER_BLOCK;
  }

  return bytes_to_process;
}

static void DTKStreamingCallback(DIInterruptType interrupt_type, const std::vector<u8>& audio_data,
                                 s64 cycles_late)
{
  auto& system = Core::System::GetInstance();
  auto& state = system.GetDVDInterfaceState().GetData();

  // Actual games always set this to 48 KHz
  // but let's make sure to use GetAISSampleRateDivisor()
  // just in case it changes to 32 KHz
  const u32 sample_rate_divisor = AudioInterface::GetAISSampleRateDivisor();

  // Determine which audio data to read next.

  // 3.5 ms of samples
  const u32 maximum_samples =
      ((Mixer::FIXED_SAMPLE_RATE_DIVIDEND / 2000) * 7) / sample_rate_divisor;
  u64 read_offset = 0;
  u32 read_length = 0;

  if (interrupt_type == DIInterruptType::TCINT)
  {
    // Send audio to the mixer.
    std::vector<s16> temp_pcm(state.pending_samples * 2, 0);
    ProcessDTKSamples(&temp_pcm, audio_data);

    SoundStream* sound_stream = system.GetSoundStream();
    sound_stream->GetMixer()->PushStreamingSamples(temp_pcm.data(), state.pending_samples);

    if (state.stream && AudioInterface::IsPlaying())
    {
      read_offset = state.audio_position;
      read_length = AdvanceDTK(maximum_samples, &state.pending_samples);
    }
    else
    {
      read_length = 0;
      state.pending_samples = maximum_samples;
    }
  }
  else
  {
    read_length = 0;
    state.pending_samples = maximum_samples;
  }

  // Read the next chunk of audio data asynchronously.
  s64 ticks_to_dtk = SystemTimers::GetTicksPerSecond() * s64(state.pending_samples) *
                     sample_rate_divisor / Mixer::FIXED_SAMPLE_RATE_DIVIDEND;
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
    system.GetCoreTiming().ScheduleEvent(ticks_to_dtk, state.finish_executing_command, userdata);
  }
}

void Init()
{
  ASSERT(!IsDiscInside());

  DVDThread::Start();

  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  auto& state = system.GetDVDInterfaceState().GetData();

  state.DISR.Hex = 0;
  state.DICVR.Hex = 1;  // Disc Channel relies on cover being open when no disc is inserted
  state.DICMDBUF[0] = 0;
  state.DICMDBUF[1] = 0;
  state.DICMDBUF[2] = 0;
  state.DIMAR = 0;
  state.DILENGTH = 0;
  state.DICR.Hex = 0;
  state.DIIMMBUF = 0;
  state.DICFG.Hex = 0;
  state.DICFG.CONFIG = 1;  // Disable bootrom descrambler

  ResetDrive(false);

  state.auto_change_disc = core_timing.RegisterEvent("AutoChangeDisc", AutoChangeDiscCallback);
  state.eject_disc = core_timing.RegisterEvent("EjectDisc", EjectDiscCallback);
  state.insert_disc = core_timing.RegisterEvent("InsertDisc", InsertDiscCallback);

  state.finish_executing_command =
      core_timing.RegisterEvent("FinishExecutingCommand", FinishExecutingCommandCallback);

  u64 userdata = PackFinishExecutingCommandUserdata(ReplyType::DTK, DIInterruptType::TCINT);
  core_timing.ScheduleEvent(0, state.finish_executing_command, userdata);
}

// Resets state on the MN102 chip in the drive itself, but not the DI registers exposed on the
// emulated device, or any inserted disc.
void ResetDrive(bool spinup)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  state.stream = false;
  state.stop_at_track_end = false;
  state.audio_position = 0;
  state.next_start = 0;
  state.next_length = 0;
  state.current_start = 0;
  state.current_length = 0;
  state.pending_samples = 0;
  state.enable_dtk = false;
  state.dtk_buffer_length = 0;

  if (!IsDiscInside())
  {
    // CoverOpened is used when the cover is open;
    // NoMediumPresent is used when the cover is closed but there is no disc.
    // On the Wii, this can only happen if something other than a DVD is inserted into the disc
    // drive (for instance, an audio CD) and only after it attempts to read it.  Otherwise, it will
    // report the cover as opened.
    SetDriveState(DriveState::CoverOpened);
  }
  else if (!spinup)
  {
    // Wii hardware tests indicate that this is used when ejecting and inserting a new disc, or
    // performing a reset without spinup.
    SetDriveState(DriveState::DiscChangeDetected);
  }
  else
  {
    SetDriveState(DriveState::DiscIdNotRead);
  }

  SetDriveError(DriveError::None);

  // The buffer is empty at start
  state.read_buffer_start_offset = 0;
  state.read_buffer_end_offset = 0;
  state.read_buffer_start_time = 0;
  state.read_buffer_end_time = 0;
}

void Shutdown()
{
  DVDThread::Stop();
}

static u64 GetDiscEndOffset(const DiscIO::VolumeDisc& disc)
{
  u64 size = disc.GetDataSize();

  if (disc.GetDataSizeType() == DiscIO::DataSizeType::Accurate)
  {
    if (size == DiscIO::MINI_DVD_SIZE)
      return DiscIO::MINI_DVD_SIZE;
  }
  else
  {
    size = DiscIO::GetBiggestReferencedOffset(disc);
  }

  const bool should_be_mini_dvd =
      disc.GetVolumeType() == DiscIO::Platform::GameCubeDisc || disc.IsDatelDisc();

  // We always return standard DVD sizes here, not DVD-R sizes.
  // RVT-R (devkit) consoles can't read the extra megabytes there are on RVT-R (DVD-R) discs.
  if (should_be_mini_dvd && size <= DiscIO::MINI_DVD_SIZE)
    return DiscIO::MINI_DVD_SIZE;
  else if (size <= DiscIO::SL_DVD_R_SIZE)
    return DiscIO::SL_DVD_SIZE;
  else
    return DiscIO::DL_DVD_SIZE;
}

void SetDisc(std::unique_ptr<DiscIO::VolumeDisc> disc,
             std::optional<std::vector<std::string>> auto_disc_change_paths = {})
{
  bool had_disc = IsDiscInside();
  bool has_disc = static_cast<bool>(disc);

  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  if (has_disc)
  {
    state.disc_end_offset = GetDiscEndOffset(*disc);
    if (disc->GetDataSizeType() != DiscIO::DataSizeType::Accurate)
      WARN_LOG_FMT(DVDINTERFACE, "Unknown disc size, guessing {0} bytes", state.disc_end_offset);

    const DiscIO::BlobReader& blob = disc->GetBlobReader();

    // DirectoryBlobs (including Riivolution-patched discs) may end up larger than a real physical
    // Wii disc, which triggers Error #001. In those cases we manually make the check succeed to
    // avoid problems.
    const bool should_fake_error_001 =
        SConfig::GetInstance().bWii && blob.GetBlobType() == DiscIO::BlobType::DIRECTORY;
    Config::SetCurrent(Config::SESSION_SHOULD_FAKE_ERROR_001, should_fake_error_001);

    if (!blob.HasFastRandomAccessInBlock() && blob.GetBlockSize() > 0x200000)
    {
      OSD::AddMessage("You are running a disc image with a very large block size.", 60000);
      OSD::AddMessage("This will likely lead to performance problems.", 60000);
      OSD::AddMessage("You can use Dolphin's convert feature to reduce the block size.", 60000);
    }
  }

  if (auto_disc_change_paths)
  {
    ASSERT_MSG(DISCIO, (*auto_disc_change_paths).size() != 1,
               "Cannot automatically change between one disc");

    state.auto_disc_change_paths = *auto_disc_change_paths;
    state.auto_disc_change_index = 0;
  }

  // Assume that inserting a disc requires having an empty disc before
  if (had_disc != has_disc)
    ExpansionInterface::g_rtc_flags[ExpansionInterface::RTCFlag::DiscChanged] = true;

  DVDThread::SetDisc(std::move(disc));
  SetLidOpen();

  ResetDrive(false);
}

bool IsDiscInside()
{
  return DVDThread::HasDisc();
}

static void AutoChangeDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  AutoChangeDisc();
}

static void EjectDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  SetDisc(nullptr, {});
}

static void InsertDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  auto& state = system.GetDVDInterfaceState().GetData();
  std::unique_ptr<DiscIO::VolumeDisc> new_disc = DiscIO::CreateDisc(state.disc_path_to_insert);

  if (new_disc)
    SetDisc(std::move(new_disc), {});
  else
    PanicAlertFmtT("The disc that was about to be inserted couldn't be found.");

  state.disc_path_to_insert.clear();
}

// Must only be called on the CPU thread
void EjectDisc(EjectCause cause)
{
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  auto& state = system.GetDVDInterfaceState().GetData();
  core_timing.ScheduleEvent(0, state.eject_disc);
  if (cause == EjectCause::User)
    ExpansionInterface::g_rtc_flags[ExpansionInterface::RTCFlag::EjectButton] = true;
}

// Must only be called on the CPU thread
void ChangeDisc(const std::vector<std::string>& paths)
{
  ASSERT_MSG(DISCIO, !paths.empty(), "Trying to insert an empty list of discs");

  if (paths.size() > 1)
  {
    auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
    state.auto_disc_change_paths = paths;
    state.auto_disc_change_index = 0;
  }

  ChangeDisc(paths[0]);
}

// Must only be called on the CPU thread
void ChangeDisc(const std::string& new_path)
{
  auto& system = Core::System::GetInstance();
  auto& state = system.GetDVDInterfaceState().GetData();
  if (!state.disc_path_to_insert.empty())
  {
    PanicAlertFmtT("A disc is already about to be inserted.");
    return;
  }

  EjectDisc(EjectCause::User);

  state.disc_path_to_insert = new_path;
  system.GetCoreTiming().ScheduleEvent(SystemTimers::GetTicksPerSecond(), state.insert_disc);
  Movie::SignalDiscChange(new_path);

  for (size_t i = 0; i < state.auto_disc_change_paths.size(); ++i)
  {
    if (state.auto_disc_change_paths[i] == new_path)
    {
      state.auto_disc_change_index = i;
      return;
    }
  }

  state.auto_disc_change_paths.clear();
}

// Must only be called on the CPU thread
bool AutoChangeDisc()
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  if (state.auto_disc_change_paths.empty())
    return false;

  state.auto_disc_change_index =
      (state.auto_disc_change_index + 1) % state.auto_disc_change_paths.size();
  ChangeDisc(state.auto_disc_change_paths[state.auto_disc_change_index]);
  return true;
}

static void SetLidOpen()
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  const u32 old_value = state.DICVR.CVR;
  state.DICVR.CVR = IsDiscInside() ? 0 : 1;
  if (state.DICVR.CVR != old_value)
    GenerateDIInterrupt(DIInterruptType::CVRINT);
}

bool UpdateRunningGameMetadata(std::optional<u64> title_id)
{
  if (!DVDThread::HasDisc())
    return false;

  return DVDThread::UpdateRunningGameMetadata(IOS::HLE::DIDevice::GetCurrentPartition(), title_id);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base, bool is_wii)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  mmio->Register(base | DI_STATUS_REGISTER, MMIO::DirectRead<u32>(&state.DISR.Hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& state = system.GetDVDInterfaceState().GetData();
                   const UDISR tmp_status_reg(val);

                   state.DISR.DEINTMASK = tmp_status_reg.DEINTMASK.Value();
                   state.DISR.TCINTMASK = tmp_status_reg.TCINTMASK.Value();
                   state.DISR.BRKINTMASK = tmp_status_reg.BRKINTMASK.Value();
                   state.DISR.BREAK = tmp_status_reg.BREAK.Value();

                   if (tmp_status_reg.DEINT)
                     state.DISR.DEINT = 0;

                   if (tmp_status_reg.TCINT)
                     state.DISR.TCINT = 0;

                   if (tmp_status_reg.BRKINT)
                     state.DISR.BRKINT = 0;

                   if (state.DISR.BREAK)
                   {
                     DEBUG_ASSERT(0);
                   }

                   UpdateInterrupts();
                 }));

  mmio->Register(base | DI_COVER_REGISTER, MMIO::DirectRead<u32>(&state.DICVR.Hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& state = system.GetDVDInterfaceState().GetData();
                   const UDICVR tmp_cover_reg(val);

                   state.DICVR.CVRINTMASK = tmp_cover_reg.CVRINTMASK.Value();

                   if (tmp_cover_reg.CVRINT)
                     state.DICVR.CVRINT = 0;

                   UpdateInterrupts();
                 }));

  // Command registers, which have no special logic
  mmio->Register(base | DI_COMMAND_0, MMIO::DirectRead<u32>(&state.DICMDBUF[0]),
                 MMIO::DirectWrite<u32>(&state.DICMDBUF[0]));
  mmio->Register(base | DI_COMMAND_1, MMIO::DirectRead<u32>(&state.DICMDBUF[1]),
                 MMIO::DirectWrite<u32>(&state.DICMDBUF[1]));
  mmio->Register(base | DI_COMMAND_2, MMIO::DirectRead<u32>(&state.DICMDBUF[2]),
                 MMIO::DirectWrite<u32>(&state.DICMDBUF[2]));

  // DMA related registers. Mostly direct accesses (+ masking for writes to
  // handle things like address alignment) and complex write on the DMA
  // control register that will trigger the DMA.

  // The DMA address register masks away the top and bottom bits on GameCube, but only the top bits
  // on Wii (which can be observed by reading back the register; this difference probably exists due
  // to the existence of MEM2). The behavior of GameCube mode on a Wii (via MIOS/booting form the
  // system menu) has not been tested yet. Note that RegisterMMIO does not get re-called when
  // switching to GameCube mode; we handle this difference by applying the masking when using the
  // GameCube's DI MMIO address (0x0C006000) but not applying it when using the Wii's DI MMIO
  // address (0x0D006000), although we allow writes to both of these addresses if Dolphin was
  // started in Wii mode. (Also, normally in Wii mode the DI MMIOs are only written by the
  // IOS /dev/di module, but we *do* emulate /dev/di writing the DI MMIOs.)
  mmio->Register(base | DI_DMA_ADDRESS_REGISTER, MMIO::DirectRead<u32>(&state.DIMAR),
                 MMIO::DirectWrite<u32>(&state.DIMAR, is_wii ? ~0x1F : ~0xFC00001F));
  mmio->Register(base | DI_DMA_LENGTH_REGISTER, MMIO::DirectRead<u32>(&state.DILENGTH),
                 MMIO::DirectWrite<u32>(&state.DILENGTH, ~0x1F));
  mmio->Register(base | DI_DMA_CONTROL_REGISTER, MMIO::DirectRead<u32>(&state.DICR.Hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& state = system.GetDVDInterfaceState().GetData();
                   state.DICR.Hex = val & 7;
                   if (state.DICR.TSTART)
                   {
                     ExecuteCommand(ReplyType::Interrupt);
                   }
                 }));

  mmio->Register(base | DI_IMMEDIATE_DATA_BUFFER, MMIO::DirectRead<u32>(&state.DIIMMBUF),
                 MMIO::DirectWrite<u32>(&state.DIIMMBUF));

  // DI config register is read only.
  mmio->Register(base | DI_CONFIG_REGISTER, MMIO::DirectRead<u32>(&state.DICFG.Hex),
                 MMIO::InvalidWrite<u32>());
}

static void UpdateInterrupts()
{
  auto& system = Core::System::GetInstance();
  auto& state = system.GetDVDInterfaceState().GetData();
  const bool set_mask = (state.DISR.DEINT & state.DISR.DEINTMASK) != 0 ||
                        (state.DISR.TCINT & state.DISR.TCINTMASK) != 0 ||
                        (state.DISR.BRKINT & state.DISR.BRKINTMASK) != 0 ||
                        (state.DICVR.CVRINT & state.DICVR.CVRINTMASK) != 0;

  ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DI, set_mask);

  // Required for Summoner: A Goddess Reborn
  system.GetCoreTiming().ForceExceptionCheck(50);
}

static void GenerateDIInterrupt(DIInterruptType dvd_interrupt)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  switch (dvd_interrupt)
  {
  case DIInterruptType::DEINT:
    state.DISR.DEINT = true;
    break;
  case DIInterruptType::TCINT:
    state.DISR.TCINT = true;
    break;
  case DIInterruptType::BRKINT:
    state.DISR.BRKINT = true;
    break;
  case DIInterruptType::CVRINT:
    state.DICVR.CVRINT = true;
    break;
  }

  UpdateInterrupts();
}

void SetInterruptEnabled(DIInterruptType interrupt, bool enabled)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  switch (interrupt)
  {
  case DIInterruptType::DEINT:
    state.DISR.DEINTMASK = enabled;
    break;
  case DIInterruptType::TCINT:
    state.DISR.TCINTMASK = enabled;
    break;
  case DIInterruptType::BRKINT:
    state.DISR.BRKINTMASK = enabled;
    break;
  case DIInterruptType::CVRINT:
    state.DICVR.CVRINTMASK = enabled;
    break;
  }
}

void ClearInterrupt(DIInterruptType interrupt)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  switch (interrupt)
  {
  case DIInterruptType::DEINT:
    state.DISR.DEINT = false;
    break;
  case DIInterruptType::TCINT:
    state.DISR.TCINT = false;
    break;
  case DIInterruptType::BRKINT:
    state.DISR.BRKINT = false;
    break;
  case DIInterruptType::CVRINT:
    state.DICVR.CVRINT = false;
    break;
  }
}

// Checks the drive state to make sure a read-like command can be performed.
// If false is returned, SetDriveError will have been called, and the caller
// should issue a DEINT interrupt.
static bool CheckReadPreconditions()
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  if (!IsDiscInside())  // Implies CoverOpened or NoMediumPresent
  {
    ERROR_LOG_FMT(DVDINTERFACE, "No disc inside.");
    SetDriveError(DriveError::MediumNotPresent);
    return false;
  }
  if (state.drive_state == DriveState::DiscChangeDetected)
  {
    ERROR_LOG_FMT(DVDINTERFACE, "Disc changed (motor stopped).");
    SetDriveError(DriveError::MediumChanged);
    return false;
  }
  if (state.drive_state == DriveState::MotorStopped)
  {
    ERROR_LOG_FMT(DVDINTERFACE, "Motor stopped.");
    SetDriveError(DriveError::MotorStopped);
    return false;
  }
  if (state.drive_state == DriveState::DiscIdNotRead)
  {
    ERROR_LOG_FMT(DVDINTERFACE, "Disc id not read.");
    SetDriveError(DriveError::NoDiscID);
    return false;
  }
  return true;
}

// Iff false is returned, ScheduleEvent must be used to finish executing the command
static bool ExecuteReadCommand(u64 dvd_offset, u32 output_address, u32 dvd_length,
                               u32 output_length, const DiscIO::Partition& partition,
                               ReplyType reply_type, DIInterruptType* interrupt_type)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
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
    WARN_LOG_FMT(DVDINTERFACE, "Detected an attempt to read more data from the DVD "
                               "than what fits inside the out buffer. Clamping.");
    dvd_length = output_length;
  }

  // Many Wii games intentionally try to read from an offset which is just past the end of a
  // regular DVD but just before the end of a DVD-R, displaying "Error #001" and failing to boot
  // if the read succeeds, so it's critical that we set the correct error code for such reads.
  // See https://wiibrew.org/wiki//dev/di#0x8D_DVDLowUnencryptedRead for details on Error #001.
  if (dvd_offset + dvd_length > state.disc_end_offset)
  {
    SetDriveError(DriveError::BlockOOB);
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
  auto& system = Core::System::GetInstance();
  auto& state = system.GetDVDInterfaceState().GetData();
  DIInterruptType interrupt_type = DIInterruptType::TCINT;
  bool command_handled_by_thread = false;

  // DVDLowRequestError needs access to the error code set by the previous command
  if (static_cast<DICommand>(state.DICMDBUF[0] >> 24) != DICommand::RequestError)
    SetDriveError(DriveError::None);

  switch (static_cast<DICommand>(state.DICMDBUF[0] >> 24))
  {
  // Used by both GC and Wii
  case DICommand::Inquiry:
  {
    // (shuffle2) Taken from my Wii
    auto& memory = system.GetMemory();
    memory.Write_U32(0x00000002, state.DIMAR);      // Revision level, device code
    memory.Write_U32(0x20060526, state.DIMAR + 4);  // Release date
    memory.Write_U32(0x41000000, state.DIMAR + 8);  // Version

    INFO_LOG_FMT(DVDINTERFACE, "DVDLowInquiry (Buffer {:#010x}, {:#x})", state.DIMAR,
                 state.DILENGTH);
    break;
  }
  // GC-only patched drive firmware command, used by libogc
  case DICommand::Unknown55:
    INFO_LOG_FMT(DVDINTERFACE, "SetExtension");
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // Wii-exclusive
  case DICommand::ReportKey:
    INFO_LOG_FMT(DVDINTERFACE, "DVDLowReportKey");
    // Does not work on retail discs/drives
    // Retail games send this command to see if they are running on real retail hw
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // DMA Read from Disc. Only used through direct access on GC; direct use is prohibited by
  // IOS (which uses it internally)
  case DICommand::Read:
    switch (state.DICMDBUF[0] & 0xFF)
    {
    case 0x00:  // Read Sector
    {
      const u64 dvd_offset = static_cast<u64>(state.DICMDBUF[1]) << 2;

      INFO_LOG_FMT(
          DVDINTERFACE,
          "Read: DVDOffset={:08x}, DMABuffer = {:08x}, SrcLength = {:08x}, DMALength = {:08x}",
          dvd_offset, state.DIMAR, state.DICMDBUF[2], state.DILENGTH);

      if (state.drive_state == DriveState::ReadyNoReadsMade)
        SetDriveState(DriveState::Ready);

      command_handled_by_thread =
          ExecuteReadCommand(dvd_offset, state.DIMAR, state.DICMDBUF[2], state.DILENGTH,
                             DiscIO::PARTITION_NONE, reply_type, &interrupt_type);
    }
    break;

    case 0x40:  // Read DiscID
      INFO_LOG_FMT(DVDINTERFACE, "Read DiscID: buffer {:08x}", state.DIMAR);
      if (state.drive_state == DriveState::DiscIdNotRead)
      {
        SetDriveState(DriveState::ReadyNoReadsMade);
      }
      else if (state.drive_state == DriveState::ReadyNoReadsMade)
      {
        // The first disc ID reading is required before DTK can be configured.
        // If the disc ID is read again (or any other read occurs), it no longer can
        // be configured.
        SetDriveState(DriveState::Ready);
      }

      command_handled_by_thread =
          ExecuteReadCommand(0, state.DIMAR, 0x20, state.DILENGTH, DiscIO::PARTITION_NONE,
                             reply_type, &interrupt_type);
      break;

    default:
      ERROR_LOG_FMT(DVDINTERFACE, "Unknown read subcommand: {:08x}", state.DICMDBUF[0]);
      break;
    }
    break;

  // Used by both GC and Wii
  case DICommand::Seek:
    // Currently unimplemented
    INFO_LOG_FMT(DVDINTERFACE, "Seek: offset={:09x} (ignoring)",
                 static_cast<u64>(state.DICMDBUF[1]) << 2);
    break;

  // Wii-exclusive
  case DICommand::ReadDVDMetadata:
    switch ((state.DICMDBUF[0] >> 16) & 0xFF)
    {
    case 0:
      ERROR_LOG_FMT(DVDINTERFACE, "DVDLowReadDvdPhysical");
      break;
    case 1:
      ERROR_LOG_FMT(DVDINTERFACE, "DVDLowReadDvdCopyright");
      break;
    case 2:
      ERROR_LOG_FMT(DVDINTERFACE, "DVDLowReadDvdDiscKey");
      break;
    default:
      ERROR_LOG_FMT(DVDINTERFACE, "Unknown 0xAD subcommand in {:08x}", state.DICMDBUF[0]);
      break;
    }
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::ReadDVD:
    ERROR_LOG_FMT(DVDINTERFACE, "DVDLowReadDvd");
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::ReadDVDConfig:
    ERROR_LOG_FMT(DVDINTERFACE, "DVDLowReadDvdConfig");
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::StopLaser:
    ERROR_LOG_FMT(DVDINTERFACE, "DVDLowStopLaser");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_STOP_LASER);
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::Offset:
    ERROR_LOG_FMT(DVDINTERFACE, "DVDLowOffset");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_OFFSET);
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::ReadBCA:
  {
    WARN_LOG_FMT(DVDINTERFACE, "DVDLowReadDiskBca - supplying dummy data to appease NSMBW");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_READ_DISK_BCA);
    // NSMBW checks that the first 0x33 bytes of the BCA are 0, then it expects a 1.
    // Most (all?) other games have 0x34 0's at the start of the BCA, but don't actually
    // read it.  NSMBW doesn't care about the other 12 bytes (which contain manufacturing data?)

    auto& memory = system.GetMemory();
    // TODO: Read the .bca file that cleanrip generates, if it exists
    // memory.CopyToEmu(output_address, bca_data, 0x40);
    memory.Memset(state.DIMAR, 0, 0x40);
    memory.Write_U8(1, state.DIMAR + 0x33);
    break;
  }
  // Wii-exclusive
  case DICommand::RequestDiscStatus:
    ERROR_LOG_FMT(DVDINTERFACE, "DVDLowRequestDiscStatus");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_REQUEST_DISC_STATUS);
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::RequestRetryNumber:
    ERROR_LOG_FMT(DVDINTERFACE, "DVDLowRequestRetryNumber");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_REQUEST_RETRY_NUMBER);
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::SetMaximumRotation:
    ERROR_LOG_FMT(DVDINTERFACE, "DVDLowSetMaximumRotation");
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  // Wii-exclusive
  case DICommand::SerMeasControl:
    ERROR_LOG_FMT(DVDINTERFACE, "DVDLowSerMeasControl");
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_DVD_LOW_SER_MEAS_CONTROL);
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // Used by both GC and Wii
  case DICommand::RequestError:
  {
    u32 drive_state;
    if (state.drive_state == DriveState::Ready)
      drive_state = 0;
    else
      drive_state = static_cast<u32>(state.drive_state) - 1;

    const u32 result = (drive_state << 24) | static_cast<u32>(state.error_code);
    INFO_LOG_FMT(DVDINTERFACE, "Requesting error... ({:#010x})", result);
    state.DIIMMBUF = result;
    SetDriveError(DriveError::None);
    break;
  }

  // Audio Stream (Immediate). Only used by some GC games, but does exist on the Wii
  // (command_0 >> 16) & 0xFF = Subcommand
  // command_1 << 2           = Offset on disc
  // command_2                = Length of the stream
  case DICommand::AudioStream:
  {
    if (!CheckReadPreconditions())
    {
      ERROR_LOG_FMT(DVDINTERFACE, "Cannot play audio (command {:08x})", state.DICMDBUF[0]);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }
    if (!state.enable_dtk)
    {
      ERROR_LOG_FMT(
          DVDINTERFACE,
          "Attempted to change playing audio while audio is disabled!  ({:08x} {:08x} {:08x})",
          state.DICMDBUF[0], state.DICMDBUF[1], state.DICMDBUF[2]);
      SetDriveError(DriveError::NoAudioBuf);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    if (state.drive_state == DriveState::ReadyNoReadsMade)
      SetDriveState(DriveState::Ready);

    switch ((state.DICMDBUF[0] >> 16) & 0xFF)
    {
    case 0x00:
    {
      const u64 offset = static_cast<u64>(state.DICMDBUF[1]) << 2;
      const u32 length = state.DICMDBUF[2];
      INFO_LOG_FMT(DVDINTERFACE, "(Audio) Start stream: offset: {:08x} length: {:08x}", offset,
                   length);

      if ((offset == 0) && (length == 0))
      {
        state.stop_at_track_end = true;
      }
      else if (!state.stop_at_track_end)
      {
        state.next_start = offset;
        state.next_length = length;
        if (!state.stream)
        {
          state.current_start = state.next_start;
          state.current_length = state.next_length;
          state.audio_position = state.current_start;
          state.adpcm_decoder.ResetFilter();
          state.stream = true;
        }
      }
      break;
    }
    case 0x01:
      INFO_LOG_FMT(DVDINTERFACE, "(Audio) Stop stream");
      state.stop_at_track_end = false;
      state.stream = false;
      break;
    default:
      ERROR_LOG_FMT(DVDINTERFACE, "Invalid audio command!  ({:08x} {:08x} {:08x})",
                    state.DICMDBUF[0], state.DICMDBUF[1], state.DICMDBUF[2]);
      SetDriveError(DriveError::InvalidAudioCommand);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }
  }
  break;

  // Request Audio Status (Immediate). Only used by some GC games, but does exist on the Wii
  case DICommand::RequestAudioStatus:
  {
    if (!CheckReadPreconditions())
    {
      ERROR_LOG_FMT(DVDINTERFACE, "Attempted to request audio status in an invalid state!");
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    if (!state.enable_dtk)
    {
      ERROR_LOG_FMT(DVDINTERFACE, "Attempted to request audio status while audio is disabled!");
      SetDriveError(DriveError::NoAudioBuf);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    switch (state.DICMDBUF[0] >> 16 & 0xFF)
    {
    case 0x00:  // Returns streaming status
      INFO_LOG_FMT(DVDINTERFACE,
                   "(Audio): Stream Status: Request Audio status "
                   "AudioPos:{:08x}/{:08x} "
                   "CurrentStart:{:08x} CurrentLength:{:08x}",
                   state.audio_position, state.current_start + state.current_length,
                   state.current_start, state.current_length);
      state.DIIMMBUF = (state.stream ? 1 : 0);
      break;
    case 0x01:  // Returns the current offset
      INFO_LOG_FMT(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:{:08x}",
                   state.audio_position);
      state.DIIMMBUF = static_cast<u32>((state.audio_position & 0xffffffffffff8000ull) >> 2);
      break;
    case 0x02:  // Returns the start offset
      INFO_LOG_FMT(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentStart:{:08x}",
                   state.current_start);
      state.DIIMMBUF = static_cast<u32>(state.current_start >> 2);
      break;
    case 0x03:  // Returns the total length
      INFO_LOG_FMT(DVDINTERFACE,
                   "(Audio): Stream Status: Request Audio status CurrentLength:{:08x}",
                   state.current_length);
      state.DIIMMBUF = state.current_length;
      break;
    default:
      ERROR_LOG_FMT(DVDINTERFACE, "Invalid audio status command!  ({:08x} {:08x} {:08x})",
                    state.DICMDBUF[0], state.DICMDBUF[1], state.DICMDBUF[2]);
      SetDriveError(DriveError::InvalidAudioCommand);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }
  }
  break;

  // Used by both GC and Wii
  case DICommand::StopMotor:
  {
    const bool eject = (state.DICMDBUF[0] & (1 << 17));
    const bool kill = (state.DICMDBUF[0] & (1 << 20));
    INFO_LOG_FMT(DVDINTERFACE, "DVDLowStopMotor{}{}", eject ? " eject" : "", kill ? " kill!" : "");

    if (state.drive_state == DriveState::Ready ||
        state.drive_state == DriveState::ReadyNoReadsMade ||
        state.drive_state == DriveState::DiscIdNotRead)
    {
      SetDriveState(DriveState::MotorStopped);
    }

    const bool force_eject = eject && !kill;

    if (Config::Get(Config::MAIN_AUTO_DISC_CHANGE) && !Movie::IsPlayingInput() &&
        DVDThread::IsInsertedDiscRunning() && !state.auto_disc_change_paths.empty())
    {
      system.GetCoreTiming().ScheduleEvent(force_eject ? 0 : SystemTimers::GetTicksPerSecond() / 2,
                                           state.auto_change_disc);
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
    // reads. Too early, and you get NoDiscID.  Too late, and you get InvalidPeriod.
    if (!CheckReadPreconditions())
    {
      ERROR_LOG_FMT(DVDINTERFACE, "Attempted to change DTK configuration in an invalid state!");
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    if (state.drive_state == DriveState::Ready)
    {
      ERROR_LOG_FMT(DVDINTERFACE,
                    "Attempted to change DTK configuration after a read has been made!");
      SetDriveError(DriveError::InvalidPeriod);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    // Note that this can be called multiple times, as long as the drive is in the ReadyNoReadsMade
    // state. Calling it does not exit that state.
    AudioBufferConfig((state.DICMDBUF[0] >> 16) & 1, state.DICMDBUF[0] & 0xf);
    break;

  // GC-only patched drive firmware command, used by libogc
  case DICommand::UnknownEE:
    INFO_LOG_FMT(DVDINTERFACE, "SetStatus");
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // Debug commands; see yagcd. We don't really care
  // NOTE: commands to stream data will send...a raw data stream
  // This will appear as unknown commands, unless the check is re-instated to catch such data.
  // Can only be used through direct access and only after unlocked.
  case DICommand::Debug:
    ERROR_LOG_FMT(DVDINTERFACE, "Unsupported DVD Drive debug command {:#010x}", state.DICMDBUF[0]);
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // Unlock Commands. 1: "MATSHITA" 2: "DVD-GAME"
  // Just for fun
  // Can only be used through direct access.  The unlock command doesn't seem to work on the Wii.
  case DICommand::DebugUnlock:
  {
    if (state.DICMDBUF[0] == 0xFF014D41 && state.DICMDBUF[1] == 0x54534849 &&
        state.DICMDBUF[2] == 0x54410200)
    {
      INFO_LOG_FMT(DVDINTERFACE, "Unlock test 1 passed");
    }
    else if (state.DICMDBUF[0] == 0xFF004456 && state.DICMDBUF[1] == 0x442D4741 &&
             state.DICMDBUF[2] == 0x4D450300)
    {
      INFO_LOG_FMT(DVDINTERFACE, "Unlock test 2 passed");
    }
    else
    {
      INFO_LOG_FMT(DVDINTERFACE, "Unlock test failed");
    }
  }
  break;

  default:
    ERROR_LOG_FMT(DVDINTERFACE, "Unknown command {:#010x} (Buffer {:#010x}, {:#x})",
                  state.DICMDBUF[0], state.DIMAR, state.DILENGTH);
    PanicAlertFmtT("Unknown DVD command {0:08x} - fatal error", state.DICMDBUF[0]);
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  }

  if (!command_handled_by_thread)
  {
    // TODO: Needs testing to determine if MINIMUM_COMMAND_LATENCY_US is accurate for this
    system.GetCoreTiming().ScheduleEvent(
        MINIMUM_COMMAND_LATENCY_US * (SystemTimers::GetTicksPerSecond() / 1000000),
        state.finish_executing_command,
        PackFinishExecutingCommandUserdata(reply_type, interrupt_type));
  }
}

void PerformDecryptingRead(u32 position, u32 length, u32 output_address,
                           const DiscIO::Partition& partition, ReplyType reply_type)
{
  auto& system = Core::System::GetInstance();
  auto& state = system.GetDVDInterfaceState().GetData();
  DIInterruptType interrupt_type = DIInterruptType::TCINT;

  if (state.drive_state == DriveState::ReadyNoReadsMade)
    SetDriveState(DriveState::Ready);

  const bool command_handled_by_thread =
      ExecuteReadCommand(static_cast<u64>(position) << 2, output_address, length, length, partition,
                         reply_type, &interrupt_type);

  if (!command_handled_by_thread)
  {
    // TODO: Needs testing to determine if MINIMUM_COMMAND_LATENCY_US is accurate for this
    system.GetCoreTiming().ScheduleEvent(
        MINIMUM_COMMAND_LATENCY_US * (SystemTimers::GetTicksPerSecond() / 1000000),
        state.finish_executing_command,
        PackFinishExecutingCommandUserdata(reply_type, interrupt_type));
  }
}

void ForceOutOfBoundsRead(ReplyType reply_type)
{
  auto& system = Core::System::GetInstance();
  auto& state = system.GetDVDInterfaceState().GetData();
  INFO_LOG_FMT(DVDINTERFACE, "Forcing an out-of-bounds disc read.");

  if (state.drive_state == DriveState::ReadyNoReadsMade)
    SetDriveState(DriveState::Ready);

  SetDriveError(DriveError::BlockOOB);

  // TODO: Needs testing to determine if MINIMUM_COMMAND_LATENCY_US is accurate for this
  const DIInterruptType interrupt_type = DIInterruptType::DEINT;
  system.GetCoreTiming().ScheduleEvent(
      MINIMUM_COMMAND_LATENCY_US * (SystemTimers::GetTicksPerSecond() / 1000000),
      state.finish_executing_command,
      PackFinishExecutingCommandUserdata(reply_type, interrupt_type));
}

void AudioBufferConfig(bool enable_dtk, u8 dtk_buffer_length)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  state.enable_dtk = enable_dtk;
  state.dtk_buffer_length = dtk_buffer_length;
  if (state.enable_dtk)
    INFO_LOG_FMT(DVDINTERFACE, "DTK enabled: buffer size {}", state.dtk_buffer_length);
  else
    INFO_LOG_FMT(DVDINTERFACE, "DTK disabled");
}

static u64 PackFinishExecutingCommandUserdata(ReplyType reply_type, DIInterruptType interrupt_type)
{
  return (static_cast<u64>(reply_type) << 32) + static_cast<u32>(interrupt_type);
}

void FinishExecutingCommandCallback(Core::System& system, u64 userdata, s64 cycles_late)
{
  ReplyType reply_type = static_cast<ReplyType>(userdata >> 32);
  DIInterruptType interrupt_type = static_cast<DIInterruptType>(userdata & 0xFFFFFFFF);
  FinishExecutingCommand(reply_type, interrupt_type, cycles_late);
}

void SetDriveState(DriveState state)
{
  auto& dvd_interface_state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  dvd_interface_state.drive_state = state;
}

void SetDriveError(DriveError error)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();
  state.error_code = error;
}

void FinishExecutingCommand(ReplyType reply_type, DIInterruptType interrupt_type, s64 cycles_late,
                            const std::vector<u8>& data)
{
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();

  // The data parameter contains the requested data iff this was called from DVDThread, and is
  // empty otherwise. DVDThread is the only source of ReplyType::NoReply and ReplyType::DTK.

  u32 transfer_size = 0;
  if (reply_type == ReplyType::NoReply)
    transfer_size = static_cast<u32>(data.size());
  else if (reply_type == ReplyType::Interrupt || reply_type == ReplyType::IOS)
    transfer_size = state.DILENGTH;

  if (interrupt_type == DIInterruptType::TCINT)
  {
    state.DIMAR += transfer_size;
    state.DILENGTH -= transfer_size;
  }

  switch (reply_type)
  {
  case ReplyType::NoReply:
  {
    break;
  }

  case ReplyType::Interrupt:
  {
    if (state.DICR.TSTART)
    {
      state.DICR.TSTART = 0;
      GenerateDIInterrupt(interrupt_type);
    }
    break;
  }

  case ReplyType::IOS:
  {
    IOS::HLE::DIDevice::InterruptFromDVDInterface(interrupt_type);
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
static void ScheduleReads(u64 offset, u32 length, const DiscIO::Partition& partition,
                          u32 output_address, ReplyType reply_type)
{
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  auto& state = Core::System::GetInstance().GetDVDInterfaceState().GetData();

  // The drive continues to read 1 MiB beyond the last read position when idle.
  // If a future read falls within this window, part of the read may be returned
  // from the buffer. Data can be transferred from the buffer at up to 32 MiB/s.

  // Metroid Prime is a good example of a game that's sensitive to disc timing
  // details; if there isn't enough latency in the right places, doors can open
  // faster than on real hardware, and if there's too much latency in the wrong
  // places, the video before the save-file select screen lags.

  const u64 current_time = core_timing.GetTicks();
  const u32 ticks_per_second = SystemTimers::GetTicksPerSecond();
  const bool wii_disc = DVDThread::GetDiscType() == DiscIO::Platform::WiiDisc;

  // Whether we have performed a seek.
  bool seek = false;

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
  const u64 first_block = dvd_offset;

  if (Config::Get(Config::MAIN_FAST_DISC_SPEED))
  {
    // The SUDTR setting makes us act as if all reads are buffered
    buffer_start = std::numeric_limits<u64>::min();
    buffer_end = std::numeric_limits<u64>::max();
    head_position = 0;
  }
  else
  {
    if (state.read_buffer_start_time == state.read_buffer_end_time)
    {
      // No buffer
      buffer_start = buffer_end = head_position = 0;
    }
    else
    {
      buffer_start = state.read_buffer_end_offset > STREAMING_BUFFER_SIZE ?
                         state.read_buffer_end_offset - STREAMING_BUFFER_SIZE :
                         0;

      DEBUG_LOG_FMT(DVDINTERFACE, "Buffer: now={:#x} start time={:#x} end time={:#x}", current_time,
                    state.read_buffer_start_time, state.read_buffer_end_time);

      if (current_time >= state.read_buffer_end_time)
      {
        // Buffer is fully read
        buffer_end = state.read_buffer_end_offset;
      }
      else
      {
        // The amount of data the buffer contains *right now*, rounded to a DVD ECC block.
        buffer_end =
            state.read_buffer_start_offset +
            Common::AlignDown((current_time - state.read_buffer_start_time) *
                                  (state.read_buffer_end_offset - state.read_buffer_start_offset) /
                                  (state.read_buffer_end_time - state.read_buffer_start_time),
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

  DEBUG_LOG_FMT(DVDINTERFACE, "Buffer: start={:#x} end={:#x} avail={:#x}", buffer_start, buffer_end,
                buffer_end - buffer_start);

  DEBUG_LOG_FMT(DVDINTERFACE, "Schedule reads: offset={:#x} length={:#x} address={:#x}", offset,
                length, output_address);

  s64 ticks_until_completion =
      READ_COMMAND_LATENCY_US * (SystemTimers::GetTicksPerSecond() / 1000000);

  u32 buffered_blocks = 0;
  u32 unbuffered_blocks = 0;

  const u32 bytes_per_chunk = partition != DiscIO::PARTITION_NONE && DVDThread::HasWiiHashes() ?
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
        seek = true;
        ticks_until_completion += static_cast<u64>(
            ticks_per_second * DVDMath::CalculateSeekTime(head_position, dvd_offset));

        // TODO: The above emulates seeking and then reading one ECC block of data,
        // and then the below emulates the rotational latency. The rotational latency
        // should actually happen before reading data from the disc.

        const double time_after_seek =
            (core_timing.GetTicks() + ticks_until_completion) / ticks_per_second;
        ticks_until_completion += ticks_per_second * DVDMath::CalculateRotationalLatency(
                                                         dvd_offset, time_after_seek, wii_disc);

        DEBUG_LOG_FMT(DVDINTERFACE, "Seek+read {:#x} bytes @ {:#x} ticks={}", chunk_length, offset,
                      ticks_until_completion);
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

  // Evict blocks from the buffer which are unlikely to be used again after this read,
  // so that the buffer gets space for prefetching new blocks. Based on hardware testing,
  // the blocks which are kept are the most recently accessed block, the block immediately
  // before it, and all blocks after it.
  //
  // If the block immediately before the most recently accessed block is not kept, loading
  // screens in Pitfall: The Lost Expedition are longer than they should be.
  // https://bugs.dolphin-emu.org/issues/12279
  const u64 last_block = dvd_offset;
  constexpr u32 BUFFER_BACKWARD_SEEK_LIMIT = DVD_ECC_BLOCK_SIZE * 2;
  if (last_block - buffer_start <= BUFFER_BACKWARD_SEEK_LIMIT && buffer_start != buffer_end)
  {
    // Special case: reading the first two blocks of the buffer doesn't change the buffer state
  }
  else
  {
    // Note that the read_buffer_start_offset value we calculate here is not the
    // actual start of the buffer - it is just the start of the portion we need to read.
    // The actual start of the buffer is read_buffer_end_offset - STREAMING_BUFFER_SIZE.
    if (last_block >= buffer_end)
    {
      // Full buffer read
      state.read_buffer_start_offset = last_block;
    }
    else
    {
      // Partial buffer read
      state.read_buffer_start_offset = buffer_end;
    }

    state.read_buffer_end_offset = last_block + STREAMING_BUFFER_SIZE - BUFFER_BACKWARD_SEEK_LIMIT;
    if (seek)
    {
      // If we seek, the block preceding the first accessed block never gets read into the buffer
      state.read_buffer_end_offset =
          std::max(state.read_buffer_end_offset, first_block + STREAMING_BUFFER_SIZE);
    }

    // Assume the buffer starts prefetching new blocks right after the end of the last operation
    state.read_buffer_start_time = current_time + ticks_until_completion;
    state.read_buffer_end_time =
        state.read_buffer_start_time +
        static_cast<u64>(ticks_per_second *
                         DVDMath::CalculateRawDiscReadTime(state.read_buffer_start_offset,
                                                           state.read_buffer_end_offset -
                                                               state.read_buffer_start_offset,
                                                           wii_disc));
  }

  DEBUG_LOG_FMT(DVDINTERFACE,
                "Schedule reads: ECC blocks unbuffered={}, buffered={}, "
                "ticks={}, time={} us",
                unbuffered_blocks, buffered_blocks, ticks_until_completion,
                ticks_until_completion * 1000000 / SystemTimers::GetTicksPerSecond());
}

}  // namespace DVDInterface
