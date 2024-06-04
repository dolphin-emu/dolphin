// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DVD/DVDInterface.h"

#include <algorithm>
#include <array>
#include <cmath>
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

#include "Core/AchievementManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SessionSettings.h"
#include "Core/Core.h"
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

namespace DVD
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

DVDInterface::DVDInterface(Core::System& system) : m_system(system)
{
}

DVDInterface::~DVDInterface() = default;

static u64 PackFinishExecutingCommandUserdata(ReplyType reply_type, DIInterruptType interrupt_type);

void DVDInterface::DoState(PointerWrap& p)
{
  p.Do(m_DISR);
  p.Do(m_DICVR);
  p.DoArray(m_DICMDBUF);
  p.Do(m_DIMAR);
  p.Do(m_DILENGTH);
  p.Do(m_DICR);
  p.Do(m_DIIMMBUF);
  p.Do(m_DICFG);

  p.Do(m_stream);
  p.Do(m_stop_at_track_end);
  p.Do(m_audio_position);
  p.Do(m_current_start);
  p.Do(m_current_length);
  p.Do(m_next_start);
  p.Do(m_next_length);
  p.Do(m_pending_blocks);
  p.Do(m_enable_dtk);
  p.Do(m_dtk_buffer_length);

  p.Do(m_drive_state);
  p.Do(m_error_code);

  p.Do(m_read_buffer_start_time);
  p.Do(m_read_buffer_end_time);
  p.Do(m_read_buffer_start_offset);
  p.Do(m_read_buffer_end_offset);

  p.Do(m_disc_path_to_insert);

  m_system.GetDVDThread().DoState(p);

  m_adpcm_decoder.DoState(p);
}

size_t DVDInterface::ProcessDTKSamples(s16* target_samples, size_t target_block_count,
                                       const std::vector<u8>& audio_data)
{
  const size_t block_count_to_process =
      std::min(target_block_count, audio_data.size() / StreamADPCM::ONE_BLOCK_SIZE);
  size_t samples_processed = 0;
  size_t bytes_processed = 0;
  for (size_t i = 0; i < block_count_to_process; ++i)
  {
    m_adpcm_decoder.DecodeBlock(&target_samples[samples_processed * 2],
                                &audio_data[bytes_processed]);
    for (size_t j = 0; j < StreamADPCM::SAMPLES_PER_BLOCK * 2; ++j)
    {
      // TODO: Fix the mixer so it can accept non-byte-swapped samples.
      s16* sample = &target_samples[samples_processed * 2 + j];
      *sample = Common::swap16(*sample);
    }
    samples_processed += StreamADPCM::SAMPLES_PER_BLOCK;
    bytes_processed += StreamADPCM::ONE_BLOCK_SIZE;
  }
  return block_count_to_process;
}

u32 DVDInterface::AdvanceDTK(u32 maximum_blocks, u32* blocks_to_process)
{
  u32 bytes_to_process = 0;
  *blocks_to_process = 0;
  while (*blocks_to_process < maximum_blocks)
  {
    if (m_audio_position >= m_current_start + m_current_length)
    {
      DEBUG_LOG_FMT(DVDINTERFACE,
                    "AdvanceDTK: NextStart={:08x}, NextLength={:08x}, "
                    "CurrentStart={:08x}, CurrentLength={:08x}, AudioPos={:08x}",
                    m_next_start, m_next_length, m_current_start, m_current_length,
                    m_audio_position);

      m_audio_position = m_next_start;
      m_current_start = m_next_start;
      m_current_length = m_next_length;

      if (m_stop_at_track_end)
      {
        m_stop_at_track_end = false;
        m_stream = false;
        break;
      }

      m_adpcm_decoder.ResetFilter();
    }

    m_audio_position += StreamADPCM::ONE_BLOCK_SIZE;
    bytes_to_process += StreamADPCM::ONE_BLOCK_SIZE;
    *blocks_to_process += 1;
  }

  return bytes_to_process;
}

