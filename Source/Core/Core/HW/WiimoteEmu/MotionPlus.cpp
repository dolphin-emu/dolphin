// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/MotionPlus.h"

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

namespace WiimoteEmu
{
MotionPlus::MotionPlus() : Extension("MotionPlus")
{
}

void MotionPlus::Reset()
{
  reg_data = {};

  constexpr std::array<u8, 6> initial_id = {0x00, 0x00, 0xA6, 0x20, 0x00, 0x05};

  // FYI: This ID changes on activation
  std::copy(std::begin(initial_id), std::end(initial_id), reg_data.ext_identifier);

  // TODO: determine meaning of calibration data:
  static const u8 cdata[32] = {
      0x78, 0xd9, 0x78, 0x38, 0x77, 0x9d, 0x2f, 0x0c, 0xcf, 0xf0, 0x31,
      0xad, 0xc8, 0x0b, 0x5e, 0x39, 0x6f, 0x81, 0x7b, 0x89, 0x78, 0x51,
      0x33, 0x60, 0xc9, 0xf5, 0x37, 0xc1, 0x2d, 0xe9, 0x15, 0x8d,
  };

  std::copy(std::begin(cdata), std::end(cdata), reg_data.calibration_data);

  // TODO: determine the meaning behind this:
  static const u8 cert[64] = {
      0x99, 0x1a, 0x07, 0x1b, 0x97, 0xf1, 0x11, 0x78, 0x0c, 0x42, 0x2b, 0x68, 0xdf,
      0x44, 0x38, 0x0d, 0x2b, 0x7e, 0xd6, 0x84, 0x84, 0x58, 0x65, 0xc9, 0xf2, 0x95,
      0xd9, 0xaf, 0xb6, 0xc4, 0x87, 0xd5, 0x18, 0xdb, 0x67, 0x3a, 0xc0, 0x71, 0xec,
      0x3e, 0xf4, 0xe6, 0x7e, 0x35, 0xa3, 0x29, 0xf8, 0x1f, 0xc5, 0x7c, 0x3d, 0xb9,
      0x56, 0x22, 0x95, 0x98, 0x8f, 0xfb, 0x66, 0x3e, 0x9a, 0xdd, 0xeb, 0x7e,
  };

  std::copy(std::begin(cert), std::end(cert), reg_data.cert_data);
}

void MotionPlus::DoState(PointerWrap& p)
{
  p.Do(reg_data);
}

bool MotionPlus::IsActive() const
{
  return ACTIVE_DEVICE_ADDR << 1 == reg_data.ext_identifier[2];
}

MotionPlus::PassthroughMode MotionPlus::GetPassthroughMode() const
{
  return static_cast<PassthroughMode>(reg_data.ext_identifier[4]);
}

ExtensionPort& MotionPlus::GetExtPort()
{
  return m_extension_port;
}

int MotionPlus::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  if (IsActive())
  {
    // Motion plus does not respond to 0x53 when activated
    if (ACTIVE_DEVICE_ADDR == slave_addr)
      return RawRead(&reg_data, addr, count, data_out);
    else
      return 0;
  }
  else
  {
    if (INACTIVE_DEVICE_ADDR == slave_addr)
      return RawRead(&reg_data, addr, count, data_out);
    else
    {
      // Passthrough to the connected extension (if any)
      return i2c_bus.BusRead(slave_addr, addr, count, data_out);
    }
  }
}

int MotionPlus::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  if (IsActive())
  {
    // Motion plus does not respond to 0x53 when activated
    if (ACTIVE_DEVICE_ADDR == slave_addr)
    {
      auto const result = RawWrite(&reg_data, addr, count, data_in);

      // It seems a write of any value triggers deactivation.
      // TODO: kill magic number
      if (0xf0 == addr)
      {
        // Deactivate motion plus:
        reg_data.ext_identifier[2] = INACTIVE_DEVICE_ADDR << 1;
        reg_data.cert_ready = 0x0;

        // Pass through the activation write to the attached extension:
        // The M+ deactivation signal is cleverly the same as EXT activation:
        i2c_bus.BusWrite(slave_addr, addr, count, data_in);
      }
      // TODO: kill magic number
      else if (0xf1 == addr)
      {
        INFO_LOG(WIIMOTE, "M+ cert activation: 0x%x", reg_data.cert_enable);
        // 0x14,0x18 is also a valid value
        // 0x1a is final value
        reg_data.cert_ready = 0x18;
      }
      // TODO: kill magic number
      else if (0xf2 == addr)
      {
        INFO_LOG(WIIMOTE, "M+ calibration ?? : 0x%x", reg_data.unknown_0xf2[0]);
      }

      return result;
    }
    else
    {
      // No i2c passthrough when activated.
      return 0;
    }
  }
  else
  {
    if (INACTIVE_DEVICE_ADDR == slave_addr)
    {
      auto const result = RawWrite(&reg_data, addr, count, data_in);

      // It seems a write of any value triggers activation.
      if (0xfe == addr)
      {
        INFO_LOG(WIIMOTE, "M+ has been activated: %d", data_in[0]);

        // Activate motion plus:
        reg_data.ext_identifier[2] = ACTIVE_DEVICE_ADDR << 1;
        // TODO: kill magic number
        // reg_data.cert_ready = 0x2;

        // A real M+ is unresponsive on the bus for some time during activation
        // Reads fail to ack and ext data gets filled with 0xff for a frame or two
        // I don't think we need to emulate that.

        // TODO: activate extension and disable encrption
        // also do this if an extension is attached after activation.
        std::array<u8, 1> data = {0x55};
        i2c_bus.BusWrite(ACTIVE_DEVICE_ADDR, 0xf0, (int)data.size(), data.data());
      }

      return result;
    }
    else
    {
      // Passthrough to the connected extension (if any)
      return i2c_bus.BusWrite(slave_addr, addr, count, data_in);
    }
  }
}

