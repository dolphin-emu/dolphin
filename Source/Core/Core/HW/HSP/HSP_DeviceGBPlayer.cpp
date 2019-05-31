// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/HSP/HSP_DeviceGBPlayer.h"

#include <mgba/flags.h>

#include <mgba-util/vfs.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/gba/core.h>
#include <mgba/gba/interface.h>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DSP.h"
#include "Core/HW/HSP/HSP.h"
#include "Core/HW/ProcessorInterface.h"

#include <condition_variable>
#include <cstring>
#include <mutex>

namespace HSP
{
enum GBPRegister
{
  GBP_TEST = 0x10,
  GBP_VIDEO = 0x11,
  GBP_CONTROL = 0x14,
  GBP_SIO_CONTROL = 0x15,
  GBP_AUDIO = 0x18,
  GBP_SIO = 0x19,
  GBP_KEYPAD = 0x1C,
  GBP_IRQ = 0x1D
};

enum
{
  IRQ_ASSERTED = 0x8000
};

enum
{
  CONTROL_CART_DETECTED = 0x01,
  CONTROL_CART_INSERTED = 0x02,
  CONTROL_3V = 0x04,
  CONTROL_5V = 0x08,
  CONTROL_MASK_IRQ = 0x10,
  CONTROL_SLEEP = 0x20,
  CONTROL_LINK_CABLE = 0x40,
  CONTROL_LINK_ENABLE = 0x80
};

enum class CHSPDevice_GBPlayer::IRQ : int
{
  LINK = 0,
  GAME_PAK = 1,
  SLEEP = 2,
  SERIAL = 3,
  VIDEO = 4,
  AUDIO = 5
};

class CGBPlayer_mGBA : public IGBPlayer
{
public:
  explicit CGBPlayer_mGBA(CHSPDevice_GBPlayer*);
  ~CGBPlayer_mGBA() override;

  void Reset() override;
  void Stop() override;
  void Shutdown() override;

  bool IsLoaded() const override;
  bool IsGBA() const override;

  bool LoadGame(const std::string& path) override;
  bool LoadBootrom(const std::string& path) override;

  void ReadScanlines(u32* scanlines) override;
  void ReadAudio(u8* audio) override;

  void SetKeys(u16 keys) override;

private:
  const static size_t GB_VIDEO_OFFSET = 1960;
  mCore* m_core = nullptr;
  u32* m_framebuffer;
  CoreTiming::EventType* m_gbp_update_audio_event = nullptr;
  CoreTiming::EventType* m_gbp_update_video_event = nullptr;
  bool m_game = false;

  bool m_frame_ready = false;
  bool m_scanlines_read = false;
  int m_next_scanlines = 0;
  int m_next_audio = 0;
  int m_last_audio = 0;
  u16 m_keys = 0;
  s16 m_audio[2][600] = {};

  mLogger s_logger;

  std::thread m_thread;
  std::mutex m_mutex;
  std::condition_variable m_cond_gba;
  std::condition_variable m_cond_gc;

  static void UpdateAudio(u64 user_data, s64 cycles_late);
  static void UpdateVideo(u64 user_data, s64 cycles_late);
  void UpdateAudio(s64 cycles_late);
  void UpdateVideo(s64 cycles_late);

