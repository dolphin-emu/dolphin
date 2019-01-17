// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/MotionPlus.h"

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"

namespace WiimoteEmu
{
MotionPlus::MotionPlus() : Extension("MotionPlus")
{
}

void MotionPlus::Reset()
{
  m_reg_data = {};

  m_activation_progress = {};

  // FYI: This ID changes on activation/deactivation
  constexpr std::array<u8, 6> initial_id = {0x00, 0x00, 0xA6, 0x20, 0x00, 0x05};
  m_reg_data.ext_identifier = initial_id;

  // Calibration data.
  // Copied from real hardware as it has yet to be fully reverse engineered.
  // It's possible a checksum is present as the other extensions have one.
  constexpr std::array<u8, 32> cal_data = {
      0x78, 0xd9, 0x78, 0x38, 0x77, 0x9d, 0x2f, 0x0c, 0xcf, 0xf0, 0x31,
      0xad, 0xc8, 0x0b, 0x5e, 0x39, 0x6f, 0x81, 0x7b, 0x89, 0x78, 0x51,
      0x33, 0x60, 0xc9, 0xf5, 0x37, 0xc1, 0x2d, 0xe9, 0x15, 0x8d,
      // 0x79, 0xbc, 0x77, 0xa3, 0x76, 0xd9, 0x30, 0x6c, 0xce, 0x8a, 0x2b,
      // 0x83, 0xc8, 0x02, 0x0e, 0x70, 0x74, 0xb5, 0x79, 0x8e, 0x76, 0x45,
      // 0x38, 0x22, 0xc7, 0xd6, 0x32, 0x3b, 0x2d, 0x35, 0xde, 0x37,
  };
  // constexpr std::array<u8, 32> cal_data = {
  //     0x7d, 0xe2, 0x80, 0x5f, 0x78, 0x56, 0x31, 0x04, 0xce, 0xce, 0x33,
  //     0xf9, 0xc8, 0x04, 0x63, 0x22, 0x77, 0x26, 0x7c, 0xb7, 0x79, 0x62,
  //     0x34, 0x56, 0xc9, 0xa3, 0x3a, 0x35, 0x2d, 0xa8, 0xa9, 0xbc,
  // };
  m_reg_data.calibration_data = cal_data;
}

void MotionPlus::DoState(PointerWrap& p)
{
  p.Do(m_reg_data);
  p.Do(m_activation_progress);
}

MotionPlus::ActivationStatus MotionPlus::GetActivationStatus() const
{
  // M+ takes a bit of time to activate. During which it is completely unresponsive.
  constexpr u8 ACTIVATION_STEPS = ::Wiimote::UPDATE_FREQ * 20 / 1000;

  if ((ACTIVE_DEVICE_ADDR << 1) == m_reg_data.ext_identifier[2])
  {
    if (m_activation_progress < ACTIVATION_STEPS)
      return ActivationStatus::Activating;
    else
      return ActivationStatus::Active;
  }
  else
  {
    if (m_activation_progress != 0)
      return ActivationStatus::Deactivating;
    else
      return ActivationStatus::Inactive;
  }
}

MotionPlus::PassthroughMode MotionPlus::GetPassthroughMode() const
{
  return static_cast<PassthroughMode>(m_reg_data.ext_identifier[4]);
}

ExtensionPort& MotionPlus::GetExtPort()
{
  return m_extension_port;
}

int MotionPlus::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  switch (GetActivationStatus())
  {
  case ActivationStatus::Inactive:
    if (INACTIVE_DEVICE_ADDR != slave_addr)
    {
      // Passthrough to the connected extension. (if any)
      return m_i2c_bus.BusRead(slave_addr, addr, count, data_out);
    }

    // Perform a normal read of the M+ register.
    return RawRead(&m_reg_data, addr, count, data_out);

  case ActivationStatus::Active:
    // FYI: Motion plus does not respond to 0x53 when activated.
    if (ACTIVE_DEVICE_ADDR != slave_addr)
    {
      // No i2c passthrough when activated.
      return 0;
    }

    // Perform a normal read of the M+ register.
    return RawRead(&m_reg_data, addr, count, data_out);

  default:
  case ActivationStatus::Activating:
  case ActivationStatus::Deactivating:
    // The extension port is completely unresponsive here.
    return 0;
  }
}

