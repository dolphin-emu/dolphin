// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceMic.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#include <cubeb/cubeb.h>

#include "AudioCommon/CubebUtils.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

#ifdef _WIN32
#include <objbase.h>
#endif

namespace ExpansionInterface
{
void CEXIMic::StreamInit()
{
  m_stream_buffer = nullptr;
  m_samples_avail = 0;
  m_stream_wpos = 0;
  m_stream_rpos = 0;

#ifdef _WIN32
  if (!m_coinit_success)
    return;
  m_work_queue.PushBlocking([this] {
#endif
    m_cubeb_ctx = CubebUtils::GetContext();
#ifdef _WIN32
  });
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
    m_work_queue.PushBlocking([this] {
#endif
      m_cubeb_ctx.reset();
#ifdef _WIN32
    });
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

  std::lock_guard lk(mic->m_ring_lock);

  const s16* buff_in = static_cast<const s16*>(input_buffer);
  for (long i = 0; i < nframes; i++)
  {
    mic->m_stream_buffer[mic->m_stream_wpos] = buff_in[i];
    mic->m_stream_wpos = (mic->m_stream_wpos + 1) % mic->m_stream_size;
  }

  mic->m_samples_avail += nframes;
  if (mic->m_samples_avail > mic->m_stream_size)
  {
    mic->m_samples_avail = 0;
    mic->m_status.buff_ovrflw = 1;
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
  m_work_queue.PushBlocking([this] {
#endif
    // Open stream with current parameters
    m_stream_size = m_buff_size_samples * 500;
    m_stream_buffer = new s16[m_stream_size];

    cubeb_stream_params params{};
    params.format = CUBEB_SAMPLE_S16LE;
    params.rate = m_sample_rate;
    params.channels = 1;
    params.layout = CUBEB_LAYOUT_MONO;

    u32 minimum_latency;
    if (cubeb_get_min_latency(m_cubeb_ctx.get(), &params, &minimum_latency) != CUBEB_OK)
    {
      WARN_LOG_FMT(EXPANSIONINTERFACE, "Error getting minimum latency");
    }

    if (cubeb_stream_init(m_cubeb_ctx.get(), &m_cubeb_stream,
                          "Dolphin Emulated GameCube Microphone", nullptr, &params, nullptr,
                          nullptr, std::max<u32>(m_buff_size_samples, minimum_latency),
                          DataCallback, state_callback, this) != CUBEB_OK)
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
#endif
}

void CEXIMic::StreamStop()
{
  if (m_cubeb_stream)
  {
#ifdef _WIN32
    m_work_queue.PushBlocking([this] {
#endif
      if (cubeb_stream_stop(m_cubeb_stream) != CUBEB_OK)
        ERROR_LOG_FMT(EXPANSIONINTERFACE, "Error stopping cubeb stream");
      cubeb_stream_destroy(m_cubeb_stream);
      m_cubeb_stream = nullptr;
#ifdef _WIN32
    });
#endif
  }

  m_samples_avail = 0;
  m_stream_wpos = 0;
  m_stream_rpos = 0;

  delete[] m_stream_buffer;
  m_stream_buffer = nullptr;
}

void CEXIMic::StreamReadOne()
{
  std::lock_guard lk(m_ring_lock);

  if (m_samples_avail >= m_buff_size_samples)
  {
    s16* last_buffer = &m_stream_buffer[m_stream_rpos];
    std::memcpy(m_ring_buffer.data(), last_buffer, m_buff_size);

    m_samples_avail -= m_buff_size_samples;

    m_stream_rpos += m_buff_size_samples;
    m_stream_rpos %= m_stream_size;
  }
}

// EXI Mic Device
// This works by opening and starting an input stream when the is_active
// bit is set. The interrupt is scheduled in the future based on sample rate and
// buffer size settings. When the console handles the interrupt, it will send
// cmdGetBuffer, which is when we actually read data from a buffer filled
// in the background.
CEXIMic::CEXIMic(Core::System& system, int index)
    : IEXIDevice(system), m_slot(index)
#ifdef _WIN32
      ,
      m_work_queue("Mic Worker")
#endif
{
#ifdef _WIN32
  m_work_queue.PushBlocking([this] {
    auto result = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    m_coinit_success = result == S_OK;
    m_should_couninit = result == S_OK || result == S_FALSE;
  });
#endif

  StreamInit();
}

CEXIMic::~CEXIMic()
{
  StreamTerminate();

#ifdef _WIN32
  if (m_should_couninit)
  {
    m_work_queue.PushBlocking([this] {
      m_should_couninit = false;
      CoUninitialize();
    });
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
  int diff = (m_system.GetSystemTimers().GetTicksPerSecond() / m_sample_rate) * m_buff_size_samples;
  m_next_int_ticks = m_system.GetCoreTiming().GetTicks() + diff;
  m_system.GetExpansionInterface().ScheduleUpdateInterrupts(CoreTiming::FromThread::CPU, diff);
}

bool CEXIMic::IsInterruptSet()
{
  if (m_next_int_ticks && m_system.GetCoreTiming().GetTicks() >= m_next_int_ticks)
  {
    if (m_status.is_active)
      UpdateNextInterruptTicks();
    else
      m_next_int_ticks = 0;

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
    m_command = byte;  // first byte is command
    byte = 0xFF;       // would be tristate, but we don't care.
    m_position++;
    return;
  }

  int pos = m_position - 1;

  switch (m_command)
  {
  case cmdID:
    byte = EXI_ID[pos];
    break;

  case cmdGetStatus:
    if (pos == 0)
      m_status.button = Pad::GetMicButton(m_slot);

    byte = Common::BitCastPtr<u8>(&m_status)[pos ^ 1];

    if (pos == 1)
      m_status.buff_ovrflw = 0;
    break;

  case cmdSetStatus:
  {
    bool wasactive = m_status.is_active;
    Common::BitCastPtr<u8>(&m_status)[pos ^ 1] = byte;

    // safe to do since these can only be entered if both bytes of status have been written
    if (!wasactive && m_status.is_active)
    {
      m_sample_rate = RATE_BASE << m_status.sample_rate;
      m_buff_size = RING_BASE << m_status.buff_size;
      m_buff_size_samples = m_buff_size / SAMPLE_SIZE;

      UpdateNextInterruptTicks();

      StreamStart();
    }
    else if (wasactive && !m_status.is_active)
    {
      StreamStop();
    }
  }
  break;

  case cmdGetBuffer:
  {
    if (m_ring_pos == 0)
      StreamReadOne();

    byte = m_ring_buffer[m_ring_pos ^ 1];
    m_ring_pos = (m_ring_pos + 1) % m_buff_size;
  }
  break;

  default:
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "EXI MIC: unknown command byte {:02x}", m_command);
    break;
  }

  m_position++;
}
}  // namespace ExpansionInterface