  void GBAThread();
};

CHSPDevice_GBPlayer::CHSPDevice_GBPlayer(HSPDevices device) : IHSPDevice(device)
{
  m_gbp = std::make_unique<CGBPlayer_mGBA>(this);

  if (!SConfig::GetInstance().m_GBPGame.empty())
    m_gbp->LoadGame(SConfig::GetInstance().m_GBPGame);
  if (m_gbp->IsGBA() && !SConfig::GetInstance().m_GBPBootrom.empty())
    m_gbp->LoadBootrom(SConfig::GetInstance().m_GBPBootrom);
  if (!m_gbp->IsGBA() && !SConfig::GetInstance().m_GBPBootromCgb.empty())
    m_gbp->LoadBootrom(SConfig::GetInstance().m_GBPBootromCgb);
}

u64 CHSPDevice_GBPlayer::Read(u32 address)
{
  u64 value = 0;
  switch (address >> 20)
  {
  case GBP_TEST:
    std::memcpy(&value, &m_test[address & 0x1F], sizeof(value));
    break;
  case GBP_CONTROL:
    if (m_gbp->IsLoaded())
    {
      m_control |= CONTROL_CART_INSERTED;
      if ((m_control & CONTROL_3V) && (m_gbp->IsGBA() || (m_control & CONTROL_5V)))
        m_control |= CONTROL_CART_DETECTED;
      else
        m_control &= ~CONTROL_CART_DETECTED;
    }
    else
    {
      m_control &= ~(CONTROL_CART_DETECTED | CONTROL_CART_INSERTED);
    }
    value = m_control * 0x0101010101010101ULL;
    break;
  case GBP_IRQ:
    value = m_irq * 0x0000000100000001ULL;
    value |= (m_irq & 0xFF00) * 0x0001010000010100ULL;
    value &= 0xFFFF7FFFFFFF7FFFULL;
    break;
  case GBP_VIDEO:
    if (!(address & 0xFF8))
      m_gbp->ReadScanlines(m_scanlines.data());
    value = (u64)m_scanlines[(address & 0xFF8) / 4] << 32;
    value |= m_scanlines[(address & 0xFF8) / 4 + 1];
    break;
  case GBP_AUDIO:
    if (!(address & 0xFF8))
      m_gbp->ReadAudio(m_audio.data());
    value = 0x0101010100000000ULL * m_audio[(address & 0xFF8) / 4];
    value |= 0x01010101ULL * m_audio[(address & 0xFF8) / 4 + 1];
    break;
  default:
    WARN_LOG(HSP, "Unknown GBP read from 0x%08x", address);
    break;
  }
  return value;
}

void CHSPDevice_GBPlayer::Write(u32 address, u64 value)
{
  switch (address >> 20)
  {
  case GBP_TEST:
    value = ~value;
    std::memcpy(&m_test[address & 0x1F], &value, sizeof(value));
    break;
  case GBP_CONTROL:
    if ((address & 0x1F) != 0x18)
      break;
    if (!(m_control & (CONTROL_3V | CONTROL_5V)) && (value & (CONTROL_3V | CONTROL_5V)))
      m_gbp->Reset();
    if ((m_control & (CONTROL_3V | CONTROL_5V)) && !(value & (CONTROL_3V | CONTROL_5V)))
      m_gbp->Stop();
    m_control = value & 0xFC;
    if (m_control & CONTROL_MASK_IRQ)
      ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_HSP, false);
    else
      ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_HSP, m_irq & IRQ_ASSERTED);
    break;
  case GBP_IRQ:
    if ((address & 0x1F) != 0x18)
      break;
    m_irq &= ~value;
    ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_HSP, m_irq & IRQ_ASSERTED);
    break;
  case GBP_KEYPAD:
    if ((address & 0x1F) != 0x18)
      break;
    m_gbp->SetKeys(value & 0x3FF);
    break;
  default:
    WARN_LOG(HSP, "Unknown GBP write to 0x%08x: 0x%016lx", address, value);
    break;
  }
}

void CHSPDevice_GBPlayer::DoState(PointerWrap& p)
{
  p.Do(m_test);
  p.Do(m_control);
  p.Do(m_irq);
  p.Do(m_scanlines);
}

void CHSPDevice_GBPlayer::AssertIRQ(IRQ irq)
{
  m_irq |= 1 << (2 * static_cast<int>(irq));
  if (!(m_control & CONTROL_MASK_IRQ) && !(m_irq & IRQ_ASSERTED))
    ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_HSP, true);
  m_irq |= IRQ_ASSERTED;
}

IGBPlayer::IGBPlayer(CHSPDevice_GBPlayer* player) : m_player(player)
{
}

IGBPlayer::~IGBPlayer()
{
  Shutdown();
}

void IGBPlayer::Shutdown()
{
}

CGBPlayer_mGBA::CGBPlayer_mGBA(CHSPDevice_GBPlayer* player) : IGBPlayer(player)
{
  m_framebuffer = static_cast<u32*>(
      Common::AllocateMemoryPages(GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * 4));
  memset(m_framebuffer, 0, GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * 4);
  if (!m_gbp_update_audio_event)
    m_gbp_update_audio_event =
        CoreTiming::RegisterEvent("GBPmGBAUpdateAudioEvent", CGBPlayer_mGBA::UpdateAudio);
  if (!m_gbp_update_video_event)
    m_gbp_update_video_event =
        CoreTiming::RegisterEvent("GBPmGBAUpdateVideoEvent", CGBPlayer_mGBA::UpdateVideo);

  s_logger.log = [](mLogger*, int, mLogLevel, const char*, va_list) {};
  s_logger.filter = nullptr;
  mLogSetDefaultLogger(&s_logger);
}

