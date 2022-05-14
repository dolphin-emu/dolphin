// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "Common/BitField.h"
#include "Common/BitField2.h"
#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_Device.h"

struct cubeb;
struct cubeb_stream;

namespace ExpansionInterface
{
class CEXIMic : public IEXIDevice
{
public:
  CEXIMic(const int index);
  virtual ~CEXIMic();
  void SetCS(int cs) override;
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
  struct UStatus : BitField2<u16>
  {
    FIELDARRAY(u8, 0, 8, 2, byte);

    FIELD(u16, 0, 4, out);           // MICSet/GetOut...???
    FIELD(u16, 4, 1, id);            // Used for MICGetDeviceID (always 0)
    FIELD(u16, 5, 3, button_unk);    // Button bits which appear unused
    FIELD(bool, 8, 1, button);       // The actual button on the mic
    FIELD(bool, 9, 1, buff_ovrflw);  // Ring buffer wrote over bytes which weren't read by
                                     // console
    FIELD(bool, 10, 1, gain);        // Gain: 0dB or 15dB
    FIELD(u16, 11, 2, sample_rate);  // Sample rate
                                     // 00-11025, 01-22050, 10-44100, 11-??
    FIELD(u16, 13, 2, buff_size);    // Ring buffer size in bytes
                                     // 00-32, 01-64, 10-128, 11-???
    FIELD(bool, 15, 1, is_active);   // If we are sampling or not

    UStatus() = default;
    UStatus(u16 val) : BitField2(val) {}
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
};
}  // namespace ExpansionInterface
