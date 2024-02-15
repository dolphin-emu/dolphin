// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GBACore.h"

#define PYCPARSE  // Remove static functions from the header
#include <mgba/core/interface.h>
#undef PYCPARSE
#include <mgba-util/vfs.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gba/gba.h>

#include "AudioCommon/AudioCommon.h"
#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/MinizipUtil.h"
#include "Common/ScopeGuard.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Host.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"

namespace HW::GBA
{
namespace
{
mLogger s_stub_logger = {
    [](mLogger*, int category, mLogLevel level, const char* format, va_list args) {}, nullptr};
}  // namespace

constexpr auto SAMPLES = 512;
constexpr auto SAMPLE_RATE = 48000;

// libmGBA does not return the correct frequency for some GB models
static u32 GetCoreFrequency(mCore* core)
{
  if (core->platform(core) != mPLATFORM_GB)
    return static_cast<u32>(core->frequency(core));

  switch (static_cast<::GB*>(core->board)->model)
  {
  case GB_MODEL_CGB:
  case GB_MODEL_SCGB:
  case GB_MODEL_AGB:
    return CGB_SM83_FREQUENCY;
  case GB_MODEL_SGB:
    return SGB_SM83_FREQUENCY;
  default:
    return DMG_SM83_FREQUENCY;
  }
}

static VFile* OpenROM_Archive(const char* path)
{
  VFile* vf{};
  VDir* archive = VDirOpenArchive(path);
  if (!archive)
    return nullptr;
  VFile* vf_archive =
      VDirFindFirst(archive, [](VFile* vf_) { return mCoreIsCompatible(vf_) != mPLATFORM_NONE; });
  if (vf_archive)
  {
    size_t size = static_cast<size_t>(vf_archive->size(vf_archive));

    std::vector<u8> buffer(size);
    vf_archive->seek(vf_archive, 0, SEEK_SET);
    vf_archive->read(vf_archive, buffer.data(), size);
    vf_archive->close(vf_archive);

    vf = VFileMemChunk(buffer.data(), size);
  }
  archive->close(archive);
  return vf;
}

static VFile* OpenROM_Zip(const char* path)
{
  VFile* vf{};
  unzFile zip = unzOpen(path);
  if (!zip)
    return nullptr;
  do
  {
    unz_file_info info{};
    if (unzGetCurrentFileInfo(zip, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK ||
        !info.uncompressed_size)
      continue;

    std::vector<u8> buffer(info.uncompressed_size);
    if (!Common::ReadFileFromZip(zip, &buffer))
      continue;

    vf = VFileMemChunk(buffer.data(), info.uncompressed_size);
    if (mCoreIsCompatible(vf) == mPLATFORM_GBA)
    {
      vf->seek(vf, 0, SEEK_SET);
      break;
    }

    vf->close(vf);
    vf = nullptr;
  } while (unzGoToNextFile(zip) == UNZ_OK);
  unzClose(zip);
  return vf;
}

static VFile* OpenROM(const char* rom_path)
{
  VFile* vf{};

  vf = OpenROM_Archive(rom_path);
  if (!vf)
    vf = OpenROM_Zip(rom_path);
  if (!vf)
    vf = VFileOpen(rom_path, O_RDONLY);
  if (!vf)
    return nullptr;

  if (mCoreIsCompatible(vf) == mPLATFORM_NONE)
  {
    vf->close(vf);
    return nullptr;
  }
  vf->seek(vf, 0, SEEK_SET);

  return vf;
}

static std::array<u8, 20> GetROMHash(VFile* rom)
{
  size_t size = rom->size(rom);
  u8* buffer = static_cast<u8*>(rom->map(rom, size, MAP_READ));

  const auto digest = Common::SHA1::CalculateDigest(buffer, size);
  rom->unmap(rom, buffer, size);

  return digest;
}

Core::Core(::Core::System& system, int device_number)
    : m_device_number(device_number), m_system(system)
{
  mLogSetDefaultLogger(&s_stub_logger);
}

Core::~Core()
{
  Stop();
}

bool Core::Start(u64 gc_ticks)
{
  if (IsStarted())
    return false;

  Common::ScopeGuard start_guard{[&] { Stop(); }};

  VFile* rom{};
  Common::ScopeGuard rom_guard{[&] {
    if (rom)
      rom->close(rom);
  }};

  m_rom_path = Config::Get(Config::MAIN_GBA_ROM_PATHS[m_device_number]);
  if (!m_rom_path.empty())
  {
    rom = OpenROM(m_rom_path.c_str());
    if (!rom)
    {
      PanicAlertFmtT("Error: GBA{0} failed to open the ROM in {1}", m_device_number + 1,
                     m_rom_path);
      return false;
    }
    m_rom_hash = GetROMHash(rom);
  }

  m_core = rom ? mCoreFindVF(rom) : mCoreCreate(mPLATFORM_GBA);
  if (!m_core)
  {
    PanicAlertFmtT("Error: GBA{0} failed to create core", m_device_number + 1);
    return false;
  }
  m_core->init(m_core);

  mCoreInitConfig(m_core, "dolphin");
  mCoreConfigSetValue(&m_core->config, "idleOptimization", "detect");
  mCoreConfigSetIntValue(&m_core->config, "useBios", 0);
  mCoreConfigSetIntValue(&m_core->config, "skipBios", 0);

  if (m_core->platform(m_core) == mPLATFORM_GBA &&
      !LoadBIOS(File::GetUserPath(F_GBABIOS_IDX).c_str()))
  {
    return false;
  }

  if (rom)
  {
    if (!m_core->loadROM(m_core, rom))
    {
      PanicAlertFmtT("Error: GBA{0} failed to load the ROM in {1}", m_device_number + 1,
                     m_rom_path);
      return false;
    }
    rom_guard.Dismiss();

    std::array<char, 17> game_title{};
    m_core->getGameTitle(m_core, game_title.data());
    m_game_title = game_title.data();

    m_save_path = NetPlay::IsNetPlayRunning() ? NetPlay::GetGBASavePath(m_device_number) :
                                                GetSavePath(m_rom_path, m_device_number);
    if (!m_save_path.empty() && !LoadSave(m_save_path.c_str()))
      return false;
  }

  m_last_gc_ticks = gc_ticks;
  m_gc_ticks_remainder = 0;
  m_keys = 0;

  SetSIODriver();
  SetVideoBuffer();
  SetSampleRates();
  AddCallbacks();
  SetAVStream();
  SetupEvent();

  m_core->reset(m_core);
  m_started = true;
  start_guard.Dismiss();
  // Notify the host and handle a dimension change if that happened after reset()
  SetVideoBuffer();

  if (Config::Get(Config::MAIN_GBA_THREADS))
  {
    m_idle = true;
    m_exit_loop = false;
    m_thread = std::make_unique<std::thread>([this] { ThreadLoop(); });
  }

  return true;
}

void Core::Stop()
{
  if (m_thread)
  {
    Flush();
    m_exit_loop = true;
    {
      std::lock_guard<std::mutex> lock(m_queue_mutex);
      m_command_cv.notify_one();
    }
    m_thread->join();
    m_thread.reset();
  }
  if (m_core)
  {
    mCoreConfigDeinit(&m_core->config);
    m_core->deinit(m_core);
    m_core = nullptr;
  }
  m_started = false;
  m_rom_path = {};
  m_save_path = {};
  m_rom_hash = {};
  m_game_title = {};
}

void Core::Reset()
{
  Flush();
  if (!IsStarted())
    return;

  m_core->reset(m_core);
}

bool Core::IsStarted() const
{
  return m_started;
}

CoreInfo Core::GetCoreInfo() const
{
  CoreInfo info{};
  info.device_number = m_device_number;
  info.width = GBA_VIDEO_HORIZONTAL_PIXELS;
  info.height = GBA_VIDEO_VERTICAL_PIXELS;

  if (!IsStarted())
    return info;

  info.is_gba = m_core->platform(m_core) == mPlatform::mPLATFORM_GBA;
  info.has_rom = !m_rom_path.empty();
  info.has_ereader =
      info.is_gba && static_cast<::GBA*>(m_core->board)->memory.hw.devices & HW_EREADER;
  m_core->currentVideoSize(m_core, &info.width, &info.height);
  info.game_title = m_game_title;
  return info;
}

void Core::SetHost(std::weak_ptr<GBAHostInterface> host)
{
  m_host = std::move(host);
}

void Core::SetForceDisconnect(bool force_disconnect)
{
  m_force_disconnect = force_disconnect;
}

void Core::EReaderQueueCard(std::string_view card_path)
{
  Flush();
  if (!GetCoreInfo().has_ereader)
    return;

  File::IOFile file(std::string(card_path), "rb");
  std::vector<u8> core_state(file.GetSize());
  file.ReadBytes(core_state.data(), core_state.size());
  GBACartEReaderQueueCard(static_cast<::GBA*>(m_core->board), core_state.data(), core_state.size());
}

bool Core::LoadBIOS(const char* bios_path)
{
  VFile* vf = VFileOpen(bios_path, O_RDONLY);
  if (!vf)
  {
    PanicAlertFmtT("Error: GBA{0} failed to open the BIOS in {1}", m_device_number + 1, bios_path);
    return false;
  }

  if (!m_core->loadBIOS(m_core, vf, 0))
  {
    PanicAlertFmtT("Error: GBA{0} failed to load the BIOS in {1}", m_device_number + 1, bios_path);
    vf->close(vf);
    return false;
  }

  return true;
}

bool Core::LoadSave(const char* save_path)
{
  VFile* vf = VFileOpen(save_path, O_CREAT | O_RDWR);
  if (!vf)
  {
    PanicAlertFmtT("Error: GBA{0} failed to open the save in {1}", m_device_number + 1, save_path);
    return false;
  }

  if (!m_core->loadSave(m_core, vf))
  {
    PanicAlertFmtT("Error: GBA{0} failed to load the save in {1}", m_device_number + 1, save_path);
    vf->close(vf);
    return false;
  }

  return true;
}

void Core::SetSIODriver()
{
  if (m_core->platform(m_core) != mPLATFORM_GBA)
    return;

  GBASIOJOYCreate(&m_sio_driver);
  GBASIOSetDriver(&static_cast<::GBA*>(m_core->board)->sio, &m_sio_driver, SIO_JOYBUS);

  m_sio_driver.core = this;
  m_sio_driver.load = [](GBASIODriver* driver) {
    static_cast<SIODriver*>(driver)->core->m_link_enabled = true;
    return true;
  };
  m_sio_driver.unload = [](GBASIODriver* driver) {
    static_cast<SIODriver*>(driver)->core->m_link_enabled = false;
    return true;
  };
}

void Core::SetVideoBuffer()
{
  u32 width, height;
  m_core->currentVideoSize(m_core, &width, &height);
  m_video_buffer.resize(width * height);
  m_core->setVideoBuffer(m_core, m_video_buffer.data(), width);
  if (auto host = m_host.lock())
    host->GameChanged();
}

void Core::SetSampleRates()
{
  m_core->setAudioBufferSize(m_core, SAMPLES);
  blip_set_rates(m_core->getAudioChannel(m_core, 0), m_core->frequency(m_core), SAMPLE_RATE);
  blip_set_rates(m_core->getAudioChannel(m_core, 1), m_core->frequency(m_core), SAMPLE_RATE);

  SoundStream* sound_stream = m_system.GetSoundStream();
  sound_stream->GetMixer()->SetGBAInputSampleRateDivisors(
      m_device_number, Mixer::FIXED_SAMPLE_RATE_DIVIDEND / SAMPLE_RATE);
}

void Core::AddCallbacks()
{
  mCoreCallbacks callbacks{};
  callbacks.context = this;
  callbacks.keysRead = [](void* context) {
    auto core = static_cast<Core*>(context);
    core->m_core->setKeys(core->m_core, core->m_keys);
  };
  callbacks.videoFrameEnded = [](void* context) {
    auto core = static_cast<Core*>(context);
    if (auto host = core->m_host.lock())
      host->FrameEnded(core->m_video_buffer);
  };
  m_core->addCoreCallbacks(m_core, &callbacks);
}

void Core::SetAVStream()
{
  m_stream = {};
  m_stream.core = this;
  m_stream.videoDimensionsChanged = [](mAVStream* stream, unsigned width, unsigned height) {
    auto core = static_cast<AVStream*>(stream)->core;
    core->SetVideoBuffer();
  };
  m_stream.postAudioBuffer = [](mAVStream* stream, blip_t* left, blip_t* right) {
    auto core = static_cast<AVStream*>(stream)->core;
    std::vector<s16> buffer(SAMPLES * 2);
    blip_read_samples(left, &buffer[0], SAMPLES, 1);
    blip_read_samples(right, &buffer[1], SAMPLES, 1);

    SoundStream* sound_stream = core->m_system.GetSoundStream();
    sound_stream->GetMixer()->PushGBASamples(core->m_device_number, &buffer[0], SAMPLES);
  };
  m_core->setAVStream(m_core, &m_stream);
}

void Core::SetupEvent()
{
  m_event.context = this;
  m_event.name = "Dolphin Sync";
  m_event.callback = [](mTiming* timing, void* context, u32 cycles_late) {
    Core* core = static_cast<Core*>(context);
    if (core->m_core->platform(core->m_core) == mPLATFORM_GBA)
      static_cast<::GBA*>(core->m_core->board)->earlyExit = true;
    else if (core->m_core->platform(core->m_core) == mPLATFORM_GB)
      static_cast<::GB*>(core->m_core->board)->earlyExit = true;
    core->m_waiting_for_event = false;
  };
  m_event.priority = 0x80;
}

void Core::SendJoybusCommand(u64 gc_ticks, int transfer_time, u8* buffer, u16 keys)
{
  if (!IsStarted())
    return;

  Command command{};
  command.ticks = gc_ticks;
  command.transfer_time = transfer_time;
  command.sync_only = buffer == nullptr;
  if (buffer)
    std::copy_n(buffer, command.buffer.size(), command.buffer.begin());
  command.keys = keys;

  if (m_thread)
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_command_queue.push(command);
    m_idle = false;
    m_command_cv.notify_one();
  }
  else
  {
    RunCommand(command);
  }
}

std::vector<u8> Core::GetJoybusResponse()
{
  if (!IsStarted())
    return {};

  if (m_thread)
  {
    std::unique_lock<std::mutex> lock(m_response_mutex);
    m_response_cv.wait(lock, [&] { return m_response_ready; });
  }
  m_response_ready = false;
  return m_response;
}

void Core::Flush()
{
  if (!IsStarted() || !m_thread)
    return;
  std::unique_lock<std::mutex> lock(m_queue_mutex);
  m_response_cv.wait(lock, [&] { return m_idle; });
}

void Core::ThreadLoop()
{
  Common::SetCurrentThreadName(fmt::format("GBA{}", m_device_number + 1).c_str());
  std::unique_lock<std::mutex> queue_lock(m_queue_mutex);
  while (true)
  {
    m_command_cv.wait(queue_lock, [&] { return !m_command_queue.empty() || m_exit_loop; });
    if (m_exit_loop)
      break;
    Command command{m_command_queue.front()};
    m_command_queue.pop();
    queue_lock.unlock();

    RunCommand(command);

    queue_lock.lock();
    if (m_command_queue.empty())
      m_idle = true;
    m_response_cv.notify_one();
  }
}

void Core::RunCommand(Command& command)
{
  m_keys = command.keys;
  RunUntil(command.ticks);
  if (!command.sync_only)
  {
    m_response.clear();
    if (m_link_enabled && !m_force_disconnect)
    {
      int recvd = GBASIOJOYSendCommand(
          &m_sio_driver, static_cast<GBASIOJOYCommand>(command.buffer[0]), &command.buffer[1]);
      std::copy(command.buffer.begin() + 1, command.buffer.begin() + 1 + recvd,
                std::back_inserter(m_response));
    }

    if (m_thread && !m_response_ready)
    {
      std::lock_guard<std::mutex> response_lock(m_response_mutex);
      m_response_ready = true;
      m_response_cv.notify_one();
    }
    else
    {
      m_response_ready = true;
    }
  }
  if (command.transfer_time)
    RunFor(command.transfer_time);
}

void Core::RunUntil(u64 gc_ticks)
{
  if (static_cast<s64>(gc_ticks - m_last_gc_ticks) <= 0)
    return;

  const u64 gc_frequency = m_system.GetSystemTimers().GetTicksPerSecond();
  const u32 core_frequency = GetCoreFrequency(m_core);

  mTimingSchedule(m_core->timing, &m_event,
                  static_cast<s32>((gc_ticks - m_last_gc_ticks) * core_frequency / gc_frequency));
  m_waiting_for_event = true;

  s32 begin_time = mTimingCurrentTime(m_core->timing);
  while (m_waiting_for_event)
    m_core->runLoop(m_core);
  s32 end_time = mTimingCurrentTime(m_core->timing);

  u64 d = (static_cast<u64>(end_time - begin_time) * gc_frequency) + m_gc_ticks_remainder;
  m_last_gc_ticks += d / core_frequency;
  m_gc_ticks_remainder = d % core_frequency;
}

void Core::RunFor(u64 gc_ticks)
{
  RunUntil(m_last_gc_ticks + gc_ticks);
}

void Core::ImportState(std::string_view state_path)
{
  Flush();
  if (!IsStarted())
    return;

  std::vector<u8> core_state(m_core->stateSize(m_core));
  File::IOFile file(std::string(state_path), "rb");
  if (core_state.size() != file.GetSize())
    return;

  file.ReadBytes(core_state.data(), core_state.size());
  m_core->loadState(m_core, core_state.data());
}

void Core::ExportState(std::string_view state_path)
{
  Flush();
  if (!IsStarted())
    return;

  std::vector<u8> core_state(m_core->stateSize(m_core));
  m_core->saveState(m_core, core_state.data());

  File::IOFile file(std::string(state_path), "wb");
  file.WriteBytes(core_state.data(), core_state.size());
}

void Core::ImportSave(std::string_view save_path)
{
  Flush();
  if (!IsStarted())
    return;

  File::IOFile file(std::string(save_path), "rb");
  std::vector<u8> save_file(file.GetSize());
  if (!file.ReadBytes(save_file.data(), save_file.size()))
    return;

  m_core->savedataRestore(m_core, save_file.data(), save_file.size(), true);
  m_core->reset(m_core);
}

void Core::ExportSave(std::string_view save_path)
{
  Flush();
  if (!IsStarted())
    return;

  File::IOFile file(std::string(save_path), "wb");

  void* sram = nullptr;
  size_t size = m_core->savedataClone(m_core, &sram);
  if (!sram)
    return;

  file.WriteBytes(sram, size);
  free(sram);
}

void Core::DoState(PointerWrap& p)
{
  Flush();
  if (!IsStarted())
  {
    ::Core::DisplayMessage(fmt::format("GBA{} core not started. Aborting.", m_device_number + 1),
                           3000);
    p.SetVerifyMode();
    return;
  }

  bool has_rom = !m_rom_path.empty();
  p.Do(has_rom);
  auto old_hash = m_rom_hash;
  p.Do(m_rom_hash);
  auto old_title = m_game_title;
  p.Do(m_game_title);

  if (p.IsReadMode() && (has_rom != !m_rom_path.empty() ||
                         (has_rom && (old_hash != m_rom_hash || old_title != m_game_title))))
  {
    ::Core::DisplayMessage(
        fmt::format("Incompatible ROM state in GBA{}. Aborting load state.", m_device_number + 1),
        3000);
    p.SetVerifyMode();
    return;
  }

  p.Do(m_video_buffer);
  p.Do(m_last_gc_ticks);
  p.Do(m_gc_ticks_remainder);
  p.Do(m_keys);
  p.Do(m_link_enabled);
  p.Do(m_response_ready);
  p.Do(m_response);

  std::vector<u8> core_state;
  core_state.resize(m_core->stateSize(m_core));

  if (p.IsWriteMode() || p.IsVerifyMode())
  {
    m_core->saveState(m_core, core_state.data());
  }

  p.Do(core_state);

  if (p.IsReadMode() && m_core->stateSize(m_core) == core_state.size())
  {
    m_core->loadState(m_core, core_state.data());
    if (auto host = m_host.lock())
      host->FrameEnded(m_video_buffer);
  }
}

bool Core::GetRomInfo(const char* rom_path, std::array<u8, 20>& hash, std::string& title)
{
  VFile* rom = OpenROM(rom_path);
  if (!rom)
    return false;

  hash = GetROMHash(rom);

  mCore* core = mCoreFindVF(rom);
  if (!core)
  {
    rom->close(rom);
    return false;
  }
  core->init(core);
  if (!core->loadROM(core, rom))
  {
    rom->close(rom);
    return false;
  }

  std::array<char, 17> game_title{};
  core->getGameTitle(core, game_title.data());
  title = game_title.data();

  core->deinit(core);
  return true;
}

std::string Core::GetSavePath(std::string_view rom_path, int device_number)
{
  std::string save_path =
      fmt::format("{}-{}.sav", rom_path.substr(0, rom_path.find_last_of('.')), device_number + 1);

  if (!Config::Get(Config::MAIN_GBA_SAVES_IN_ROM_PATH))
  {
    save_path =
        File::GetUserPath(D_GBASAVES_IDX) + save_path.substr(save_path.find_last_of("\\/") + 1);
  }

  return save_path;
}
}  // namespace HW::GBA