CGBPlayer_mGBA::~CGBPlayer_mGBA()
{
  Shutdown();
  Common::FreeMemoryPages(m_framebuffer,
                          GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * 4);
  m_framebuffer = nullptr;
}

void CGBPlayer_mGBA::Reset()
{
  std::unique_lock<std::mutex> guard(m_mutex);
  if (m_core)
  {
    m_core->reset(m_core);
    m_core->setAudioBufferSize(m_core, 600);
    blip_set_rates(m_core->getAudioChannel(m_core, 0), m_core->frequency(m_core), 0x8000);
    blip_set_rates(m_core->getAudioChannel(m_core, 1), m_core->frequency(m_core), 0x8000);
  }
  if (!m_thread.joinable())
    m_thread = std::thread(&CGBPlayer_mGBA::GBAThread, this);
  guard.unlock();
  m_cond_gba.notify_one();
  CoreTiming::ScheduleEvent(10000000, m_gbp_update_audio_event, (u64)this);
  CoreTiming::ScheduleEvent(10000000, m_gbp_update_video_event, (u64)this);
}

void CGBPlayer_mGBA::Stop()
{
  CoreTiming::RemoveEvent(m_gbp_update_audio_event);
  CoreTiming::RemoveEvent(m_gbp_update_video_event);
}

void CGBPlayer_mGBA::Shutdown()
{
  mCore* core = m_core;
  std::unique_lock<std::mutex> guard(m_mutex);
  if (m_core)
    m_core = nullptr;
  guard.unlock();
  m_cond_gba.notify_one();
  if (m_thread.joinable())
    m_thread.join();
  if (core)
  {
    mCoreConfigDeinit(&core->config);
    core->deinit(core);
  }
  m_game = false;
}

bool CGBPlayer_mGBA::IsLoaded() const
{
  return m_core && m_game;
}

bool CGBPlayer_mGBA::IsGBA() const
{
  return m_core && m_core->platform(m_core) == PLATFORM_GBA;
}

bool CGBPlayer_mGBA::LoadGame(const std::string& path)
{
  if (m_core)
    Shutdown();

  m_core = mCoreFind(path.c_str());
  if (!m_core)
    return false;
  m_game = true;
  m_core->init(m_core);
  mCoreInitConfig(m_core, "dolphin");
  if (m_core->platform(m_core) == PLATFORM_GBA)
    m_core->setVideoBuffer(m_core, m_framebuffer, GBA_VIDEO_HORIZONTAL_PIXELS);
  else
    m_core->setVideoBuffer(m_core, m_framebuffer + GB_VIDEO_OFFSET, GBA_VIDEO_HORIZONTAL_PIXELS);
  mCoreLoadFile(m_core, path.c_str());
  mCoreAutoloadSave(m_core);
  return true;
}

bool CGBPlayer_mGBA::LoadBootrom(const std::string& path)
{
  VFile* vf = VFileOpen(path.c_str(), O_RDONLY);
  if (!vf)
    return false;
  if (!m_core)
  {
    m_core = GBACoreCreate();
    m_core->init(m_core);
    mCoreInitConfig(m_core, "dolphin");
    m_core->setVideoBuffer(m_core, m_framebuffer, GBA_VIDEO_HORIZONTAL_PIXELS);
    m_game = false;
  }
  m_core->loadBIOS(m_core, vf, 0);
  return true;
}

void CGBPlayer_mGBA::ReadScanlines(u32* scanlines)
{
  for (int i = 0; i < GBA_VIDEO_HORIZONTAL_PIXELS * 4; ++i)
  {
    u16 color = M_RGB8_TO_RGB5(m_framebuffer[m_next_scanlines * GBA_VIDEO_HORIZONTAL_PIXELS + i]);
    u32 c = color & 0xFF;
    c |= (color & 0xFF00) << 8;
    scanlines[i] = c | (c << 8);  // Parity
  }

  if (!m_next_scanlines)
    scanlines[0] |= 0x80800000;
  m_scanlines_read = true;
  m_next_scanlines += 4;
}

static u8 ToBits(s16& value)
{
  static const u8 bitmap[] = {0, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE};
  if (value >= 8)
  {
    value -= 8;
    return 0xFF;
  }
  u8 bits = bitmap[value];
  value = 0;
  return bits;
}