int MotionPlus::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  switch (GetActivationStatus())
  {
  case ActivationStatus::Inactive:
  {
    if (INACTIVE_DEVICE_ADDR != slave_addr)
    {
      // Passthrough to the connected extension. (if any)
      return m_i2c_bus.BusWrite(slave_addr, addr, count, data_in);
    }

    auto const result = RawWrite(&m_reg_data, addr, count, data_in);

    if (PASSTHROUGH_MODE_OFFSET == addr)
    {
      OnPassthroughModeWrite();
    }

    return result;
  }

  case ActivationStatus::Active:
  {
    // FYI: Motion plus does not respond to 0x53 when activated.
    if (ACTIVE_DEVICE_ADDR != slave_addr)
    {
      // No i2c passthrough when activated.
      return 0;
    }

    auto const result = RawWrite(&m_reg_data, addr, count, data_in);

    if (offsetof(Register, initialized) == addr)
    {
      // It seems a write of any value here triggers deactivation on a real M+.
      Deactivate();

      // Passthrough the write to the attached extension.
      // The M+ deactivation signal is cleverly the same as EXT activation.
      m_i2c_bus.BusWrite(slave_addr, addr, count, data_in);
    }
    else if (offsetof(Register, init_stage) == addr)
    {
      if (m_reg_data.init_stage == 0x01)
      {
        m_reg_data.init_progress = 0x18;
      }
      else
      {
        // Games are sometimes unhappy with the 64 bytes of data that we have provided.
        // We have no choice here but to deactivate and try again.
        WARN_LOG(WIIMOTE, "M+ reset due to bad initialization sequence.");

        Deactivate();
      }
    }
    else if (offsetof(Register, calibration_trigger) == addr)
    {
      // Games seem to invoke this twice to start and stop. Exact consequences unknown.
      DEBUG_LOG(WIIMOTE, "M+ calibration trigger: 0x%x", m_reg_data.calibration_trigger);
    }
    else if (PASSTHROUGH_MODE_OFFSET == addr)
    {
      // Games sometimes (not often) write zero here to deactivate the M+.
      OnPassthroughModeWrite();
    }

    return result;
  }

  default:
  case ActivationStatus::Activating:
  case ActivationStatus::Deactivating:
    // The extension port is completely unresponsive here.
    return 0;
  }
}

void MotionPlus::OnPassthroughModeWrite()
{
  const auto status = GetActivationStatus();

  switch (GetPassthroughMode())
  {
  case PassthroughMode::Disabled:
  case PassthroughMode::Nunchuk:
  case PassthroughMode::Classic:
    if (ActivationStatus::Active != status)
      Activate();
    break;

  default:
    if (ActivationStatus::Inactive != status)
      Deactivate();
    break;
  }
}

void MotionPlus::Activate()
{
  DEBUG_LOG(WIIMOTE, "M+ has been activated.");

  m_reg_data.ext_identifier[2] = ACTIVE_DEVICE_ADDR << 1;
  m_reg_data.init_progress = 0x2;

  // We must do this to reset our extension_connected flag:
  m_reg_data.controller_data = {};
}

void MotionPlus::Deactivate()
{
  DEBUG_LOG(WIIMOTE, "M+ has been deactivated.");

  m_reg_data.ext_identifier[2] = INACTIVE_DEVICE_ADDR << 1;
  m_reg_data.init_progress = 0x0;
}

bool MotionPlus::ReadDeviceDetectPin() const
{
  switch (GetActivationStatus())
  {
  case ActivationStatus::Inactive:
    // When inactive the device detect pin reads from the ext port:
    return m_extension_port.IsDeviceConnected();

  case ActivationStatus::Active:
    return true;

  default:
  case ActivationStatus::Activating:
  case ActivationStatus::Deactivating:
    return false;
  }
}

bool MotionPlus::IsButtonPressed() const
{
  return false;
}