void DVDInterface::DTKStreamingCallback(DIInterruptType interrupt_type,
                                        const std::vector<u8>& audio_data, s64 cycles_late)
{
  auto& ai = m_system.GetAudioInterface();

  // Actual games always set this to 48 KHz
  // but let's make sure to use GetAISSampleRateDivisor()
  // just in case it changes to 32 KHz
  const auto sample_rate = ai.GetAISSampleRate();
  const u32 sample_rate_divisor = ai.GetAISSampleRateDivisor();

  // Determine which audio data to read next.

  // 3.5 ms of samples
  constexpr u32 MAX_POSSIBLE_BLOCKS = 6;
  constexpr u32 MAX_POSSIBLE_SAMPLES = MAX_POSSIBLE_BLOCKS * StreamADPCM::SAMPLES_PER_BLOCK;
  const u32 maximum_blocks = sample_rate == AudioInterface::SampleRate::AI32KHz ? 4 : 6;
  u64 read_offset = 0;
  u32 read_length = 0;

  if (interrupt_type == DIInterruptType::TCINT)
  {
    // Send audio to the mixer.
    std::array<s16, MAX_POSSIBLE_SAMPLES * 2> temp_pcm{};
    ASSERT(m_pending_blocks <= MAX_POSSIBLE_BLOCKS);
    const u32 pending_blocks = std::min(m_pending_blocks, MAX_POSSIBLE_BLOCKS);
    ProcessDTKSamples(temp_pcm.data(), pending_blocks, audio_data);

    SoundStream* sound_stream = m_system.GetSoundStream();
    sound_stream->GetMixer()->PushStreamingSamples(temp_pcm.data(),
                                                   pending_blocks * StreamADPCM::SAMPLES_PER_BLOCK);

    if (m_stream && ai.IsPlaying())
    {
      read_offset = m_audio_position;
      read_length = AdvanceDTK(maximum_blocks, &m_pending_blocks);
    }
    else
    {
      read_length = 0;
      m_pending_blocks = maximum_blocks;
    }
  }
  else
  {
    read_length = 0;
    m_pending_blocks = maximum_blocks;
  }

  // Read the next chunk of audio data asynchronously.
  s64 ticks_to_dtk = m_system.GetSystemTimers().GetTicksPerSecond() * s64(m_pending_blocks) *
                     StreamADPCM::SAMPLES_PER_BLOCK * sample_rate_divisor /
                     Mixer::FIXED_SAMPLE_RATE_DIVIDEND;
  ticks_to_dtk -= cycles_late;
  if (read_length > 0)
  {
    m_system.GetDVDThread().StartRead(read_offset, read_length, DiscIO::PARTITION_NONE,
                                      ReplyType::DTK, ticks_to_dtk);
  }
  else
  {
    // There's nothing to read, so using DVDThread is unnecessary.
    u64 userdata = PackFinishExecutingCommandUserdata(ReplyType::DTK, DIInterruptType::TCINT);
    m_system.GetCoreTiming().ScheduleEvent(ticks_to_dtk, m_finish_executing_command, userdata);
  }
}

void DVDInterface::Init()
{
  ASSERT(!IsDiscInside());

  m_system.GetDVDThread().Start();

  m_DISR.Hex = 0;
  m_DICVR.Hex = 1;  // Disc Channel relies on cover being open when no disc is inserted
  m_DICMDBUF[0] = 0;
  m_DICMDBUF[1] = 0;
  m_DICMDBUF[2] = 0;
  m_DIMAR = 0;
  m_DILENGTH = 0;
  m_DICR.Hex = 0;
  m_DIIMMBUF = 0;
  m_DICFG.Hex = 0;
  m_DICFG.CONFIG = 1;  // Disable bootrom descrambler

  ResetDrive(false);

  auto& core_timing = m_system.GetCoreTiming();
  m_auto_change_disc = core_timing.RegisterEvent("AutoChangeDisc", AutoChangeDiscCallback);
  m_eject_disc = core_timing.RegisterEvent("EjectDisc", EjectDiscCallback);
  m_insert_disc = core_timing.RegisterEvent("InsertDisc", InsertDiscCallback);

  m_finish_executing_command =
      core_timing.RegisterEvent("FinishExecutingCommand", FinishExecutingCommandCallback);

  u64 userdata = PackFinishExecutingCommandUserdata(ReplyType::DTK, DIInterruptType::TCINT);
  core_timing.ScheduleEvent(0, m_finish_executing_command, userdata);
}

// Resets state on the MN102 chip in the drive itself, but not the DI registers exposed on the
// emulated device, or any inserted disc.
void DVDInterface::ResetDrive(bool spinup)
{
  m_stream = false;
  m_stop_at_track_end = false;
  m_audio_position = 0;
  m_next_start = 0;
  m_next_length = 0;
  m_current_start = 0;
  m_current_length = 0;
  m_pending_blocks = 0;
  m_enable_dtk = false;
  m_dtk_buffer_length = 0;

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
  m_read_buffer_start_offset = 0;
  m_read_buffer_end_offset = 0;
  m_read_buffer_start_time = 0;
  m_read_buffer_end_time = 0;
}

void DVDInterface::Shutdown()
{
  m_system.GetDVDThread().Stop();
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

void DVDInterface::SetDisc(std::unique_ptr<DiscIO::VolumeDisc> disc,
                           std::optional<std::vector<std::string>> auto_disc_change_paths = {})
{
  bool had_disc = IsDiscInside();
  bool has_disc = static_cast<bool>(disc);

  if (has_disc)
  {
    m_disc_end_offset = GetDiscEndOffset(*disc);
    if (disc->GetDataSizeType() != DiscIO::DataSizeType::Accurate)
      WARN_LOG_FMT(DVDINTERFACE, "Unknown disc size, guessing {0} bytes", m_disc_end_offset);

    const DiscIO::BlobReader& blob = disc->GetBlobReader();

    // DirectoryBlobs (including Riivolution-patched discs) may end up larger than a real physical
    // Wii disc, which triggers Error #001. In those cases we manually make the check succeed to
    // avoid problems.
    const bool should_fake_error_001 =
        m_system.IsWii() && blob.GetBlobType() == DiscIO::BlobType::DIRECTORY;
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

    m_auto_disc_change_paths = *auto_disc_change_paths;
    m_auto_disc_change_index = 0;
  }

  AchievementManager::GetInstance().LoadGame("", disc.get());

  // Assume that inserting a disc requires having an empty disc before
  if (had_disc != has_disc)
    ExpansionInterface::g_rtc_flags[ExpansionInterface::RTCFlag::DiscChanged] = true;

  m_system.GetDVDThread().SetDisc(std::move(disc));
  SetLidOpen();

  ResetDrive(false);
}

bool DVDInterface::IsDiscInside() const
{
  return m_system.GetDVDThread().HasDisc();
}

void DVDInterface::AutoChangeDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  system.GetDVDInterface().AutoChangeDisc(Core::CPUThreadGuard{system});
}

