// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceMic.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#include <cubeb/cubeb.h>

#include "AudioCommon/CubebUtils.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"

#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

#ifdef _WIN32
#include <Objbase.h>
#endif

namespace ExpansionInterface
{
void CEXIMic::StreamInit()
{
  stream_buffer = nullptr;
  samples_avail = stream_wpos = stream_rpos = 0;

#ifdef _WIN32
  if (!m_coinit_success)
    return;
  Common::Event sync_event;
  m_work_queue.Push(
      [this, &sync_event]
      {
        Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
        m_cubeb_ctx = CubebUtils::GetContext();
#ifdef _WIN32
      });
  sync_event.Wait();
#endif
}

void CEXIMic::StreamTerminate()
{
  StreamStop();

  if (m_cubeb_ctx)
  {
#ifdef _WIN32
    if (!m_coinit_success)
      return;
    Common::Event sync_event;
    m_work_queue.Push(
        [this, &sync_event]
        {
          Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
          m_cubeb_ctx.reset();
#ifdef _WIN32
        });
    sync_event.Wait();
#endif
  }
}

static void state_callback(cubeb_stream* stream, void* user_data, cubeb_state state)
{
}

long CEXIMic::DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
    void* /*output_buffer*/, long nframes)
{
  CEXIMic* mic = static_cast<CEXIMic*>(user_data);

  std::lock_guard lk(mic->ring_lock);

  const s16* buff_in = static_cast<const s16*>(input_buffer);
  for (long i = 0; i < nframes; i++)
  {
    mic->stream_buffer[mic->stream_wpos] = buff_in[i];
    mic->stream_wpos = (mic->stream_wpos + 1) % mic->stream_size;
  }

  mic->samples_avail += nframes;
  if (mic->samples_avail > mic->stream_size)
  {
    mic->samples_avail = 0;
    mic->status.buff_ovrflw = 1;
  }

  return nframes;
}