void MotionPlus::Update()
{
  switch (GetActivationStatus())
  {
  case ActivationStatus::Activating:
    ++m_activation_progress;
    break;

  case ActivationStatus::Deactivating:
    --m_activation_progress;
    break;

  case ActivationStatus::Active:
  {
    u8* const data = m_reg_data.controller_data.data();
    DataFormat mplus_data = Common::BitCastPtr<DataFormat>(data);

    const bool is_ext_connected = m_extension_port.IsDeviceConnected();

    // Check for extension change:
    if (is_ext_connected != mplus_data.extension_connected)
    {
      if (is_ext_connected)
      {
        DEBUG_LOG(WIIMOTE, "M+ initializing new extension.");

        // The M+ automatically initializes an extension when attached.

        // What we do here does not exactly match a real M+,
        // but it's close enough for our emulated extensions which are not very picky.

        // Disable encryption
        {
          constexpr u8 INIT_OFFSET = offsetof(Register, initialized);
          std::array<u8, 1> enc_data = {0x55};
          m_i2c_bus.BusWrite(ACTIVE_DEVICE_ADDR, INIT_OFFSET, (int)enc_data.size(),
                             enc_data.data());
        }

        // Read identifier
        {
          constexpr u8 ID_OFFSET = offsetof(Register, ext_identifier);
          std::array<u8, 6> id_data = {};
          m_i2c_bus.BusRead(ACTIVE_DEVICE_ADDR, ID_OFFSET, (int)id_data.size(), id_data.data());
          m_reg_data.passthrough_ext_id_0 = id_data[0];
          m_reg_data.passthrough_ext_id_4 = id_data[4];
          m_reg_data.passthrough_ext_id_5 = id_data[5];
        }

        // Read calibration data
        {
          constexpr u8 CAL_OFFSET = offsetof(Register, calibration_data);
          m_i2c_bus.BusRead(ACTIVE_DEVICE_ADDR, CAL_OFFSET,
                            (int)m_reg_data.passthrough_ext_calib.size(),
                            m_reg_data.passthrough_ext_calib.data());
        }
      }

      // Update flag in register:
      mplus_data.extension_connected = is_ext_connected;
      Common::BitCastPtr<DataFormat>(data) = mplus_data;
    }

    break;
  }

  default:
    break;
  }
}