void DVDInterface::EjectDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  system.GetDVDInterface().SetDisc(nullptr, {});
}

void DVDInterface::InsertDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate)
{
  auto& di = system.GetDVDInterface();
  std::unique_ptr<DiscIO::VolumeDisc> new_disc = DiscIO::CreateDisc(di.m_disc_path_to_insert);

  if (new_disc)
    di.SetDisc(std::move(new_disc), {});
  else
    PanicAlertFmtT("The disc that was about to be inserted couldn't be found.");

  di.m_disc_path_to_insert.clear();
}

// Must only be called on the CPU thread
void DVDInterface::EjectDisc(const Core::CPUThreadGuard& guard, EjectCause cause)
{
  m_system.GetCoreTiming().ScheduleEvent(0, m_eject_disc);
  if (cause == EjectCause::User)
    ExpansionInterface::g_rtc_flags[ExpansionInterface::RTCFlag::EjectButton] = true;
}

// Must only be called on the CPU thread
void DVDInterface::ChangeDisc(const Core::CPUThreadGuard& guard,
                              const std::vector<std::string>& paths)
{
  ASSERT_MSG(DISCIO, !paths.empty(), "Trying to insert an empty list of discs");

  if (paths.size() > 1)
  {
    m_auto_disc_change_paths = paths;
    m_auto_disc_change_index = 0;
  }

  ChangeDisc(guard, paths[0]);
}

// Must only be called on the CPU thread
void DVDInterface::ChangeDisc(const Core::CPUThreadGuard& guard, const std::string& new_path)
{
  if (!m_disc_path_to_insert.empty())
  {
    PanicAlertFmtT("A disc is already about to be inserted.");
    return;
  }

  EjectDisc(guard, EjectCause::User);

  m_disc_path_to_insert = new_path;
  m_system.GetCoreTiming().ScheduleEvent(m_system.GetSystemTimers().GetTicksPerSecond(),
                                         m_insert_disc);
  m_system.GetMovie().SignalDiscChange(new_path);

  for (size_t i = 0; i < m_auto_disc_change_paths.size(); ++i)
  {
    if (m_auto_disc_change_paths[i] == new_path)
    {
      m_auto_disc_change_index = i;
      return;
    }
  }

  m_auto_disc_change_paths.clear();
}

// Must only be called on the CPU thread
bool DVDInterface::AutoChangeDisc(const Core::CPUThreadGuard& guard)
{
  if (m_auto_disc_change_paths.empty())
    return false;

  m_auto_disc_change_index = (m_auto_disc_change_index + 1) % m_auto_disc_change_paths.size();
  ChangeDisc(guard, m_auto_disc_change_paths[m_auto_disc_change_index]);
  return true;
}

void DVDInterface::SetLidOpen()
{
  const u32 old_value = m_DICVR.CVR;
  m_DICVR.CVR = IsDiscInside() ? 0 : 1;
  if (m_DICVR.CVR != old_value)
    GenerateDIInterrupt(DIInterruptType::CVRINT);
}

bool DVDInterface::UpdateRunningGameMetadata(std::optional<u64> title_id)
{
  auto& dvd_thread = m_system.GetDVDThread();

  if (!dvd_thread.HasDisc())
    return false;

  return dvd_thread.UpdateRunningGameMetadata(IOS::HLE::DIDevice::GetCurrentPartition(), title_id);
}