bool MotionPlus::ReadDeviceDetectPin() const
{
  if (IsActive())
  {
    return true;
  }
  else
  {
    // When inactive the device detect pin reads from the ext port:
    return m_extension_port.IsDeviceConnected();
  }
}

bool MotionPlus::IsButtonPressed() const
{
  return false;
}

void MotionPlus::Update()
{
  if (!IsActive())
  {
    return;
  }

  auto& data = reg_data.controller_data;

  if (0x0 == reg_data.cert_ready)
  {
    // Without sending this nonsense, inputs are unresponsive.. even regular buttons
    // Device still operates when changing the data slightly so its not any sort of encrpytion
    // It even works when removing the is_mp_data bit in the last byte
    // My M+ non-inside gives: 61,46,45,aa,0,2 or b6,46,45,9a,0,2
    // static const u8 init_data[6] = {0x8e, 0xb0, 0x4f, 0x5a, 0xfc | 0x01, 0x02};
    static const u8 init_data[6] = {0x81, 0x46, 0x46, 0xb6, 0x01, 0x02};
    std::copy(std::begin(init_data), std::end(init_data), data);
    reg_data.cert_ready = 0x2;
    return;
  }

  if (0x2 == reg_data.cert_ready)
  {
    static const u8 init_data[6] = {0x7f, 0xcf, 0xdf, 0x8b, 0x4f, 0x82};
    std::copy(std::begin(init_data), std::end(init_data), data);
    reg_data.cert_ready = 0x8;
    return;
  }

  if (0x8 == reg_data.cert_ready)
  {
    // A real wiimote takes about 2 seconds to reach this state:
    reg_data.cert_ready = 0xe;
  }

  if (0x18 == reg_data.cert_ready)
  {
    // TODO: determine the meaning of this
    const u8 mp_cert2[64] = {
        0xa5, 0x84, 0x1f, 0xd6, 0xbd, 0xdc, 0x7a, 0x4c, 0xf3, 0xc0, 0x24, 0xe0, 0x92,
        0xef, 0x19, 0x28, 0x65, 0xe0, 0x62, 0x7c, 0x9b, 0x41, 0x6f, 0x12, 0xc3, 0xac,
        0x78, 0xe4, 0xfc, 0x6b, 0x7b, 0x0a, 0xb4, 0x50, 0xd6, 0xf2, 0x45, 0xf7, 0x93,
        0x04, 0xaf, 0xf2, 0xb7, 0x26, 0x94, 0xee, 0xad, 0x92, 0x05, 0x6d, 0xe5, 0xc6,
        0xd6, 0x36, 0xdc, 0xa5, 0x69, 0x0f, 0xc8, 0x99, 0xf2, 0x1c, 0x4e, 0x0d,
    };

    std::copy(std::begin(mp_cert2), std::end(mp_cert2), reg_data.cert_data);

    if (0x01 != reg_data.cert_enable)
    {
      PanicAlert("M+ Failure! Game requested cert2 with value other than 0x01. M+ will disconnect "
                 "shortly unfortunately. Reconnect wiimote and hope for the best.");
    }

    // A real wiimote takes about 2 seconds to reach this state:
    reg_data.cert_ready = 0x1a;
    INFO_LOG(WIIMOTE, "M+ cert 2 ready!");
  }

  // TODO: make sure a motion plus report is sent first after init

  // On real mplus:
  // For some reason the first read seems to have garbage data
  // is_mp_data and extension_connected are set, but the data is junk
  // it does seem to have some sort of pattern though, byte 5 is always 2
  // something like: d5, b0, 4e, 6e, fc, 2
  // When a passthrough mode is set:
  // the second read is valid mplus data, which then triggers a read from the extension
  // the third read is finally extension data
  // If an extension is not attached the data is always mplus data
  // even when passthrough is enabled

  // Real M+ seems to only ever read 6 bytes from the extension.
  // Data after 6 bytes seems to be zero-filled.
  // After reading, the real M+ uses that data for the next frame.
  // But we are going to use it for the current frame instead.
  constexpr int EXT_AMT = 6;
  // Always read from 0x52 @ 0x00:
  constexpr u8 EXT_SLAVE = ExtensionPort::REPORT_I2C_SLAVE;
  constexpr u8 EXT_ADDR = ExtensionPort::REPORT_I2C_ADDR;

  // Try to alternate between M+ and EXT data:
  auto& mplus_data = *reinterpret_cast<DataFormat*>(data);
  mplus_data.is_mp_data ^= true;

  // hax!!!
  // static const u8 hacky_mp_data[6] = {0x1d, 0x91, 0x49, 0x87, 0x73, 0x7a};
  // static const u8 hacky_nc_data[6] = {0x79, 0x7f, 0x4b, 0x83, 0x8b, 0xec};
  // auto& hacky_ptr = mplus_data.is_mp_data ? hacky_mp_data : hacky_nc_data;
  // std::copy(std::begin(hacky_ptr), std::end(hacky_ptr), data);
  // return;

  // If the last frame had M+ data try to send some non-M+ data:
  if (!mplus_data.is_mp_data)
  {
    switch (GetPassthroughMode())
    {
    case PassthroughMode::DISABLED:
    {
      // Passthrough disabled, always send M+ data:
      mplus_data.is_mp_data = true;
      break;
    }
    case PassthroughMode::NUNCHUK:
    {
      if (EXT_AMT == i2c_bus.BusRead(EXT_SLAVE, EXT_ADDR, EXT_AMT, data))
      {
        // Passthrough data modifications via wiibrew.org
        // Data passing through drops the least significant bit of the three accelerometer values
        // Bit 7 of byte 5 is moved to bit 6 of byte 5, overwriting it
        Common::SetBit(data[5], 6, Common::ExtractBit(data[5], 7));
        // Bit 0 of byte 4 is moved to bit 7 of byte 5
        Common::SetBit(data[5], 7, Common::ExtractBit(data[4], 0));
        // Bit 3 of byte 5 is moved to  bit 4 of byte 5, overwriting it
        Common::SetBit(data[5], 4, Common::ExtractBit(data[5], 3));
        // Bit 1 of byte 5 is moved to bit 3 of byte 5
        Common::SetBit(data[5], 3, Common::ExtractBit(data[5], 1));
        // Bit 0 of byte 5 is moved to bit 2 of byte 5, overwriting it
        Common::SetBit(data[5], 2, Common::ExtractBit(data[5], 0));

        // Bit 0 and 1 of byte 5 contain a M+ flag and a zero bit which is set below.
        mplus_data.is_mp_data = false;
      }
      else
      {
        // Read failed (extension unplugged), Send M+ data instead
        mplus_data.is_mp_data = true;
      }
      break;
    }
    case PassthroughMode::CLASSIC:
    {
      if (EXT_AMT == i2c_bus.BusRead(EXT_SLAVE, EXT_ADDR, EXT_AMT, data))
      {
        // Passthrough data modifications via wiibrew.org
        // Data passing through drops the least significant bit of the axes of the left (or only)
        // joystick Bit 0 of Byte 4 is overwritten [by the 'extension_connected' flag] Bits 0 and 1
        // of Byte 5 are moved to bit 0 of Bytes 0 and 1, overwriting what was there before
        Common::SetBit(data[0], 0, Common::ExtractBit(data[5], 0));
        Common::SetBit(data[1], 0, Common::ExtractBit(data[5], 1));

        // Bit 0 and 1 of byte 5 contain a M+ flag and a zero bit which is set below.
        mplus_data.is_mp_data = false;
      }
      else
      {
        // Read failed (extension unplugged), Send M+ data instead
        mplus_data.is_mp_data = true;
      }
      break;
    }
    default:
      PanicAlert("MotionPlus unknown passthrough-mode %d", (int)GetPassthroughMode());
      break;
    }
  }

  // If the above logic determined this should be M+ data, update it here
  if (mplus_data.is_mp_data)
  {
    // Wiibrew: "While the Wiimote is still, the values will be about 0x1F7F (8,063)"
    // high-velocity range should be about +/- 1500 or 1600 dps
    // low-velocity range should be about +/- 400 dps
    // Wiibrew implies it shoould be +/- 595 and 2700

    u16 yaw_value = 0x2000;
    u16 roll_value = 0x2000;
    u16 pitch_value = 0x2000;

    mplus_data.yaw_slow = 1;
    mplus_data.roll_slow = 1;
    mplus_data.pitch_slow = 1;

    // Bits 0-7
    mplus_data.yaw1 = yaw_value & 0xff;
    mplus_data.roll1 = roll_value & 0xff;
    mplus_data.pitch1 = pitch_value & 0xff;

    // Bits 8-13
    mplus_data.yaw2 = yaw_value >> 8;
    mplus_data.roll2 = roll_value >> 8;
    mplus_data.pitch2 = pitch_value >> 8;
  }

  mplus_data.extension_connected = m_extension_port.IsDeviceConnected();
  mplus_data.zero = 0;
}

}  // namespace WiimoteEmu