// This is something that is triggered by a read of 0x00 on real hardware.
// But we do it here for determinism reasons.
void MotionPlus::PrepareInput(const Common::Vec3& angular_velocity)
{
  if (GetActivationStatus() != ActivationStatus::Active)
    return;

  u8* const data = m_reg_data.controller_data.data();

  // Try to alternate between M+ and EXT data:
  // This flag is checked down below where the controller data is prepared.
  DataFormat mplus_data = Common::BitCastPtr<DataFormat>(data);
  mplus_data.is_mp_data ^= true;

  // Maintain the current state of this bit rather than reading from the port.
  // We update this bit elsewhere and performs some tasks on change.
  const bool is_ext_connected = mplus_data.extension_connected;

  if (0x2 == m_reg_data.init_progress)
  {
    // Activation sets init_progress to 0x2.
    // Harness this to send some special first-time data.

    // The first data report of the M+ contains some unknown data.
    // Without sending this, inputs are unresponsive.. even regular buttons.
    // The data varies but it is typically something like the following:
    const std::array<u8, 6> init_data = {0x81, 0x46, 0x46, 0xb6, is_ext_connected, 0x02};
    // const std::array<u8, 6> init_data = {0xdd, 0x46, 0x47, 0xb6, is_ext_connected, 0x02};
    // const std::array<u8, 6> init_data = {0xc3, 0xb0, 0x4f, 0x52, u8(0xfc | is_ext_connected),
    // 0x02};
    // const std::array<u8, 6> init_data = {0xf0, 0x46, 0x47, 0xb6, is_ext_connected, 0x02};

    std::copy(std::begin(init_data), std::end(init_data), data);

    m_reg_data.init_progress = 0x4;

    return;
  }
  else if (0x4 == m_reg_data.init_progress)
  {
    // Force another report of M+ data.
    // The second data report is regular M+ data, even if a passthrough mode is set.
    mplus_data.is_mp_data = true;

    // This is some sort of calibration data and checksum.
    // Copied from real hardware as it has yet to be fully reverse engineered.
    constexpr std::array<u8, 64> init_data = {
        0x99, 0x1a, 0x07, 0x1b, 0x97, 0xf1, 0x11, 0x78, 0x0c, 0x42, 0x2b, 0x68, 0xdf,
        0x44, 0x38, 0x0d, 0x2b, 0x7e, 0xd6, 0x84, 0x84, 0x58, 0x65, 0xc9, 0xf2, 0x95,
        0xd9, 0xaf, 0xb6, 0xc4, 0x87, 0xd5, 0x18, 0xdb, 0x67, 0x3a, 0xc0, 0x71, 0xec,
        0x3e, 0xf4, 0xe6, 0x7e, 0x35, 0xa3, 0x29, 0xf8, 0x1f, 0xc5, 0x7c, 0x3d, 0xb9,
        0x56, 0x22, 0x95, 0x98, 0x8f, 0xfb, 0x66, 0x3e, 0x9a, 0xdd, 0xeb, 0x7e,
    };
    m_reg_data.init_data = init_data;

    DEBUG_LOG(WIIMOTE, "M+ initialization data step 1 is ready.");

    // Note. A real M+ can take about 2 seconds to reach this state.
    // Games seem to not care that we complete almost instantly.
    m_reg_data.init_progress = 0xe;
  }
  else if (0x18 == m_reg_data.init_progress)
  {
    // This is some sort of calibration data and checksum.
    // Copied from real hardware as it has yet to be fully reverse engineered.
    constexpr std::array<u8, 64> init_data = {
        0xa5, 0x84, 0x1f, 0xd6, 0xbd, 0xdc, 0x7a, 0x4c, 0xf3, 0xc0, 0x24, 0xe0, 0x92,
        0xef, 0x19, 0x28, 0x65, 0xe0, 0x62, 0x7c, 0x9b, 0x41, 0x6f, 0x12, 0xc3, 0xac,
        0x78, 0xe4, 0xfc, 0x6b, 0x7b, 0x0a, 0xb4, 0x50, 0xd6, 0xf2, 0x45, 0xf7, 0x93,
        0x04, 0xaf, 0xf2, 0xb7, 0x26, 0x94, 0xee, 0xad, 0x92, 0x05, 0x6d, 0xe5, 0xc6,
        0xd6, 0x36, 0xdc, 0xa5, 0x69, 0x0f, 0xc8, 0x99, 0xf2, 0x1c, 0x4e, 0x0d,
    };
    m_reg_data.init_data = init_data;

    DEBUG_LOG(WIIMOTE, "M+ initialization data step 2 is ready.");

    // Note. A real M+ can take about 2 seconds to reach this state.
    // Games seem to not care that we complete almost instantly.
    m_reg_data.init_progress = 0x1a;
  }

  // After the first two data reports it alternates between EXT and M+ data.
  // Failure to read from the extension results in a fallback to M+ data.

  // Real M+ only ever reads 6 bytes from the extension which is triggered by a read at 0x00.
  // Data after 6 bytes seems to be zero-filled.
  // After reading from the EXT, the real M+ uses that data for the next frame.
  // But we are going to use it for the current frame, because we can.
  constexpr int EXT_AMT = 6;
  // Always read from 0x52 @ 0x00:
  constexpr u8 EXT_SLAVE = ExtensionPort::REPORT_I2C_SLAVE;
  constexpr u8 EXT_ADDR = ExtensionPort::REPORT_I2C_ADDR;

  // If the last frame had M+ data try to send some non-M+ data:
  if (!mplus_data.is_mp_data)
  {
    switch (GetPassthroughMode())
    {
    case PassthroughMode::Disabled:
    {
      // Passthrough disabled, always send M+ data:
      mplus_data.is_mp_data = true;
      break;
    }
    case PassthroughMode::Nunchuk:
    {
      if (EXT_AMT == m_i2c_bus.BusRead(EXT_SLAVE, EXT_ADDR, EXT_AMT, data))
      {
        // Passthrough data modifications via wiibrew.org
        // Verified on real hardware via a test of every bit.
        // Data passing through drops the least significant bit of the three accelerometer values.
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

        mplus_data = Common::BitCastPtr<DataFormat>(data);

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
    case PassthroughMode::Classic:
    {
      if (EXT_AMT == m_i2c_bus.BusRead(EXT_SLAVE, EXT_ADDR, EXT_AMT, data))
      {
        // Passthrough data modifications via wiibrew.org
        // Verified on real hardware via a test of every bit.
        // Data passing through drops the least significant bit of the axes of the left (or only)
        // joystick Bit 0 of Byte 4 is overwritten [by the 'extension_connected' flag] Bits 0 and 1
        // of Byte 5 are moved to bit 0 of Bytes 0 and 1, overwriting what was there before.
        Common::SetBit(data[0], 0, Common::ExtractBit(data[5], 0));
        Common::SetBit(data[1], 0, Common::ExtractBit(data[5], 1));

        mplus_data = Common::BitCastPtr<DataFormat>(data);

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
      // This really shouldn't happen as the M+ deactivates on an invalid mode write.
      WARN_LOG(WIIMOTE, "M+ unknown passthrough-mode %d", (int)GetPassthroughMode());
      mplus_data.is_mp_data = true;
      break;
    }
  }

  // If the above logic determined this should be M+ data, update it here
  if (mplus_data.is_mp_data)
  {
    // These are the max referene velocities used by the sensor of the M+.
    // TODO: Reverse engineer the calibration data to send perfect values.
    constexpr float SLOW_MAX_RAD_PER_SEC = 440 * float(MathUtil::TAU) / 360;
    constexpr float FAST_MAX_RAD_PER_SEC = 2000 * float(MathUtil::TAU) / 360;

    constexpr int BITS_OF_PRECISION = 14;
    constexpr s32 MAX_VALUE = (1 << BITS_OF_PRECISION) - 1;

    // constexpr u16 NEUTRAL_YAW = 0x1f66;
    // constexpr u16 NEUTRAL_ROLL = 0x2058;
    // constexpr u16 NEUTRAL_PITCH = 0x1fa8;

    constexpr u16 NEUTRAL_YAW = 0x1f2e;
    constexpr u16 NEUTRAL_ROLL = 0x1f72;
    constexpr u16 NEUTRAL_PITCH = 0x1f9d;

    // constexpr u16 SENSOR_NEUTRAL = (1 << (BITS_OF_PRECISION - 1));
    // constexpr u16 SENSOR_NEUTRAL = 0x783a >> 2;
    constexpr u16 SENSOR_RANGE = (1 << (BITS_OF_PRECISION - 1));

    constexpr float SLOW_SCALE = SENSOR_RANGE / SLOW_MAX_RAD_PER_SEC;
    constexpr float FAST_SCALE = SENSOR_RANGE / FAST_MAX_RAD_PER_SEC;

    const float yaw = angular_velocity.z;
    // TODO: verify roll signedness with our calibration data.
    const float roll = angular_velocity.y;
    const float pitch = angular_velocity.x;

    // Slow scaling can be used if it fits in the sensor range.
    mplus_data.yaw_slow = (std::abs(yaw) < SLOW_MAX_RAD_PER_SEC);
    s32 yaw_value = yaw * (mplus_data.yaw_slow ? SLOW_SCALE : FAST_SCALE);

    mplus_data.roll_slow = (std::abs(roll) < SLOW_MAX_RAD_PER_SEC);
    s32 roll_value = roll * (mplus_data.roll_slow ? SLOW_SCALE : FAST_SCALE);

    mplus_data.pitch_slow = (std::abs(pitch) < SLOW_MAX_RAD_PER_SEC);
    s32 pitch_value = pitch * (mplus_data.pitch_slow ? SLOW_SCALE : FAST_SCALE);

    yaw_value = MathUtil::Clamp(yaw_value + NEUTRAL_YAW, 0, MAX_VALUE);
    roll_value = MathUtil::Clamp(roll_value + NEUTRAL_ROLL, 0, MAX_VALUE);
    pitch_value = MathUtil::Clamp(pitch_value + NEUTRAL_PITCH, 0, MAX_VALUE);

    // INFO_LOG(WIIMOTE, "M+ YAW: 0x%x slow:%d", yaw_value, mplus_data.yaw_slow);
    // INFO_LOG(WIIMOTE, "M+ ROL: 0x%x slow:%d", roll_value, mplus_data.roll_slow);
    // INFO_LOG(WIIMOTE, "M+ PIT: 0x%x slow:%d", pitch_value, mplus_data.pitch_slow);

    // Bits 0-7
    mplus_data.yaw1 = yaw_value & 0xff;
    mplus_data.roll1 = roll_value & 0xff;
    mplus_data.pitch1 = pitch_value & 0xff;

    // Bits 8-13
    mplus_data.yaw2 = yaw_value >> 8;
    mplus_data.roll2 = roll_value >> 8;
    mplus_data.pitch2 = pitch_value >> 8;
  }

  mplus_data.extension_connected = is_ext_connected;
  mplus_data.zero = 0;

  Common::BitCastPtr<DataFormat>(data) = mplus_data;
}

}  // namespace WiimoteEmu