void DVDInterface::RegisterMMIO(MMIO::Mapping* mmio, u32 base, bool is_wii)
{
  mmio->Register(base | DI_STATUS_REGISTER, MMIO::DirectRead<u32>(&m_DISR.Hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& di = system.GetDVDInterface();
                   const UDISR tmp_status_reg(val);

                   di.m_DISR.DEINTMASK = tmp_status_reg.DEINTMASK.Value();
                   di.m_DISR.TCINTMASK = tmp_status_reg.TCINTMASK.Value();
                   di.m_DISR.BRKINTMASK = tmp_status_reg.BRKINTMASK.Value();
                   di.m_DISR.BREAK = tmp_status_reg.BREAK.Value();

                   if (tmp_status_reg.DEINT)
                     di.m_DISR.DEINT = 0;

                   if (tmp_status_reg.TCINT)
                     di.m_DISR.TCINT = 0;

                   if (tmp_status_reg.BRKINT)
                     di.m_DISR.BRKINT = 0;

                   if (di.m_DISR.BREAK)
                   {
                     DEBUG_ASSERT(false);
                   }

                   di.UpdateInterrupts();
                 }));

  mmio->Register(base | DI_COVER_REGISTER, MMIO::DirectRead<u32>(&m_DICVR.Hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& di = system.GetDVDInterface();
                   const UDICVR tmp_cover_reg(val);

                   di.m_DICVR.CVRINTMASK = tmp_cover_reg.CVRINTMASK.Value();

                   if (tmp_cover_reg.CVRINT)
                     di.m_DICVR.CVRINT = 0;

                   di.UpdateInterrupts();
                 }));

  // Command registers, which have no special logic
  mmio->Register(base | DI_COMMAND_0, MMIO::DirectRead<u32>(&m_DICMDBUF[0]),
                 MMIO::DirectWrite<u32>(&m_DICMDBUF[0]));
  mmio->Register(base | DI_COMMAND_1, MMIO::DirectRead<u32>(&m_DICMDBUF[1]),
                 MMIO::DirectWrite<u32>(&m_DICMDBUF[1]));
  mmio->Register(base | DI_COMMAND_2, MMIO::DirectRead<u32>(&m_DICMDBUF[2]),
                 MMIO::DirectWrite<u32>(&m_DICMDBUF[2]));

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
  mmio->Register(base | DI_DMA_ADDRESS_REGISTER, MMIO::DirectRead<u32>(&m_DIMAR),
                 MMIO::DirectWrite<u32>(&m_DIMAR, is_wii ? ~0x1F : ~0xFC00001F));
  mmio->Register(base | DI_DMA_LENGTH_REGISTER, MMIO::DirectRead<u32>(&m_DILENGTH),
                 MMIO::DirectWrite<u32>(&m_DILENGTH, ~0x1F));
  mmio->Register(base | DI_DMA_CONTROL_REGISTER, MMIO::DirectRead<u32>(&m_DICR.Hex),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& di = system.GetDVDInterface();
                   di.m_DICR.Hex = val & 7;
                   if (di.m_DICR.TSTART)
                   {
                     di.ExecuteCommand(ReplyType::Interrupt);
                   }
                 }));

  mmio->Register(base | DI_IMMEDIATE_DATA_BUFFER, MMIO::DirectRead<u32>(&m_DIIMMBUF),
                 MMIO::DirectWrite<u32>(&m_DIIMMBUF));

  // DI config register is read only.
  mmio->Register(base | DI_CONFIG_REGISTER, MMIO::DirectRead<u32>(&m_DICFG.Hex),
                 MMIO::InvalidWrite<u32>());
}

void DVDInterface::UpdateInterrupts()
{
  const bool set_mask =
      (m_DISR.DEINT & m_DISR.DEINTMASK) != 0 || (m_DISR.TCINT & m_DISR.TCINTMASK) != 0 ||
      (m_DISR.BRKINT & m_DISR.BRKINTMASK) != 0 || (m_DICVR.CVRINT & m_DICVR.CVRINTMASK) != 0;

  m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_DI, set_mask);

  // Required for Summoner: A Goddess Reborn
  m_system.GetCoreTiming().ForceExceptionCheck(50);
}

void DVDInterface::GenerateDIInterrupt(DIInterruptType dvd_interrupt)
{
  switch (dvd_interrupt)
  {
  case DIInterruptType::DEINT:
    m_DISR.DEINT = true;
    break;
  case DIInterruptType::TCINT:
    m_DISR.TCINT = true;
    break;
  case DIInterruptType::BRKINT:
    m_DISR.BRKINT = true;
    break;
  case DIInterruptType::CVRINT:
    m_DICVR.CVRINT = true;
    break;
  }

  UpdateInterrupts();
}

void DVDInterface::SetInterruptEnabled(DIInterruptType interrupt, bool enabled)
{
  switch (interrupt)
  {
  case DIInterruptType::DEINT:
    m_DISR.DEINTMASK = enabled;
    break;
  case DIInterruptType::TCINT:
    m_DISR.TCINTMASK = enabled;
    break;
  case DIInterruptType::BRKINT:
    m_DISR.BRKINTMASK = enabled;
    break;
  case DIInterruptType::CVRINT:
    m_DICVR.CVRINTMASK = enabled;
    break;
  }
}

void DVDInterface::ClearInterrupt(DIInterruptType interrupt)
{
  switch (interrupt)
  {
  case DIInterruptType::DEINT:
    m_DISR.DEINT = false;
    break;
  case DIInterruptType::TCINT:
    m_DISR.TCINT = false;
    break;
  case DIInterruptType::BRKINT:
    m_DISR.BRKINT = false;
    break;
  case DIInterruptType::CVRINT:
    m_DICVR.CVRINT = false;
    break;
  }
}

// Checks the drive state to make sure a read-like command can be performed.
// If false is returned, SetDriveError will have been called, and the caller
// should issue a DEINT interrupt.
bool DVDInterface::CheckReadPreconditions()
{
  if (!IsDiscInside())  // Implies CoverOpened or NoMediumPresent
  {
    ERROR_LOG_FMT(DVDINTERFACE, "No disc inside.");
    SetDriveError(DriveError::MediumNotPresent);
    return false;
  }
  if (m_drive_state == DriveState::DiscChangeDetected)
  {
    ERROR_LOG_FMT(DVDINTERFACE, "Disc changed (motor stopped).");
    SetDriveError(DriveError::MediumChanged);
    return false;
  }
  if (m_drive_state == DriveState::MotorStopped)
  {
    ERROR_LOG_FMT(DVDINTERFACE, "Motor stopped.");
    SetDriveError(DriveError::MotorStopped);
    return false;
  }
  if (m_drive_state == DriveState::DiscIdNotRead)
  {
    ERROR_LOG_FMT(DVDINTERFACE, "Disc id not read.");
    SetDriveError(DriveError::NoDiscID);
    return false;
  }
  return true;
}