void CGBPlayer_mGBA::ReadAudio(u8* audio)
{
  if (m_next_audio >= m_last_audio)
    m_next_audio = m_last_audio - 8;  // XXX: Fix sync properly

  for (int i = 0; i < 8; ++i)
  {
    s16 l = (m_audio[0][m_next_audio + i] + 0x8000) >> 7;
    s16 r = (m_audio[1][m_next_audio + i] + 0x8000) >> 7;

    for (int j = 0; j < 0x40; ++j)
    {
      audio[i * 0x80 + j * 2] = ToBits(l);
      audio[i * 0x80 + j] = ToBits(r);
    }
  }
  m_next_audio += 8;
}

void CGBPlayer_mGBA::SetKeys(u16 keys)
{
  m_keys = keys;
}

void CGBPlayer_mGBA::UpdateAudio(u64 user_data, s64 cycles_late)
{
  CGBPlayer_mGBA* self = reinterpret_cast<CGBPlayer_mGBA*>(user_data);
  self->UpdateAudio(cycles_late);
}

void CGBPlayer_mGBA::UpdateVideo(u64 user_data, s64 cycles_late)
{
  CGBPlayer_mGBA* self = reinterpret_cast<CGBPlayer_mGBA*>(user_data);
  self->UpdateVideo(cycles_late);
}

void CGBPlayer_mGBA::UpdateAudio(s64 cycles_late)
{
  if (!m_core)
    return;
  if (m_next_audio >= m_last_audio)
  {
    std::unique_lock<std::mutex> guard(m_mutex);
    if (!m_frame_ready)
      m_cond_gc.wait(guard);
    guard.unlock();
    m_next_audio = 0;
    m_last_audio = blip_read_samples(m_core->getAudioChannel(m_core, 0), m_audio[0], 600, 0);
    blip_read_samples(m_core->getAudioChannel(m_core, 1), m_audio[1], 600, 0);
  }
  m_player->AssertIRQ(CHSPDevice_GBPlayer::IRQ::AUDIO);
  if (m_next_scanlines < GBA_VIDEO_VERTICAL_PIXELS && m_scanlines_read)
  {
    m_player->AssertIRQ(CHSPDevice_GBPlayer::IRQ::VIDEO);
    m_scanlines_read = false;
  }
  // Roughly 4096 Hz
  CoreTiming::ScheduleEvent(118652 - cycles_late, m_gbp_update_audio_event, (u64)this);
}

void CGBPlayer_mGBA::UpdateVideo(s64 cycles_late)
{
  if (!m_core)
    return;
  if (m_next_scanlines == GBA_VIDEO_VERTICAL_PIXELS)
  {
    std::unique_lock<std::mutex> guard(m_mutex);
    m_frame_ready = false;
    m_cond_gba.notify_one();
    m_next_scanlines = 228;
    // Approximately 68 GBA scanlines
    CoreTiming::ScheduleEvent(2426811 - cycles_late, m_gbp_update_video_event, (u64)this);
  }
  else if (m_next_scanlines < GBA_VIDEO_VERTICAL_PIXELS)
  {
    m_scanlines_read = true;
    {
      std::unique_lock<std::mutex> guard(m_mutex);
      m_cond_gba.notify_one();
    }
    CoreTiming::ScheduleEvent(5710144 - cycles_late, m_gbp_update_video_event, (u64)this);
  }
  else if (m_next_scanlines == 228)
  {
    m_next_scanlines = 0;
    m_scanlines_read = true;
    {
      std::unique_lock<std::mutex> guard(m_mutex);
      if (!m_frame_ready)
        m_cond_gc.wait(guard);
    }
    // Approximately 160 GBA scanlines
    CoreTiming::ScheduleEvent(5710144 - cycles_late, m_gbp_update_video_event, (u64)this);
  }
}

void CGBPlayer_mGBA::GBAThread()
{
  Common::SetCurrentThreadName("GBA thread");
  while (m_core)
  {
    std::unique_lock<std::mutex> guard(m_mutex);
    if (!m_frame_ready)
    {
      guard.unlock();
      m_core->setKeys(m_core, m_keys);
      m_core->runFrame(m_core);
      guard.lock();
    }
    m_frame_ready = true;
    m_cond_gc.notify_one();
    m_cond_gba.wait(guard);
  }
}
}  // namespace HSP