void CEXIMic::StreamStart()
{
  if (!m_cubeb_ctx)
    return;

#ifdef _WIN32
  if (!m_coinit_success)
    return;
  Common::Event sync_event;
  m_work_queue.Push(
      [this, &sync_event]
      {
        Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
        // Open stream with current parameters
        stream_size = buff_size_samples * 500;
        stream_buffer = new s16[stream_size];

        cubeb_stream_params params{};
        params.format = CUBEB_SAMPLE_S16LE;
        params.rate = sample_rate;
        params.channels = 1;
        params.layout = CUBEB_LAYOUT_MONO;

        u32 minimum_latency;
        if (cubeb_get_min_latency(m_cubeb_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
        {
          WARN_LOG_FMT(EXPANSIONINTERFACE, "Error getting minimum latency");
        }

        if (cubeb_stream_init(m_cubeb_ctx.get(), &m_cubeb_stream,
                "Dolphin Emulated GameCube Microphone", nullptr, &params, nullptr, nullptr,
                std::max<u32>(buff_size_samples, minimum_latency), DataCallback, state_callback,
                this) != CUBEB_OK)
        {
          ERROR_LOG_FMT(EXPANSIONINTERFACE, "Error initializing cubeb stream");
          return;
        }

        if (cubeb_stream_start(m_cubeb_stream) != CUBEB_OK)
        {
          ERROR_LOG_FMT(EXPANSIONINTERFACE, "Error starting cubeb stream");
          return;
        }

        INFO_LOG_FMT(EXPANSIONINTERFACE, "started cubeb stream");
#ifdef _WIN32
      });
  sync_event.Wait();
#endif
}

void CEXIMic::StreamStop()
{
  if (m_cubeb_stream)
  {
#ifdef _WIN32
    Common::Event sync_event;
    m_work_queue.Push(
        [this, &sync_event]
        {
          Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
#endif
          if (cubeb_stream_stop(m_cubeb_stream) != CUBEB_OK)
            ERROR_LOG_FMT(EXPANSIONINTERFACE, "Error stopping cubeb stream");
          cubeb_stream_destroy(m_cubeb_stream);
          m_cubeb_stream = nullptr;
#ifdef _WIN32
        });
    sync_event.Wait();
#endif
  }

  samples_avail = stream_wpos = stream_rpos = 0;

  delete[] stream_buffer;
  stream_buffer = nullptr;
}

void CEXIMic::StreamReadOne()
{
  std::lock_guard lk(ring_lock);

  if (samples_avail >= buff_size_samples)
  {
    s16* last_buffer = &stream_buffer[stream_rpos];
    std::memcpy(ring_buffer, last_buffer, buff_size);

    samples_avail -= buff_size_samples;

    stream_rpos += buff_size_samples;
    stream_rpos %= stream_size;
  }
}

// EXI Mic Device
// This works by opening and starting an input stream when the is_active
// bit is set. The interrupt is scheduled in the future based on sample rate and
// buffer size settings. When the console handles the interrupt, it will send
// cmdGetBuffer, which is when we actually read data from a buffer filled
// in the background.

u8 const CEXIMic::exi_id[] = {0, 0x0a, 0, 0, 0};

CEXIMic::CEXIMic(Core::System& system, int index)
    : IEXIDevice(system)
    , slot(index)
#ifdef _WIN32
    , m_work_queue("Mic Worker")
#endif
{
  m_position = 0;
  command = 0;
  status.U16 = 0;

  sample_rate = rate_base;
  buff_size = ring_base;
  buff_size_samples = buff_size / sample_size;

  ring_pos = 0;
  std::memset(ring_buffer, 0, sizeof(ring_buffer));

  next_int_ticks = 0;

#ifdef _WIN32
  Common::Event sync_event;
  m_work_queue.Push(
      [this, &sync_event]
      {
        Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
        auto result = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
        m_coinit_success = result == S_OK;
        m_should_couninit = result == S_OK || result == S_FALSE;
      });
  sync_event.Wait();
#endif

  StreamInit();
}

CEXIMic::~CEXIMic()
{
  StreamTerminate();

#ifdef _WIN32
  if (m_should_couninit)
  {
    Common::Event sync_event;
    m_work_queue.Push(
        [this, &sync_event]
        {
          Common::ScopeGuard sync_event_guard([&sync_event] { sync_event.Set(); });
          m_should_couninit = false;
          CoUninitialize();
        });
    sync_event.Wait();
  }
  m_coinit_success = false;
#endif
}

bool CEXIMic::IsPresent() const
{
  return true;
}

void CEXIMic::SetCS(int cs)
{
  if (cs)  // not-selected to selected
    m_position = 0;
  // Doesn't appear to do anything we care about
  // else if (command == cmdReset)
}

void CEXIMic::UpdateNextInterruptTicks()
{
  int diff = (m_system.GetSystemTimers().GetTicksPerSecond() / sample_rate) * buff_size_samples;
  next_int_ticks = m_system.GetCoreTiming().GetTicks() + diff;
  m_system.GetExpansionInterface().ScheduleUpdateInterrupts(CoreTiming::FromThread::CPU, diff);
}

bool CEXIMic::IsInterruptSet()
{
  if (next_int_ticks && m_system.GetCoreTiming().GetTicks() >= next_int_ticks)
  {
    if (status.is_active)
      UpdateNextInterruptTicks();
    else
      next_int_ticks = 0;

    return true;
  }
  else
  {
    return false;
  }
}

void CEXIMic::TransferByte(u8& byte)
{
  if (m_position == 0)
  {
    command = byte;  // first byte is command
    byte = 0xFF;     // would be tristate, but we don't care.
    m_position++;
    return;
  }

  int pos = m_position - 1;

  switch (command)
  {
  case cmdID:
    byte = exi_id[pos];
    break;

  case cmdGetStatus:
    if (pos == 0)
      status.button = Pad::GetMicButton(slot);

    byte = status.U8[pos ^ 1];

    if (pos == 1)
      status.buff_ovrflw = 0;
    break;

  case cmdSetStatus:
  {
    bool wasactive = status.is_active;
    status.U8[pos ^ 1] = byte;

    // safe to do since these can only be entered if both bytes of status have been written
    if (!wasactive && status.is_active)
    {
      sample_rate = rate_base << status.sample_rate;
      buff_size = ring_base << status.buff_size;
      buff_size_samples = buff_size / sample_size;

      UpdateNextInterruptTicks();

      StreamStart();
    }
    else if (wasactive && !status.is_active)
    {
      StreamStop();
    }
  }
  break;

  case cmdGetBuffer:
  {
    if (ring_pos == 0)
      StreamReadOne();

    byte = ring_buffer[ring_pos ^ 1];
    ring_pos = (ring_pos + 1) % buff_size;
  }
  break;

  default:
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "EXI MIC: unknown command byte {:02x}", command);
    break;
  }

  m_position++;
}
}  // namespace ExpansionInterface