// Iff false is returned, ScheduleEvent must be used to finish executing the command
bool DVDInterface::ExecuteReadCommand(u64 dvd_offset, u32 output_address, u32 dvd_length,
                                      u32 output_length, const DiscIO::Partition& partition,
                                      ReplyType reply_type, DIInterruptType* interrupt_type)
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
    WARN_LOG_FMT(DVDINTERFACE, "Detected an attempt to read more data from the DVD "
                               "than what fits inside the out buffer. Clamping.");
    dvd_length = output_length;
  }

  // Many Wii games intentionally try to read from an offset which is just past the end of a
  // regular DVD but just before the end of a DVD-R, displaying "Error #001" and failing to boot
  // if the read succeeds, so it's critical that we set the correct error code for such reads.
  // See https://wiibrew.org/wiki//dev/di#0x8D_DVDLowUnencryptedRead for details on Error #001.
  if (dvd_offset + dvd_length > m_disc_end_offset)
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
void DVDInterface::ExecuteCommand(ReplyType reply_type)
{
  DIInterruptType interrupt_type = DIInterruptType::TCINT;
  bool command_handled_by_thread = false;

  // DVDLowRequestError needs access to the error code set by the previous command
  if (static_cast<DICommand>(m_DICMDBUF[0] >> 24) != DICommand::RequestError)
    SetDriveError(DriveError::None);

  switch (static_cast<DICommand>(m_DICMDBUF[0] >> 24))
  {
  // Used by both GC and Wii
  case DICommand::Inquiry:
  {
    // (shuffle2) Taken from my Wii
    auto& memory = m_system.GetMemory();
    memory.Write_U32(0x00000002, m_DIMAR);      // Revision level, device code
    memory.Write_U32(0x20060526, m_DIMAR + 4);  // Release date
    memory.Write_U32(0x41000000, m_DIMAR + 8);  // Version

    INFO_LOG_FMT(DVDINTERFACE, "DVDLowInquiry (Buffer {:#010x}, {:#x})", m_DIMAR, m_DILENGTH);
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
    switch (m_DICMDBUF[0] & 0xFF)
    {
    case 0x00:  // Read Sector
    {
      const u64 dvd_offset = static_cast<u64>(m_DICMDBUF[1]) << 2;

      INFO_LOG_FMT(
          DVDINTERFACE,
          "Read: DVDOffset={:08x}, DMABuffer = {:08x}, SrcLength = {:08x}, DMALength = {:08x}",
          dvd_offset, m_DIMAR, m_DICMDBUF[2], m_DILENGTH);

      if (m_drive_state == DriveState::ReadyNoReadsMade)
        SetDriveState(DriveState::Ready);

      command_handled_by_thread =
          ExecuteReadCommand(dvd_offset, m_DIMAR, m_DICMDBUF[2], m_DILENGTH, DiscIO::PARTITION_NONE,
                             reply_type, &interrupt_type);
    }
    break;

    case 0x40:  // Read DiscID
      INFO_LOG_FMT(DVDINTERFACE, "Read DiscID: buffer {:08x}", m_DIMAR);
      if (m_drive_state == DriveState::DiscIdNotRead)
      {
        SetDriveState(DriveState::ReadyNoReadsMade);
      }
      else if (m_drive_state == DriveState::ReadyNoReadsMade)
      {
        // The first disc ID reading is required before DTK can be configured.
        // If the disc ID is read again (or any other read occurs), it no longer can
        // be configured.
        SetDriveState(DriveState::Ready);
      }

      command_handled_by_thread = ExecuteReadCommand(
          0, m_DIMAR, 0x20, m_DILENGTH, DiscIO::PARTITION_NONE, reply_type, &interrupt_type);
      break;

    default:
      ERROR_LOG_FMT(DVDINTERFACE, "Unknown read subcommand: {:08x}", m_DICMDBUF[0]);
      break;
    }
    break;

  // Used by both GC and Wii
  case DICommand::Seek:
    // Currently unimplemented
    INFO_LOG_FMT(DVDINTERFACE, "Seek: offset={:09x} (ignoring)",
                 static_cast<u64>(m_DICMDBUF[1]) << 2);
    break;

  // Wii-exclusive
  case DICommand::ReadDVDMetadata:
    switch ((m_DICMDBUF[0] >> 16) & 0xFF)
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
      ERROR_LOG_FMT(DVDINTERFACE, "Unknown 0xAD subcommand in {:08x}", m_DICMDBUF[0]);
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

    auto& memory = m_system.GetMemory();
    // TODO: Read the .bca file that cleanrip generates, if it exists
    // memory.CopyToEmu(output_address, bca_data, 0x40);
    memory.Memset(m_DIMAR, 0, 0x40);
    memory.Write_U8(1, m_DIMAR + 0x33);
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
    if (m_drive_state == DriveState::Ready)
      drive_state = 0;
    else
      drive_state = static_cast<u32>(m_drive_state) - 1;

    const u32 result = (drive_state << 24) | static_cast<u32>(m_error_code);
    INFO_LOG_FMT(DVDINTERFACE, "Requesting error... ({:#010x})", result);
    m_DIIMMBUF = result;
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
      ERROR_LOG_FMT(DVDINTERFACE, "Cannot play audio (command {:08x})", m_DICMDBUF[0]);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }
    if (!m_enable_dtk)
    {
      ERROR_LOG_FMT(
          DVDINTERFACE,
          "Attempted to change playing audio while audio is disabled!  ({:08x} {:08x} {:08x})",
          m_DICMDBUF[0], m_DICMDBUF[1], m_DICMDBUF[2]);
      SetDriveError(DriveError::NoAudioBuf);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    if (m_drive_state == DriveState::ReadyNoReadsMade)
      SetDriveState(DriveState::Ready);

    switch ((m_DICMDBUF[0] >> 16) & 0xFF)
    {
    case 0x00:
    {
      const u64 offset = static_cast<u64>(m_DICMDBUF[1]) << 2;
      const u32 length = m_DICMDBUF[2];
      INFO_LOG_FMT(DVDINTERFACE, "(Audio) Start stream: offset: {:08x} length: {:08x}", offset,
                   length);

      if ((offset == 0) && (length == 0))
      {
        m_stop_at_track_end = true;
      }
      else if (!m_stop_at_track_end)
      {
        m_next_start = offset;
        m_next_length = length;
        if (!m_stream)
        {
          m_current_start = m_next_start;
          m_current_length = m_next_length;
          m_audio_position = m_current_start;
          m_adpcm_decoder.ResetFilter();
          m_stream = true;
        }
      }
      break;
    }
    case 0x01:
      INFO_LOG_FMT(DVDINTERFACE, "(Audio) Stop stream");
      m_stop_at_track_end = false;
      m_stream = false;
      break;
    default:
      ERROR_LOG_FMT(DVDINTERFACE, "Invalid audio command!  ({:08x} {:08x} {:08x})", m_DICMDBUF[0],
                    m_DICMDBUF[1], m_DICMDBUF[2]);
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

    if (!m_enable_dtk)
    {
      ERROR_LOG_FMT(DVDINTERFACE, "Attempted to request audio status while audio is disabled!");
      SetDriveError(DriveError::NoAudioBuf);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    switch (m_DICMDBUF[0] >> 16 & 0xFF)
    {
    case 0x00:  // Returns streaming status
      INFO_LOG_FMT(DVDINTERFACE,
                   "(Audio): Stream Status: Request Audio status "
                   "AudioPos:{:08x}/{:08x} "
                   "CurrentStart:{:08x} CurrentLength:{:08x}",
                   m_audio_position, m_current_start + m_current_length, m_current_start,
                   m_current_length);
      m_DIIMMBUF = (m_stream ? 1 : 0);
      break;
    case 0x01:  // Returns the current offset
      INFO_LOG_FMT(DVDINTERFACE, "(Audio): Stream Status: Request Audio status AudioPos:{:08x}",
                   m_audio_position);
      m_DIIMMBUF = static_cast<u32>((m_audio_position & 0xffffffffffff8000ull) >> 2);
      break;
    case 0x02:  // Returns the start offset
      INFO_LOG_FMT(DVDINTERFACE, "(Audio): Stream Status: Request Audio status CurrentStart:{:08x}",
                   m_current_start);
      m_DIIMMBUF = static_cast<u32>(m_current_start >> 2);
      break;
    case 0x03:  // Returns the total length
      INFO_LOG_FMT(DVDINTERFACE,
                   "(Audio): Stream Status: Request Audio status CurrentLength:{:08x}",
                   m_current_length);
      m_DIIMMBUF = m_current_length;
      break;
    default:
      ERROR_LOG_FMT(DVDINTERFACE, "Invalid audio status command!  ({:08x} {:08x} {:08x})",
                    m_DICMDBUF[0], m_DICMDBUF[1], m_DICMDBUF[2]);
      SetDriveError(DriveError::InvalidAudioCommand);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }
  }
  break;

  // Used by both GC and Wii
  case DICommand::StopMotor:
  {
    const bool eject = (m_DICMDBUF[0] & (1 << 17));
    const bool kill = (m_DICMDBUF[0] & (1 << 20));
    INFO_LOG_FMT(DVDINTERFACE, "DVDLowStopMotor{}{}", eject ? " eject" : "", kill ? " kill!" : "");

    if (m_drive_state == DriveState::Ready || m_drive_state == DriveState::ReadyNoReadsMade ||
        m_drive_state == DriveState::DiscIdNotRead)
    {
      SetDriveState(DriveState::MotorStopped);
    }

    const bool force_eject = eject && !kill;

    if (Config::Get(Config::MAIN_AUTO_DISC_CHANGE) && !m_system.GetMovie().IsPlayingInput() &&
        m_system.GetDVDThread().IsInsertedDiscRunning() && !m_auto_disc_change_paths.empty())
    {
      m_system.GetCoreTiming().ScheduleEvent(
          force_eject ? 0 : m_system.GetSystemTimers().GetTicksPerSecond() / 2, m_auto_change_disc);
      OSD::AddMessage("Changing discs automatically...", OSD::Duration::NORMAL);
    }
    else if (force_eject)
    {
      EjectDisc(Core::CPUThreadGuard{m_system}, EjectCause::Software);
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

    if (m_drive_state == DriveState::Ready)
    {
      ERROR_LOG_FMT(DVDINTERFACE,
                    "Attempted to change DTK configuration after a read has been made!");
      SetDriveError(DriveError::InvalidPeriod);
      interrupt_type = DIInterruptType::DEINT;
      break;
    }

    // Note that this can be called multiple times, as long as the drive is in the ReadyNoReadsMade
    // state. Calling it does not exit that state.
    AudioBufferConfig((m_DICMDBUF[0] >> 16) & 1, m_DICMDBUF[0] & 0xf);
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
    ERROR_LOG_FMT(DVDINTERFACE, "Unsupported DVD Drive debug command {:#010x}", m_DICMDBUF[0]);
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;

  // Unlock Commands. 1: "MATSHITA" 2: "DVD-GAME"
  // Just for fun
  // Can only be used through direct access.  The unlock command doesn't seem to work on the Wii.
  case DICommand::DebugUnlock:
  {
    if (m_DICMDBUF[0] == 0xFF014D41 && m_DICMDBUF[1] == 0x54534849 && m_DICMDBUF[2] == 0x54410200)
    {
      INFO_LOG_FMT(DVDINTERFACE, "Unlock test 1 passed");
    }
    else if (m_DICMDBUF[0] == 0xFF004456 && m_DICMDBUF[1] == 0x442D4741 &&
             m_DICMDBUF[2] == 0x4D450300)
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
    ERROR_LOG_FMT(DVDINTERFACE, "Unknown command {:#010x} (Buffer {:#010x}, {:#x})", m_DICMDBUF[0],
                  m_DIMAR, m_DILENGTH);
    PanicAlertFmtT("Unknown DVD command {0:08x} - fatal error", m_DICMDBUF[0]);
    SetDriveError(DriveError::InvalidCommand);
    interrupt_type = DIInterruptType::DEINT;
    break;
  }

  if (!command_handled_by_thread)
  {
    // TODO: Needs testing to determine if MINIMUM_COMMAND_LATENCY_US is accurate for this
    m_system.GetCoreTiming().ScheduleEvent(
        MINIMUM_COMMAND_LATENCY_US * (m_system.GetSystemTimers().GetTicksPerSecond() / 1000000),
        m_finish_executing_command, PackFinishExecutingCommandUserdata(reply_type, interrupt_type));
  }
}

void DVDInterface::PerformDecryptingRead(u32 position, u32 length, u32 output_address,
                                         const DiscIO::Partition& partition, ReplyType reply_type)
{
  DIInterruptType interrupt_type = DIInterruptType::TCINT;

  if (m_drive_state == DriveState::ReadyNoReadsMade)
    SetDriveState(DriveState::Ready);

  const bool command_handled_by_thread =
      ExecuteReadCommand(static_cast<u64>(position) << 2, output_address, length, length, partition,
                         reply_type, &interrupt_type);

  if (!command_handled_by_thread)
  {
    // TODO: Needs testing to determine if MINIMUM_COMMAND_LATENCY_US is accurate for this
    m_system.GetCoreTiming().ScheduleEvent(
        MINIMUM_COMMAND_LATENCY_US * (m_system.GetSystemTimers().GetTicksPerSecond() / 1000000),
        m_finish_executing_command, PackFinishExecutingCommandUserdata(reply_type, interrupt_type));
  }
}

void DVDInterface::ForceOutOfBoundsRead(ReplyType reply_type)
{
  INFO_LOG_FMT(DVDINTERFACE, "Forcing an out-of-bounds disc read.");

  if (m_drive_state == DriveState::ReadyNoReadsMade)
    SetDriveState(DriveState::Ready);

  SetDriveError(DriveError::BlockOOB);

  // TODO: Needs testing to determine if MINIMUM_COMMAND_LATENCY_US is accurate for this
  const DIInterruptType interrupt_type = DIInterruptType::DEINT;
  m_system.GetCoreTiming().ScheduleEvent(
      MINIMUM_COMMAND_LATENCY_US * (m_system.GetSystemTimers().GetTicksPerSecond() / 1000000),
      m_finish_executing_command, PackFinishExecutingCommandUserdata(reply_type, interrupt_type));
}

void DVDInterface::AudioBufferConfig(bool enable_dtk, u8 dtk_buffer_length)
{
  m_enable_dtk = enable_dtk;
  m_dtk_buffer_length = dtk_buffer_length;
  if (m_enable_dtk)
    INFO_LOG_FMT(DVDINTERFACE, "DTK enabled: buffer size {}", m_dtk_buffer_length);
  else
    INFO_LOG_FMT(DVDINTERFACE, "DTK disabled");
}

static u64 PackFinishExecutingCommandUserdata(ReplyType reply_type, DIInterruptType interrupt_type)
{
  return (static_cast<u64>(reply_type) << 32) + static_cast<u32>(interrupt_type);
}

void DVDInterface::FinishExecutingCommandCallback(Core::System& system, u64 userdata,
                                                  s64 cycles_late)
{
  ReplyType reply_type = static_cast<ReplyType>(userdata >> 32);
  DIInterruptType interrupt_type = static_cast<DIInterruptType>(userdata & 0xFFFFFFFF);
  system.GetDVDInterface().FinishExecutingCommand(reply_type, interrupt_type, cycles_late);
}

void DVDInterface::SetDriveState(DriveState state)
{
  m_drive_state = state;
}

void DVDInterface::SetDriveError(DriveError error)
{
  m_error_code = error;
}

void DVDInterface::FinishExecutingCommand(ReplyType reply_type, DIInterruptType interrupt_type,
                                          s64 cycles_late, const std::vector<u8>& data)
{
  // The data parameter contains the requested data iff this was called from DVDThread, and is
  // empty otherwise. DVDThread is the only source of ReplyType::NoReply and ReplyType::DTK.

  u32 transfer_size = 0;
  if (reply_type == ReplyType::NoReply)
    transfer_size = static_cast<u32>(data.size());
  else if (reply_type == ReplyType::Interrupt || reply_type == ReplyType::IOS)
    transfer_size = m_DILENGTH;

  if (interrupt_type == DIInterruptType::TCINT)
  {
    m_DIMAR += transfer_size;
    m_DILENGTH -= transfer_size;
  }

  switch (reply_type)
  {
  case ReplyType::NoReply:
  {
    break;
  }

  case ReplyType::Interrupt:
  {
    if (m_DICR.TSTART)
    {
      m_DICR.TSTART = 0;
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
void DVDInterface::ScheduleReads(u64 offset, u32 length, const DiscIO::Partition& partition,
                                 u32 output_address, ReplyType reply_type)
{
  // The drive continues to read 1 MiB beyond the last read position when idle.
  // If a future read falls within this window, part of the read may be returned
  // from the buffer. Data can be transferred from the buffer at up to 32 MiB/s.

  // Metroid Prime is a good example of a game that's sensitive to disc timing
  // details; if there isn't enough latency in the right places, doors can open
  // faster than on real hardware, and if there's too much latency in the wrong
  // places, the video before the save-file select screen lags.

  auto& core_timing = m_system.GetCoreTiming();
  const u64 current_time = core_timing.GetTicks();
  const u32 ticks_per_second = m_system.GetSystemTimers().GetTicksPerSecond();
  auto& dvd_thread = m_system.GetDVDThread();
  const bool wii_disc = dvd_thread.GetDiscType() == DiscIO::Platform::WiiDisc;

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
  u64 dvd_offset = dvd_thread.PartitionOffsetToRawOffset(offset, partition);
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
    if (m_read_buffer_start_time == m_read_buffer_end_time)
    {
      // No buffer
      buffer_start = buffer_end = head_position = 0;
    }
    else
    {
      buffer_start = m_read_buffer_end_offset > STREAMING_BUFFER_SIZE ?
                         m_read_buffer_end_offset - STREAMING_BUFFER_SIZE :
                         0;

      DEBUG_LOG_FMT(DVDINTERFACE, "Buffer: now={:#x} start time={:#x} end time={:#x}", current_time,
                    m_read_buffer_start_time, m_read_buffer_end_time);

      if (current_time >= m_read_buffer_end_time)
      {
        // Buffer is fully read
        buffer_end = m_read_buffer_end_offset;
      }
      else
      {
        // The amount of data the buffer contains *right now*, rounded to a DVD ECC block.
        buffer_end = m_read_buffer_start_offset +
                     Common::AlignDown((current_time - m_read_buffer_start_time) *
                                           (m_read_buffer_end_offset - m_read_buffer_start_offset) /
                                           (m_read_buffer_end_time - m_read_buffer_start_time),
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
      READ_COMMAND_LATENCY_US * (m_system.GetSystemTimers().GetTicksPerSecond() / 1000000);

  u32 buffered_blocks = 0;
  u32 unbuffered_blocks = 0;

  const u32 bytes_per_chunk = partition != DiscIO::PARTITION_NONE && dvd_thread.HasWiiHashes() ?
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
    dvd_thread.StartReadToEmulatedRAM(output_address, offset, chunk_length, partition,
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
      m_read_buffer_start_offset = last_block;
    }
    else
    {
      // Partial buffer read
      m_read_buffer_start_offset = buffer_end;
    }

    m_read_buffer_end_offset = last_block + STREAMING_BUFFER_SIZE - BUFFER_BACKWARD_SEEK_LIMIT;
    if (seek)
    {
      // If we seek, the block preceding the first accessed block never gets read into the buffer
      m_read_buffer_end_offset =
          std::max(m_read_buffer_end_offset, first_block + STREAMING_BUFFER_SIZE);
    }

    // Assume the buffer starts prefetching new blocks right after the end of the last operation
    m_read_buffer_start_time = current_time + ticks_until_completion;
    m_read_buffer_end_time =
        m_read_buffer_start_time +
        static_cast<u64>(ticks_per_second *
                         DVDMath::CalculateRawDiscReadTime(
                             m_read_buffer_start_offset,
                             m_read_buffer_end_offset - m_read_buffer_start_offset, wii_disc));
  }

  DEBUG_LOG_FMT(DVDINTERFACE,
                "Schedule reads: ECC blocks unbuffered={}, buffered={}, "
                "ticks={}, time={} us",
                unbuffered_blocks, buffered_blocks, ticks_until_completion,
                ticks_until_completion * 1000000 / m_system.GetSystemTimers().GetTicksPerSecond());
}

}  // namespace DVD
