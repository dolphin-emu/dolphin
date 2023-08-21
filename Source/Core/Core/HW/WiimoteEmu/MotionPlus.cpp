// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/MotionPlus.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include <mbedtls/bignum.h>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"

namespace
{
// Minimal wrapper mainly to handle init/free
struct MPI : mbedtls_mpi
{
  explicit MPI(const char* base_10_str) : MPI() { mbedtls_mpi_read_string(this, 10, base_10_str); }

  MPI() { mbedtls_mpi_init(this); }
  ~MPI() { mbedtls_mpi_free(this); }

  mbedtls_mpi* Data() { return this; };

  template <std::size_t N>
  bool ReadBinary(const u8 (&in_data)[N])
  {
    return 0 == mbedtls_mpi_read_binary(this, std::begin(in_data), std::size(in_data));
  }

  template <std::size_t N>
  bool WriteLittleEndianBinary(std::array<u8, N>* out_data)
  {
    if (mbedtls_mpi_write_binary(this, out_data->data(), out_data->size()))
      return false;

    std::reverse(out_data->begin(), out_data->end());
    return true;
  }

  MPI(const MPI&) = delete;
  MPI& operator=(const MPI&) = delete;
};
}  // namespace

namespace WiimoteEmu
{
Common::Vec3 MotionPlus::DataFormat::Data::GetAngularVelocity(const CalibrationBlocks& blocks) const
{
  // Each axis may be using either slow or fast calibration.
  const auto calibration = blocks.GetRelevantCalibration(is_slow);

  // It seems M+ calibration data does not follow the "right-hand rule".
  const auto sign_fix = Common::Vec3(-1, +1, -1);

  // Adjust deg/s to rad/s.
  constexpr auto scalar = float(MathUtil::TAU / 360);

  return gyro.GetNormalizedValue(calibration.value) * sign_fix * Common::Vec3(calibration.degrees) *
         scalar;
}

auto MotionPlus::CalibrationBlocks::GetRelevantCalibration(SlowType is_slow) const
    -> RelevantCalibration
{
  RelevantCalibration result;

  const auto& pitch_block = is_slow.x ? slow : fast;
  const auto& roll_block = is_slow.y ? slow : fast;
  const auto& yaw_block = is_slow.z ? slow : fast;

  result.value.max = {pitch_block.pitch_scale, roll_block.roll_scale, yaw_block.yaw_scale};

  result.value.zero = {pitch_block.pitch_zero, roll_block.roll_zero, yaw_block.yaw_zero};

  result.degrees.x = pitch_block.degrees_div_6 * 6;
  result.degrees.y = roll_block.degrees_div_6 * 6;
  result.degrees.z = yaw_block.degrees_div_6 * 6;

  return result;
}

MotionPlus::MotionPlus() : Extension("MotionPlus")
{
}

void MotionPlus::Reset()
{
  m_reg_data = {};

  m_progress_timer = {};

  // Seeing as we allow disconnection of the M+, we'll say we're not integrated.
  // (0x00 or 0x01)
  constexpr u8 IS_INTEGRATED = 0x00;

  // FYI: This ID changes on activation/deactivation
  constexpr std::array<u8, 6> initial_id = {IS_INTEGRATED, 0x00, 0xA6, 0x20, 0x00, 0x05};
  m_reg_data.ext_identifier = initial_id;

  // Build calibration data.

  // Matching signedness of my real Wiimote+.
  // This also results in all values following the "right-hand rule".
  constexpr u16 YAW_SCALE = CALIBRATION_ZERO - CALIBRATION_SCALE_OFFSET;
  constexpr u16 ROLL_SCALE = CALIBRATION_ZERO + CALIBRATION_SCALE_OFFSET;
  constexpr u16 PITCH_SCALE = CALIBRATION_ZERO - CALIBRATION_SCALE_OFFSET;

  static_assert(sizeof(CalibrationData) == 0x20, "Bad size.");

  static_assert(CALIBRATION_FAST_SCALE_DEGREES % 6 == 0, "Value should be divisible by 6.");
  static_assert(CALIBRATION_SLOW_SCALE_DEGREES % 6 == 0, "Value should be divisible by 6.");

  CalibrationData calibration;
  calibration.fast.yaw_zero = calibration.slow.yaw_zero = CALIBRATION_ZERO;
  calibration.fast.roll_zero = calibration.slow.roll_zero = CALIBRATION_ZERO;
  calibration.fast.pitch_zero = calibration.slow.pitch_zero = CALIBRATION_ZERO;

  calibration.fast.yaw_scale = calibration.slow.yaw_scale = YAW_SCALE;
  calibration.fast.roll_scale = calibration.slow.roll_scale = ROLL_SCALE;
  calibration.fast.pitch_scale = calibration.slow.pitch_scale = PITCH_SCALE;

  calibration.fast.degrees_div_6 = CALIBRATION_FAST_SCALE_DEGREES / 6;
  calibration.slow.degrees_div_6 = CALIBRATION_SLOW_SCALE_DEGREES / 6;

  // From what I can tell, this value is only used to compare against a previously made copy.
  // If the value matches that of the last connected wiimote which passed the "challenge",
  // then it seems the "challenge" is not performed a second time.
  calibration.uid_1 = 0x0b;
  calibration.uid_2 = 0xe9;

  calibration.UpdateChecksum();

  Common::BitCastPtr<CalibrationData>(m_reg_data.calibration_data.data()) = calibration;
}

void MotionPlus::CalibrationData::UpdateChecksum()
{
  // Checksum is crc32 of all data other than the checksum itself.
  u32 crc_result = Common::StartCRC32();
  crc_result = Common::UpdateCRC32(crc_result, reinterpret_cast<const u8*>(this), 0xe);
  crc_result = Common::UpdateCRC32(crc_result, reinterpret_cast<const u8*>(this) + 0x10, 0xe);

  crc32_lsb = u16(crc_result);
  crc32_msb = u16(crc_result >> 16);
}

void MotionPlus::DoState(PointerWrap& p)
{
  p.Do(m_reg_data);
  p.Do(m_progress_timer);
}

MotionPlus::ActivationStatus MotionPlus::GetActivationStatus() const
{
  if ((ACTIVE_DEVICE_ADDR << 1) == m_reg_data.ext_identifier[2])
  {
    if (ChallengeState::Activating == m_reg_data.challenge_state)
      return ActivationStatus::Activating;
    else
      return ActivationStatus::Active;
  }
  else
  {
    if (m_progress_timer != 0)
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

    DEBUG_LOG_FMT(WIIMOTE, "Inactive M+ write {:#x} : {}", addr, ArrayToString(data_in, count));

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

    DEBUG_LOG_FMT(WIIMOTE, "Active M+ write {:#x} : {}", addr, ArrayToString(data_in, count));

    auto const result = RawWrite(&m_reg_data, addr, count, data_in);

    switch (addr)
    {
    case offsetof(Register, init_trigger):
      // It seems a write of any value here triggers deactivation on a real M+.
      Deactivate();

      // Passthrough the write to the attached extension.
      // The M+ deactivation signal is cleverly the same as EXT initialization.
      m_i2c_bus.BusWrite(slave_addr, addr, count, data_in);
      break;

    case offsetof(Register, challenge_type):
      if (ChallengeState::ParameterXReady == m_reg_data.challenge_state)
      {
        DEBUG_LOG_FMT(WIIMOTE, "M+ challenge: {:#x}", m_reg_data.challenge_type);

        // After games read parameter x they write here to request y0 or y1.
        if (0 == m_reg_data.challenge_type)
        {
          // Preparing y0 on the real M+ is almost instant (30ms maybe).
          constexpr int PREPARE_Y0_MS = 30;
          m_progress_timer = ::Wiimote::UPDATE_FREQ * PREPARE_Y0_MS / 1000;
        }
        else
        {
          // A real M+ takes about 1200ms to prepare y1.
          // Games seem to not care that we don't take that long.
          constexpr int PREPARE_Y1_MS = 500;
          m_progress_timer = ::Wiimote::UPDATE_FREQ * PREPARE_Y1_MS / 1000;
        }

        // Games give the M+ a bit of time to compute the value.
        // y0 gets about half a second.
        // y1 gets at about 9.5 seconds.
        // After this the M+ will fail the "challenge".

        m_reg_data.challenge_state = ChallengeState::PreparingY;
      }
      break;

    case offsetof(Register, calibration_trigger):
      // Games seem to invoke this to start and stop calibration. Exact consequences unknown.
      DEBUG_LOG_FMT(WIIMOTE, "M+ calibration trigger: {:#x}", m_reg_data.calibration_trigger);
      break;

    case PASSTHROUGH_MODE_OFFSET:
      // Games sometimes (not often) write zero here to deactivate the M+.
      OnPassthroughModeWrite();
      break;
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
  DEBUG_LOG_FMT(WIIMOTE, "M+ has been activated.");

  m_reg_data.ext_identifier[2] = ACTIVE_DEVICE_ADDR << 1;

  // We must do this to reset our extension_connected and is_mp_data flags:
  m_reg_data.controller_data = {};

  m_reg_data.challenge_state = ChallengeState::Activating;

  // M+ takes a bit of time to activate. During which it is completely unresponsive.
  // This also affects the device detect pin which results in wiimote status reports.
  constexpr int ACTIVATION_MS = 20;
  m_progress_timer = ::Wiimote::UPDATE_FREQ * ACTIVATION_MS / 1000;
}

void MotionPlus::Deactivate()
{
  DEBUG_LOG_FMT(WIIMOTE, "M+ has been deactivated.");

  m_reg_data.ext_identifier[2] = INACTIVE_DEVICE_ADDR << 1;

  // M+ takes a bit of time to deactivate. During which it is completely unresponsive.
  // This also affects the device detect pin which results in wiimote status reports.
  constexpr int DEACTIVATION_MS = 20;
  m_progress_timer = ::Wiimote::UPDATE_FREQ * DEACTIVATION_MS / 1000;
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

void MotionPlus::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  // MotionPlus is handled separately, nothing to do here.
}

void MotionPlus::Update(const DesiredExtensionState& target_state)
{
  if (m_progress_timer)
    --m_progress_timer;

  if (!m_progress_timer && ActivationStatus::Activating == GetActivationStatus())
  {
    // M+ is active now that the timer is up.
    m_reg_data.challenge_state = ChallengeState::PreparingX;

    // Games give the M+ about a minute to prepare x before failure.
    // A real M+ can take about 1500ms.
    // The SDK seems to have a race condition that fails if a non-ready value is not read.
    // A necessary delay preventing challenge failure is not inserted if x is immediately ready.
    // So we must use at least a small delay.
    // Note: This does not delay game start. The challenge takes place in the background.
    constexpr int PREPARE_X_MS = 500;
    m_progress_timer = ::Wiimote::UPDATE_FREQ * PREPARE_X_MS / 1000;
  }

  if (ActivationStatus::Active != GetActivationStatus())
    return;

  u8* const data = m_reg_data.controller_data.data();
  DataFormat mplus_data = Common::BitCastPtr<DataFormat>(data);

  const bool is_ext_connected = m_extension_port.IsDeviceConnected();

  // Check for extension change:
  if (is_ext_connected != mplus_data.extension_connected)
  {
    if (is_ext_connected)
    {
      DEBUG_LOG_FMT(WIIMOTE, "M+ initializing new extension.");

      // The M+ automatically initializes an extension when attached.

      // What we do here does not exactly match a real M+,
      // but it's close enough for our emulated extensions which are not very picky.

      // Disable encryption
      {
        constexpr u8 INIT_OFFSET = offsetof(Register, init_trigger);
        std::array<u8, 1> enc_data = {0x55};
        m_i2c_bus.BusWrite(ACTIVE_DEVICE_ADDR, INIT_OFFSET, int(enc_data.size()), enc_data.data());
      }

      // Read identifier
      {
        constexpr u8 ID_OFFSET = offsetof(Register, ext_identifier);
        std::array<u8, 6> id_data = {};
        m_i2c_bus.BusRead(ACTIVE_DEVICE_ADDR, ID_OFFSET, int(id_data.size()), id_data.data());
        m_reg_data.passthrough_ext_id_0 = id_data[0];
        m_reg_data.passthrough_ext_id_4 = id_data[4];
        m_reg_data.passthrough_ext_id_5 = id_data[5];
      }

      // Read calibration data
      {
        constexpr u8 CAL_OFFSET = offsetof(Register, calibration_data);
        m_i2c_bus.BusRead(ACTIVE_DEVICE_ADDR, CAL_OFFSET,
                          int(m_reg_data.passthrough_ext_calib.size()),
                          m_reg_data.passthrough_ext_calib.data());
      }
    }

    // Update flag in register:
    mplus_data.extension_connected = is_ext_connected;
    Common::BitCastPtr<DataFormat>(data) = mplus_data;
  }

  // Only perform any of the following challenge logic if our timer is up.
  if (m_progress_timer)
    return;

  // This is potentially any value that is less than cert_n and >= 2.
  // A real M+ uses random values each run.
  constexpr u8 magic[] = "DOLPHIN DOES WHAT NINTENDON'T.";

  constexpr char cert_n[] =
      "67614561104116375676885818084175632651294951727285593632649596941616763967271774525888270484"
      "88546653264235848263182009106217734439508352645687684489830161";

  constexpr char sqrt_v[] =
      "22331959796794118515742337844101477131884013381589363004659408068948154670914705521646304758"
      "02483462872732436570235909421331424649287229820640697259759264";

  switch (m_reg_data.challenge_state)
  {
  case ChallengeState::PreparingX:
  {
    MPI param_x;
    param_x.ReadBinary(magic);

    mbedtls_mpi_mul_mpi(&param_x, &param_x, &param_x);
    mbedtls_mpi_mod_mpi(&param_x, &param_x, MPI(cert_n).Data());

    // Big-int little endian parameter x.
    param_x.WriteLittleEndianBinary(&m_reg_data.challenge_data);

    DEBUG_LOG_FMT(WIIMOTE, "M+ parameter x ready.");
    m_reg_data.challenge_state = ChallengeState::ParameterXReady;
    break;
  }

  case ChallengeState::PreparingY:
    if (0 == m_reg_data.challenge_type)
    {
      MPI param_y0;
      param_y0.ReadBinary(magic);

      // Big-int little endian parameter y0.
      param_y0.WriteLittleEndianBinary(&m_reg_data.challenge_data);
    }
    else
    {
      MPI param_y1;
      param_y1.ReadBinary(magic);

      mbedtls_mpi_mul_mpi(&param_y1, &param_y1, MPI(sqrt_v).Data());
      mbedtls_mpi_mod_mpi(&param_y1, &param_y1, MPI(cert_n).Data());

      // Big-int little endian parameter y1.
      param_y1.WriteLittleEndianBinary(&m_reg_data.challenge_data);
    }

    DEBUG_LOG_FMT(WIIMOTE, "M+ parameter y ready.");
    m_reg_data.challenge_state = ChallengeState::ParameterYReady;
    break;

  default:
    break;
  }
}

MotionPlus::DataFormat::Data MotionPlus::GetGyroscopeData(const Common::Vec3& angular_velocity)
{
  // Slow (high precision) scaling can be used if it fits in the sensor range.
  const float yaw = angular_velocity.z;
  const bool yaw_slow = (std::abs(yaw) < SLOW_MAX_RAD_PER_SEC);
  const s32 yaw_value = yaw * (yaw_slow ? SLOW_SCALE : FAST_SCALE);

  const float roll = angular_velocity.y;
  const bool roll_slow = (std::abs(roll) < SLOW_MAX_RAD_PER_SEC);
  const s32 roll_value = roll * (roll_slow ? SLOW_SCALE : FAST_SCALE);

  const float pitch = angular_velocity.x;
  const bool pitch_slow = (std::abs(pitch) < SLOW_MAX_RAD_PER_SEC);
  const s32 pitch_value = pitch * (pitch_slow ? SLOW_SCALE : FAST_SCALE);

  const u16 clamped_yaw_value = u16(std::llround(std::clamp(yaw_value + ZERO_VALUE, 0, MAX_VALUE)));
  const u16 clamped_roll_value =
      u16(std::llround(std::clamp(roll_value + ZERO_VALUE, 0, MAX_VALUE)));
  const u16 clamped_pitch_value =
      u16(std::llround(std::clamp(pitch_value + ZERO_VALUE, 0, MAX_VALUE)));

  return MotionPlus::DataFormat::Data{
      MotionPlus::DataFormat::GyroRawValue{MotionPlus::DataFormat::GyroType(
          clamped_pitch_value, clamped_roll_value, clamped_yaw_value)},
      MotionPlus::DataFormat::SlowType(pitch_slow, roll_slow, yaw_slow)};
}

MotionPlus::DataFormat::Data MotionPlus::GetDefaultGyroscopeData()
{
  return MotionPlus::DataFormat::Data{
      MotionPlus::DataFormat::GyroRawValue{
          MotionPlus::DataFormat::GyroType(u16(ZERO_VALUE), u16(ZERO_VALUE), u16(ZERO_VALUE))},
      MotionPlus::DataFormat::SlowType(true, true, true)};
}

// This is something that is triggered by a read of 0x00 on real hardware.
// But we do it here for determinism reasons.
void MotionPlus::PrepareInput(const MotionPlus::DataFormat::Data& gyroscope_data)
{
  if (GetActivationStatus() != ActivationStatus::Active)
    return;

  u8* const data = m_reg_data.controller_data.data();

  // FYI: A real M+ seems to always send some garbage/mystery data for the first report,
  // followed by a normal M+ data report, and then finally passhrough data (if enabled).
  // Things seem to work without doing that so we'll just send normal M+ data right away.
  DataFormat mplus_data = Common::BitCastPtr<DataFormat>(data);

  // Maintain the current state of this bit rather than reading from the port.
  // We update this bit elsewhere and performs some tasks on change.
  const bool is_ext_connected = mplus_data.extension_connected;

  // After the first "garbage" report a real M+ alternates between M+ and EXT data.
  // Failure to read from the extension results in a fallback to M+ data.
  mplus_data.is_mp_data ^= true;

  // If the last frame had M+ data try to send some non-M+ data:
  if (!mplus_data.is_mp_data)
  {
    // Real M+ only ever reads 6 bytes from the extension which is triggered by a read at 0x00.
    // Data after 6 bytes seems to be zero-filled.
    // After reading from the EXT, the real M+ uses that data for the next frame.
    // But we are going to use it for the current frame, because we can.
    constexpr int EXT_AMT = 6;
    // Always read from 0x52 @ 0x00:
    constexpr u8 EXT_SLAVE = ExtensionPort::REPORT_I2C_SLAVE;
    constexpr u8 EXT_ADDR = ExtensionPort::REPORT_I2C_ADDR;

    switch (GetPassthroughMode())
    {
    case PassthroughMode::Disabled:
    {
      // Passthrough disabled, always send M+ data:
      mplus_data.is_mp_data = true;
      break;
    }
    case PassthroughMode::Nunchuk:
    case PassthroughMode::Classic:
      if (EXT_AMT == m_i2c_bus.BusRead(EXT_SLAVE, EXT_ADDR, EXT_AMT, data))
      {
        ApplyPassthroughModifications(GetPassthroughMode(), data);
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
    default:
      // This really shouldn't happen as the M+ deactivates on an invalid mode write.
      ERROR_LOG_FMT(WIIMOTE, "M+ unknown passthrough-mode {}",
                    static_cast<int>(GetPassthroughMode()));
      mplus_data.is_mp_data = true;
      break;
    }
  }

  // If the above logic determined this should be M+ data, update it here.
  if (mplus_data.is_mp_data)
  {
    const bool pitch_slow = gyroscope_data.is_slow.x;
    const bool roll_slow = gyroscope_data.is_slow.y;
    const bool yaw_slow = gyroscope_data.is_slow.z;
    const u16 pitch_value = gyroscope_data.gyro.value.x;
    const u16 roll_value = gyroscope_data.gyro.value.y;
    const u16 yaw_value = gyroscope_data.gyro.value.z;

    mplus_data.yaw_slow = u8(yaw_slow);
    mplus_data.roll_slow = u8(roll_slow);
    mplus_data.pitch_slow = u8(pitch_slow);

    // Bits 0-7
    mplus_data.yaw1 = u8(yaw_value);
    mplus_data.roll1 = u8(roll_value);
    mplus_data.pitch1 = u8(pitch_value);

    // Bits 8-13
    mplus_data.yaw2 = u8(yaw_value >> 8);
    mplus_data.roll2 = u8(roll_value >> 8);
    mplus_data.pitch2 = u8(pitch_value >> 8);
  }

  mplus_data.extension_connected = is_ext_connected;
  mplus_data.zero = 0;

  Common::BitCastPtr<DataFormat>(data) = mplus_data;
}

void MotionPlus::ApplyPassthroughModifications(PassthroughMode mode, u8* data)
{
  if (mode == PassthroughMode::Nunchuk)
  {
    // Passthrough data modifications via wiibrew.org
    // Verified on real hardware via a test of every bit.
    // Data passing through drops the least significant bit of the three accelerometer values.
    // Bit 7 of byte 5 is moved to bit 6 of byte 5, overwriting it
    Common::SetBit<6>(data[5], Common::ExtractBit<7>(data[5]));
    // Bit 0 of byte 4 is moved to bit 7 of byte 5
    Common::SetBit<7>(data[5], Common::ExtractBit<0>(data[4]));
    // Bit 3 of byte 5 is moved to  bit 4 of byte 5, overwriting it
    Common::SetBit<4>(data[5], Common::ExtractBit<3>(data[5]));
    // Bit 1 of byte 5 is moved to bit 3 of byte 5
    Common::SetBit<3>(data[5], Common::ExtractBit<1>(data[5]));
    // Bit 0 of byte 5 is moved to bit 2 of byte 5, overwriting it
    Common::SetBit<2>(data[5], Common::ExtractBit<0>(data[5]));
  }
  else if (mode == PassthroughMode::Classic)
  {
    // Passthrough data modifications via wiibrew.org
    // Verified on real hardware via a test of every bit.
    // Data passing through drops the least significant bit of the axes of the left (or only)
    // joystick Bit 0 of Byte 4 is overwritten [by the 'extension_connected' flag] Bits 0 and
    // 1 of Byte 5 are moved to bit 0 of Bytes 0 and 1, overwriting what was there before.
    Common::SetBit<0>(data[0], Common::ExtractBit<0>(data[5]));
    Common::SetBit<0>(data[1], Common::ExtractBit<1>(data[5]));
  }
}

void MotionPlus::ReversePassthroughModifications(PassthroughMode mode, u8* data)
{
  if (mode == PassthroughMode::Nunchuk)
  {
    // Undo M+'s "nunchuk passthrough" modifications.
    Common::SetBit<0>(data[5], Common::ExtractBit<2>(data[5]));
    Common::SetBit<1>(data[5], Common::ExtractBit<3>(data[5]));
    Common::SetBit<3>(data[5], Common::ExtractBit<4>(data[5]));
    Common::SetBit<0>(data[4], Common::ExtractBit<7>(data[5]));
    Common::SetBit<7>(data[5], Common::ExtractBit<6>(data[5]));

    // Set the overwritten bits from the next LSB.
    Common::SetBit<2>(data[5], Common::ExtractBit<3>(data[5]));
    Common::SetBit<4>(data[5], Common::ExtractBit<5>(data[5]));
    Common::SetBit<6>(data[5], Common::ExtractBit<7>(data[5]));
  }
  else if (mode == PassthroughMode::Classic)
  {
    // Undo M+'s "classic controller passthrough" modifications.
    Common::SetBit<0>(data[5], Common::ExtractBit<0>(data[0]));
    Common::SetBit<1>(data[5], Common::ExtractBit<0>(data[1]));

    // Set the overwritten bits from the next LSB.
    Common::SetBit<0>(data[0], Common::ExtractBit<1>(data[0]));
    Common::SetBit<0>(data[1], Common::ExtractBit<1>(data[1]));

    // This is an overwritten unused button bit on the Classic Controller.
    // Note it's a significant bit on the DJ Hero Turntable. (passthrough not feasible)
    Common::SetBit<0>(data[4], 1);
  }
}

}  // namespace WiimoteEmu
