// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "Common/CommonTypes.h"
#include "Common/WorkQueueThread.h"
#include "Core/HW/EXI/EXI_Device.h"

struct cubeb;
struct cubeb_stream;

namespace ExpansionInterface
{
class CEXIMic : public IEXIDevice
{
public:
  CEXIMic(Core::System& system, const int index);
  virtual ~CEXIMic();
  void SetCS(u32 cs, bool was_selected, bool is_selected) override;
  bool IsInterruptSet() override;
  bool IsPresent() const override;

private:
  static u8 const exi_id[];
  static int const sample_size = sizeof(s16);
  static int const rate_base = 11025;
  static int const ring_base = 32;

  enum
  {
    cmdID = 0x00,
    cmdGetStatus = 0x40,
    cmdSetStatus = 0x80,
    cmdGetBuffer = 0x20,
    cmdReset = 0xFF,
  };

  int slot;

  u32 m_position;
  int command;
  union UStatus
  {
    u16 U16;
    u8 U8[2];
    struct
    {
      u16 out : 4;          // MICSet/GetOut...???
      u16 id : 1;           // Used for MICGetDeviceID (always 0)
      u16 button_unk : 3;   // Button bits which appear unused
      u16 button : 1;       // The actual button on the mic
      u16 buff_ovrflw : 1;  // Ring buffer wrote over bytes which weren't read by console
      u16 gain : 1;         // Gain: 0dB or 15dB
      u16 sample_rate : 2;  // Sample rate, 00-11025, 01-22050, 10-44100, 11-??
      u16 buff_size : 2;    // Ring buffer size in bytes, 00-32, 01-64, 10-128, 11-???
      u16 is_active : 1;    // If we are sampling or not
    };
  };

  static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                           void* output_buffer, long nframes);

  void TransferByte(u8& byte) override;

  void StreamInit();
  void StreamTerminate();
  void StreamStart();
  void StreamStop();
  void StreamReadOne();

  // 64 is the max size, can be 16 or 32 as well
  int ring_pos;
  u8 ring_buffer[64 * sample_size];

  // 0 to disable interrupts, else it will be checked against current CPU ticks
  // to determine if interrupt should be raised
  u64 next_int_ticks;
  void UpdateNextInterruptTicks();

  // Streaming input interface
  std::shared_ptr<cubeb> m_cubeb_ctx = nullptr;
  cubeb_stream* m_cubeb_stream = nullptr;

  UStatus status;

  std::mutex ring_lock;

  // status bits converted to nice numbers
  int sample_rate;
  int buff_size;
  int buff_size_samples;

  // Arbitrarily small ringbuffer used by audio input backend in order to
  // keep delay tolerable
  s16* stream_buffer;
  int stream_size;
  int stream_wpos;
  int stream_rpos;
  int samples_avail;

#ifdef _WIN32
  Common::WorkQueueThread<std::function<void()>> m_work_queue;
  bool m_coinit_success = false;
  bool m_should_couninit = false;
#endif
};
}  // namespace ExpansionInterface
