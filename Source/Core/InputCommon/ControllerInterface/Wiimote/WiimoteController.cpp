// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Wiimote/WiimoteController.h"

#include "Common/BitUtils.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/HW/WiimoteEmu/ExtensionPort.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::WiimoteController
{
static constexpr char SOURCE_NAME[] = "Bluetooth";

static constexpr size_t IR_SENSITIVITY_LEVEL_COUNT = 5;

template <typename T>
class Button final : public Core::Device::Input
{
public:
  Button(const T* value, std::common_type_t<T> mask, std::string name)
      : m_value(*value), m_mask(mask), m_name(std::move(name))
  {
  }

  std::string GetName() const override { return m_name; }

  ControlState GetState() const override { return (m_value & m_mask) != 0; }

private:
  const T& m_value;
  const T m_mask;
  const std::string m_name;
};

// GetState returns value divided by supplied "extent".
template <typename T, bool Detectable>
class GenericInput : public Core::Device::Input
{
public:
  GenericInput(const T* value, std::string name, ControlState extent)
      : m_value(*value), m_name(std::move(name)), m_extent(extent)
  {
  }

  bool IsDetectable() const override { return Detectable; }

  std::string GetName() const override { return m_name; }

  ControlState GetState() const final override { return ControlState(m_value) / m_extent; }

protected:
  const T& m_value;
  const std::string m_name;
  const ControlState m_extent;
};

template <typename T>
using AnalogInput = GenericInput<T, true>;

template <typename T>
using UndetectableAnalogInput = GenericInput<T, false>;

// GetName() is appended with '-' or '+' based on sign of "extent" value.
template <bool Detectable>
class SignedInput final : public GenericInput<float, Detectable>
{
public:
  using GenericInput<float, Detectable>::GenericInput;

  std::string GetName() const override { return this->m_name + (this->m_extent < 0 ? '-' : '+'); }
};

using SignedAnalogInput = SignedInput<true>;
using UndetectableSignedAnalogInput = SignedInput<false>;

class Motor final : public Core::Device::Output
{
public:
  Motor(std::atomic<ControlState>* value) : m_value(*value) {}

  std::string GetName() const override { return "Motor"; }

  void SetState(ControlState state) override { m_value = state; }

private:
  std::atomic<ControlState>& m_value;
};

template <typename T>
void Device::QueueReport(T&& report, std::function<void(ErrorCode)> ack_callback)
{
  // Maintain proper rumble state.
  report.rumble = m_rumble;

  m_wiimote->QueueReport(report.REPORT_ID, &report, sizeof(report));

  if (ack_callback)
    AddReportHandler(MakeAckHandler(report.REPORT_ID, std::move(ack_callback)));
}

void AddDevice(std::unique_ptr<WiimoteReal::Wiimote> wiimote)
{
  // Our real wiimote class requires an index.
  // Within the pool it's only going to be used for logging purposes.
  static constexpr int CIFACE_WIIMOTE_INDEX = 55;

  if (!wiimote->Connect(CIFACE_WIIMOTE_INDEX))
  {
    WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to connect.");
    return;
  }

  wiimote->Prepare();
  wiimote->EventLinked();

  g_controller_interface.AddDevice(std::make_shared<Device>(std::move(wiimote)));
}

void ReleaseDevices(std::optional<u32> count)
{
  u32 removed_devices = 0;

  // Remove up to "count" remotes (or all of them if nullopt).
  // Real wiimotes will be added to the pool.
  // Make sure to force the device removal immediately (as they are shared ptrs and
  // they could be kept alive, preventing us from re-creating the device)
  g_controller_interface.RemoveDevice(
      [&](const Core::Device* device) {
        if (device->GetSource() != SOURCE_NAME || count == removed_devices)
          return false;

        ++removed_devices;
        return true;
      },
      true);
}

Device::Device(std::unique_ptr<WiimoteReal::Wiimote> wiimote) : m_wiimote(std::move(wiimote))
{
  using EmuWiimote = WiimoteEmu::Wiimote;

  // Buttons.
  static constexpr std::pair<u16, const char*> button_masks[] = {
      {EmuWiimote::BUTTON_A, "A"},       {EmuWiimote::BUTTON_B, "B"},
      {EmuWiimote::BUTTON_ONE, "1"},     {EmuWiimote::BUTTON_TWO, "2"},
      {EmuWiimote::BUTTON_MINUS, "-"},   {EmuWiimote::BUTTON_PLUS, "+"},
      {EmuWiimote::BUTTON_HOME, "HOME"},
  };

  for (auto& button : button_masks)
    AddInput(new Button<u16>(&m_core_data.hex, button.first, button.second));

  static constexpr u16 dpad_masks[] = {
      EmuWiimote::PAD_UP,
      EmuWiimote::PAD_DOWN,
      EmuWiimote::PAD_LEFT,
      EmuWiimote::PAD_RIGHT,
  };

  // Friendly orientation inputs.
  static constexpr const char* const rotation_names[] = {"Pitch", "Roll", "Yaw"};
  for (std::size_t i = 0; i != std::size(rotation_names); ++i)
  {
    AddInput(
        new UndetectableSignedAnalogInput(&m_rotation_inputs.data[i], rotation_names[i], -1.f));
    AddInput(new UndetectableSignedAnalogInput(&m_rotation_inputs.data[i], rotation_names[i], 1.f));
  }

  // Raw accelerometer.
  for (std::size_t i = 0; i != std::size(dpad_masks); ++i)
    AddInput(new Button<u16>(&m_core_data.hex, dpad_masks[i], named_directions[i]));

  static constexpr std::array<std::array<const char*, 2>, 3> accel_names = {{
      {"Accel Left", "Accel Right"},
      {"Accel Backward", "Accel Forward"},
      {"Accel Up", "Accel Down"},
  }};

  for (std::size_t i = 0; i != m_accel_data.data.size(); ++i)
  {
    AddInput(new UndetectableAnalogInput<float>(&m_accel_data.data[i], accel_names[i][0], 1));
    AddInput(new UndetectableAnalogInput<float>(&m_accel_data.data[i], accel_names[i][1], -1));
  }

  // IR data.
  static constexpr const char* const ir_names[] = {"IR Center X", "IR Center Y"};
  for (std::size_t i = 0; i != std::size(ir_names); ++i)
  {
    AddInput(
        new UndetectableSignedAnalogInput(&m_ir_state.center_position.data[i], ir_names[i], -1.f));
    AddInput(
        new UndetectableSignedAnalogInput(&m_ir_state.center_position.data[i], ir_names[i], 1.f));
  }

  AddInput(new UndetectableAnalogInput<bool>(&m_ir_state.is_hidden, "IR Hidden", 1));

  AddInput(new UndetectableAnalogInput<float>(&m_ir_state.distance, "IR Distance", 1));

  // Raw IR Objects.
  for (std::size_t i = 0; i < 4; ++i)
  {
    AddInput(new UndetectableAnalogInput<float>(&m_ir_state.raw_ir_object_position[i].x,
                                                fmt::format("IR Object {} X", i + 1), 1));
    AddInput(new UndetectableAnalogInput<float>(&m_ir_state.raw_ir_object_position[i].y,
                                                fmt::format("IR Object {} Y", i + 1), 1));
    AddInput(new UndetectableAnalogInput<float>(&m_ir_state.raw_ir_object_size[i],
                                                fmt::format("IR Object {} Size", i + 1), 1));
  }

  // Raw gyroscope.
  static constexpr std::array<std::array<const char*, 2>, 3> gyro_names = {{
      {"Gyro Pitch Down", "Gyro Pitch Up"},
      {"Gyro Roll Left", "Gyro Roll Right"},
      {"Gyro Yaw Left", "Gyro Yaw Right"},
  }};

  for (std::size_t i = 0; i != m_accel_data.data.size(); ++i)
  {
    AddInput(
        new UndetectableAnalogInput<float>(&m_mplus_state.gyro_data.data[i], gyro_names[i][0], 1));
    AddInput(
        new UndetectableAnalogInput<float>(&m_mplus_state.gyro_data.data[i], gyro_names[i][1], -1));
  }

  using WiimoteEmu::Nunchuk;
  const std::string nunchuk_prefix = "Nunchuk ";

  // Buttons.
  AddInput(new Button<u8>(&m_nunchuk_state.buttons, Nunchuk::BUTTON_C, nunchuk_prefix + "C"));
  AddInput(new Button<u8>(&m_nunchuk_state.buttons, Nunchuk::BUTTON_Z, nunchuk_prefix + "Z"));

  // Stick.
  static constexpr const char* const nunchuk_stick_names[] = {"X", "Y"};
  for (std::size_t i = 0; i != std::size(nunchuk_stick_names); ++i)
  {
    AddInput(new SignedAnalogInput(&m_nunchuk_state.stick.data[i],
                                   nunchuk_prefix + nunchuk_stick_names[i], -1.f));
    AddInput(new SignedAnalogInput(&m_nunchuk_state.stick.data[i],
                                   nunchuk_prefix + nunchuk_stick_names[i], 1.f));
  }

  // Raw accelerometer.
  for (std::size_t i = 0; i != m_accel_data.data.size(); ++i)
  {
    AddInput(new UndetectableAnalogInput<float>(&m_nunchuk_state.accel.data[i],
                                                nunchuk_prefix + accel_names[i][0], 1));
    AddInput(new UndetectableAnalogInput<float>(&m_nunchuk_state.accel.data[i],
                                                nunchuk_prefix + accel_names[i][1], -1));
  }

  using WiimoteEmu::Classic;
  const std::string classic_prefix = "Classic ";

  // Buttons.
  static constexpr u16 classic_dpad_masks[] = {
      Classic::PAD_UP,
      Classic::PAD_DOWN,
      Classic::PAD_LEFT,
      Classic::PAD_RIGHT,
  };

  for (std::size_t i = 0; i != std::size(classic_dpad_masks); ++i)
    AddInput(new Button<u16>(&m_classic_state.buttons, classic_dpad_masks[i],
                             classic_prefix + named_directions[i]));

  static constexpr u16 classic_button_masks[] = {
      Classic::BUTTON_A,     Classic::BUTTON_B,    Classic::BUTTON_X,    Classic::BUTTON_Y,
      Classic::TRIGGER_L,    Classic::TRIGGER_R,   Classic::BUTTON_ZL,   Classic::BUTTON_ZR,
      Classic::BUTTON_MINUS, Classic::BUTTON_PLUS, Classic::BUTTON_HOME,
  };

  static constexpr const char* const classic_button_names[] = {
      "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "-", "+", "HOME",
  };

  for (std::size_t i = 0; i != std::size(classic_button_masks); ++i)
    AddInput(new Button<u16>(&m_classic_state.buttons, classic_button_masks[i],
                             classic_prefix + classic_button_names[i]));

  // Sticks.
  static constexpr const char* const classic_stick_names[][2] = {{"Left X", "Left Y"},
                                                                 {"Right X", "Right Y"}};

  for (std::size_t s = 0; s != std::size(m_classic_state.sticks); ++s)
  {
    for (std::size_t i = 0; i != std::size(m_classic_state.sticks[0].data); ++i)
    {
      AddInput(new SignedAnalogInput(&m_classic_state.sticks[s].data[i],
                                     classic_prefix + classic_stick_names[s][i], -1.f));
      AddInput(new SignedAnalogInput(&m_classic_state.sticks[s].data[i],
                                     classic_prefix + classic_stick_names[s][i], 1.f));
    }
  }

  // Triggers.
  AddInput(new AnalogInput<float>(&m_classic_state.triggers[0], classic_prefix + "L-Analog", 1.f));
  AddInput(new AnalogInput<float>(&m_classic_state.triggers[1], classic_prefix + "R-Analog", 1.f));

  // Specialty inputs:
  AddInput(new UndetectableAnalogInput<float>(&m_battery, "Battery", 1.f));
  AddInput(new UndetectableAnalogInput<WiimoteEmu::ExtensionNumber>(
      &m_extension_number_input, "Attached Extension", WiimoteEmu::ExtensionNumber(1)));
  AddInput(new UndetectableAnalogInput<bool>(&m_mplus_attached_input, "Attached MotionPlus", 1));

  AddOutput(new Motor(&m_rumble_level));
}

Device::~Device()
{
  if (!m_wiimote->IsConnected())
    return;

  m_wiimote->EmuStop();

  INFO_LOG_FMT(WIIMOTE, "WiiRemote: Returning remote to pool.");
  WiimoteReal::AddWiimoteToPool(std::move(m_wiimote));
}

std::string Device::GetName() const
{
  return "Wii Remote";
}

std::string Device::GetSource() const
{
  return SOURCE_NAME;
}

// Always add these at the end, given their hotplug nature
int Device::GetSortPriority() const
{
  return -4;
}

void Device::RunTasks()
{
  if (IsPerformingTask())
    return;

  // Request status.
  if (Clock::now() >= m_status_outdated_time)
  {
    QueueReport(OutputReportRequestStatus());

    AddReportHandler(std::function<void(const InputReportStatus& status)>(
        [this](const InputReportStatus& status) {
          DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Received requested status.");
          ProcessStatusReport(status);
        }));

    return;
  }

  // Set LEDs.
  const auto desired_leds = GetDesiredLEDValue();
  if (m_leds != desired_leds)
  {
    OutputReportLeds rpt = {};
    rpt.ack = 1;
    rpt.leds = desired_leds;
    QueueReport(rpt, [this, desired_leds](ErrorCode result) {
      if (result != ErrorCode::Success)
      {
        WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to set LEDs.");
        return;
      }

      DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Set LEDs.");

      m_leds = desired_leds;
    });

    return;
  }

  // Set reporting mode to one that supports every component.
  static constexpr auto desired_reporting_mode = InputReportID::ReportCoreAccelIR10Ext6;
  if (m_reporting_mode != desired_reporting_mode)
  {
    OutputReportMode mode = {};
    mode.ack = 1;
    mode.mode = desired_reporting_mode;
    QueueReport(mode, [this](ErrorCode error) {
      if (error != ErrorCode::Success)
      {
        WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to set reporting mode.");
        return;
      }

      m_reporting_mode = desired_reporting_mode;

      DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Set reporting mode.");
    });

    return;
  }

  // Read accelerometer calibration.
  if (!m_accel_calibration.has_value())
  {
    static constexpr u16 ACCEL_CALIBRATION_ADDR = 0x16;

    ReadData(AddressSpace::EEPROM, 0, ACCEL_CALIBRATION_ADDR, sizeof(AccelCalibrationData),
             [this](ReadResponse response) {
               if (!response)
               {
                 WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to read accelerometer calibration.");
                 return;
               }

               DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Read accelerometer calibration.");

               auto& calibration_data = *response;

               const AccelCalibrationData accel_calibration =
                   Common::BitCastPtr<AccelCalibrationData>(calibration_data.data());
               m_accel_calibration = accel_calibration.GetCalibration();

               WiimoteEmu::UpdateCalibrationDataChecksum(calibration_data, 1);

               // We could potentially try the second block at 0x26 if the checksum is bad.
               if (accel_calibration.checksum != calibration_data.back())
                 WARN_LOG_FMT(WIIMOTE, "WiiRemote: Bad accelerometer calibration checksum.");
             });

    return;
  }

  if (!m_ir_state.IsFullyConfigured())
  {
    ConfigureIRCamera();

    return;
  }

  if (!m_speaker_configured)
  {
    ConfigureSpeaker();

    return;
  }

  // Perform the following tasks only after M+ is settled.
  if (IsWaitingForMotionPlus())
    return;

  // Read the "active" extension ID. (This also gives us the current M+ mode)
  // This will fail on an un-intialized other extension.
  // But extension initialization is the same as M+ de-activation so we must try this first.
  if (m_extension_port == true &&
      (!IsMotionPlusStateKnown() || (!IsMotionPlusActive() && !m_extension_id.has_value())))
  {
    static constexpr u16 ENCRYPTION_ADDR = 0xfb;
    static constexpr u8 ENCRYPTION_VALUE = 0x00;

    // First disable encryption. Note this is a no-op when performed on the M+.
    WriteData(AddressSpace::I2CBus, WiimoteEmu::ExtensionPort::REPORT_I2C_SLAVE, ENCRYPTION_ADDR,
              {ENCRYPTION_VALUE}, [this](ErrorCode error) {
                if (error != ErrorCode::Success)
                  return;

                ReadActiveExtensionID();
              });

    return;
  }

  static constexpr u16 INIT_ADDR = 0xf0;
  static constexpr u8 INIT_VALUE = 0x55;

  // Initialize "active" extension if ID was not recognized.
  // Note this is done before M+ setup to determine the required passthrough mode.
  if (m_extension_id == ExtensionID::Unsupported)
  {
    // Note that this signal also DE-activates a M+.
    WriteData(AddressSpace::I2CBus, WiimoteEmu::ExtensionPort::REPORT_I2C_SLAVE, INIT_ADDR,
              {INIT_VALUE}, [this](ErrorCode result) {
                DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Initialized extension: {}.", int(result));
                m_extension_id = std::nullopt;
              });

    return;
  }

  // The following tasks require a known M+ state.
  if (!IsMotionPlusStateKnown())
    return;

  // We now know the status of the M+.
  // Updating it too frequently results off/on flashes on mode change.
  m_mplus_attached_input = IsMotionPlusActive();

  // Extension removal status is known here. Attachment status is updated after the ID is read.
  if (m_extension_port != true)
    m_extension_number_input = WiimoteEmu::ExtensionNumber::NONE;

  // Periodically try to initialize and activate an inactive M+.
  if (!IsMotionPlusActive() && m_mplus_desired_mode.has_value() &&
      m_mplus_state.current_mode != m_mplus_desired_mode)
  {
    static constexpr u16 MPLUS_POLL_ADDR = WiimoteEmu::MotionPlus::PASSTHROUGH_MODE_OFFSET;
    ReadData(AddressSpace::I2CBus, WiimoteEmu::MotionPlus::INACTIVE_DEVICE_ADDR, MPLUS_POLL_ADDR, 1,
             [this](ReadResponse response) {
               if (!response)
               {
                 DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: M+ poll failed.");
                 HandleMotionPlusNonResponse();
                 return;
               }

               WriteData(AddressSpace::I2CBus, WiimoteEmu::MotionPlus::INACTIVE_DEVICE_ADDR,
                         INIT_ADDR, {INIT_VALUE}, [this](ErrorCode result) {
                           DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: M+ initialization: {}.", int(result));
                           if (result != ErrorCode::Success)
                           {
                             HandleMotionPlusNonResponse();
                             return;
                           }

                           TriggerMotionPlusModeChange();
                         });
             });

    return;
  }

  // Change active M+ passthrough mode.
  if (IsMotionPlusActive() && m_mplus_desired_mode.has_value() &&
      m_mplus_state.current_mode != m_mplus_desired_mode)
  {
    TriggerMotionPlusModeChange();

    return;
  }

  // Read passthrough extension ID.
  // This will also give us a desired M+ passthrough mode.
  if (IsMotionPlusActive() && m_mplus_state.passthrough_port == true && !m_extension_id.has_value())
  {
    // The M+ reads the passthrough ext ID and stores it at 0xf6,f8,f9.
    static constexpr u16 PASSTHROUGH_EXT_ID_ADDR = 0xf6;

    ReadData(AddressSpace::I2CBus, WiimoteEmu::MotionPlus::ACTIVE_DEVICE_ADDR,
             PASSTHROUGH_EXT_ID_ADDR, 4, [this](ReadResponse response) {
               if (!response)
                 return;

               // Port status may have changed since the read was sent.
               // In which case this data read would succeed but be useless.
               if (m_mplus_state.passthrough_port != true)
                 return;

               auto& identifier = *response;

               ProcessExtensionID(identifier[2], identifier[0], identifier[3]);
             });

    return;
  }

  // The following tasks require M+ configuration to be done.
  if (!IsMotionPlusInDesiredMode())
    return;

  // Now that M+ config has settled we can update the extension number.
  // Updating it too frequently results off/on flashes on M+ mode change.
  UpdateExtensionNumberInput();

  static constexpr u16 NORMAL_CALIBRATION_ADDR = 0x20;

  // Read M+ calibration.
  if (IsMotionPlusActive() && !m_mplus_state.calibration.has_value())
  {
    ReadData(AddressSpace::I2CBus, WiimoteEmu::MotionPlus::ACTIVE_DEVICE_ADDR,
             NORMAL_CALIBRATION_ADDR, sizeof(WiimoteEmu::MotionPlus::CalibrationData),
             [this](ReadResponse response) {
               if (!response)
                 return;

               DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Read M+ calibration.");

               WiimoteEmu::MotionPlus::CalibrationData calibration =
                   Common::BitCastPtr<WiimoteEmu::MotionPlus::CalibrationData>(response->data());

               const auto read_checksum = std::pair(calibration.crc32_lsb, calibration.crc32_msb);

               calibration.UpdateChecksum();

               m_mplus_state.SetCalibrationData(calibration);

               if (read_checksum != std::pair(calibration.crc32_lsb, calibration.crc32_msb))
               {
                 // We could potentially try another read or call the M+ unusable.
                 WARN_LOG_FMT(WIIMOTE, "WiiRemote: Bad M+ calibration checksum.");
               }
             });

    return;
  }

  // Read normal extension calibration.
  if ((m_extension_id == ExtensionID::Nunchuk && !m_nunchuk_state.calibration) ||
      (m_extension_id == ExtensionID::Classic && !m_classic_state.calibration))
  {
    // Extension calibration is normally at 0x20 but M+ reads and stores it at 0x40.
    static constexpr u16 PASSTHROUGH_CALIBRATION_ADDR = 0x40;

    const u16 calibration_addr =
        IsMotionPlusActive() ? PASSTHROUGH_CALIBRATION_ADDR : NORMAL_CALIBRATION_ADDR;
    static constexpr u16 CALIBRATION_SIZE = 0x10;

    ReadData(
        AddressSpace::I2CBus, WiimoteEmu::ExtensionPort::REPORT_I2C_SLAVE, calibration_addr,
        CALIBRATION_SIZE, [this](ReadResponse response) {
          if (!response)
            return;

          DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Read extension calibration.");

          auto& calibration_data = *response;

          const auto read_checksum = std::pair(calibration_data[CALIBRATION_SIZE - 2],
                                               calibration_data[CALIBRATION_SIZE - 1]);

          WiimoteEmu::UpdateCalibrationDataChecksum(calibration_data, 2);

          Checksum checksum = Checksum::Good;

          if (read_checksum != std::pair(calibration_data[CALIBRATION_SIZE - 2],
                                         calibration_data[CALIBRATION_SIZE - 1]))
          {
            // We could potentially try another block or call the extension unusable.
            WARN_LOG_FMT(WIIMOTE, "WiiRemote: Bad extension calibration checksum.");
            checksum = Checksum::Bad;
          }

          if (m_extension_id == ExtensionID::Nunchuk)
          {
            m_nunchuk_state.SetCalibrationData(
                Common::BitCastPtr<WiimoteEmu::Nunchuk::CalibrationData>(calibration_data.data()),
                checksum);
          }
          else if (m_extension_id == ExtensionID::Classic)
          {
            m_classic_state.SetCalibrationData(
                Common::BitCastPtr<WiimoteEmu::Classic::CalibrationData>(calibration_data.data()),
                checksum);
          }
        });

    return;
  }
}

void Device::HandleMotionPlusNonResponse()
{
  // No need for additional checks if an extension is attached.
  // (not possible for M+ to become attached)
  if (m_extension_port == true)
    m_mplus_desired_mode = MotionPlusState::PassthroughMode{};
  else
    WaitForMotionPlus();
}

// Produce LED bitmask for remotes.
// Remotes 1-4 are normal. Additional remotes LED labels will add up to their assigned ID.
u8 Device::GetDesiredLEDValue() const
{
  const auto index = GetId();

  // Normal LED behavior for remotes 1-4.
  if (index < 4)
    return 1 << index;

  // Light LED 4 and LEDs 1 through 3 for remotes 5-7. (Add up the numbers on the remote)
  if (index < 7)
    return 1 << (index - 4) | 8;

  // Light LED 4+3 and LEDs 1 or 2 for remotes 8 or 9. (Add up the numbers on the remote)
  if (index < 9)
    return 1 << (index - 7) | 8 | 4;

  // For remotes 10 and up just light all LEDs.
  return 0xf;
}

void Device::UpdateExtensionNumberInput()
{
  switch (m_extension_id.value_or(ExtensionID::Unsupported))
  {
  case ExtensionID::Nunchuk:
    m_extension_number_input = WiimoteEmu::ExtensionNumber::NUNCHUK;
    break;
  case ExtensionID::Classic:
    m_extension_number_input = WiimoteEmu::ExtensionNumber::CLASSIC;
    break;
  case ExtensionID::Unsupported:
  default:
    m_extension_number_input = WiimoteEmu::ExtensionNumber::NONE;
    break;
  }
}

void Device::ProcessExtensionEvent(bool connected)
{
  // Reset extension state.
  m_nunchuk_state = {};
  m_classic_state = {};

  m_extension_id = std::nullopt;

  // We won't know the desired mode until we get the extension ID.
  if (connected)
    m_mplus_desired_mode = std::nullopt;
}

void Device::ProcessExtensionID(u8 id_0, u8 id_4, u8 id_5)
{
  if (id_4 == 0x00 && id_5 == 0x00)
  {
    INFO_LOG_FMT(WIIMOTE, "WiiRemote: Nunchuk is attached.");
    m_extension_id = ExtensionID::Nunchuk;

    m_mplus_desired_mode = MotionPlusState::PassthroughMode::Nunchuk;
  }
  else if (id_4 == 0x01 && id_5 == 0x01)
  {
    INFO_LOG_FMT(WIIMOTE, "WiiRemote: Classic Controller is attached.");
    m_extension_id = ExtensionID::Classic;

    m_mplus_desired_mode = MotionPlusState::PassthroughMode::Classic;
  }
  else
  {
    // This is a normal occurance before extension initialization.
    DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Unknown extension: {} {} {}.", id_0, id_4, id_5);
    m_extension_id = ExtensionID::Unsupported;
  }
}

void Device::MotionPlusState::SetCalibrationData(
    const WiimoteEmu::MotionPlus::CalibrationData& data)
{
  DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Set M+ calibration.");

  calibration.emplace();

  calibration->fast = data.fast;
  calibration->slow = data.slow;
}

Device::NunchukState::Calibration::Calibration() : accel{}, stick{}
{
  accel.zero.data.fill(1 << (accel.BITS_OF_PRECISION - 1));
  // Approximate 1G value per WiiBrew:
  accel.max.data.fill(740);

  stick.zero.data.fill(1 << (stick.BITS_OF_PRECISION - 1));
  stick.max.data.fill((1 << stick.BITS_OF_PRECISION) - 1);
}

void Device::NunchukState::SetCalibrationData(const WiimoteEmu::Nunchuk::CalibrationData& data,
                                              Checksum checksum)
{
  DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Set Nunchuk calibration.");

  calibration.emplace();

  if (checksum == Checksum::Bad)
    return;

  // Genuine Nunchuks have been observed with "min" and "max" values of zero.
  // We catch that here and fall back to "full range" calibration.
  const auto stick_calibration = data.GetStick();
  if (stick_calibration.IsSane())
  {
    calibration->stick = stick_calibration;
  }
  else
  {
    WARN_LOG_FMT(WIIMOTE,
                 "WiiRemote: Nunchuk stick calibration is not sane. Using fallback values.");
  }

  // No known reports of bad accelerometer calibration but we'll handle it just in case.
  const auto accel_calibration = data.GetAccel();
  if (accel_calibration.IsSane())
  {
    calibration->accel = accel_calibration;
  }
  else
  {
    WARN_LOG_FMT(WIIMOTE,
                 "WiiRemote: Nunchuk accel calibration is not sane. Using fallback values.");
  }
}

Device::ClassicState::Calibration::Calibration()
    : left_stick{}, right_stick{}, left_trigger{}, right_trigger{}
{
  left_stick.zero.data.fill(1 << (left_stick.BITS_OF_PRECISION - 1));
  left_stick.max.data.fill((1 << left_stick.BITS_OF_PRECISION) - 1);

  right_stick.zero.data.fill(1 << (right_stick.BITS_OF_PRECISION - 1));
  right_stick.max.data.fill((1 << right_stick.BITS_OF_PRECISION) - 1);

  left_trigger.max = (1 << left_trigger.BITS_OF_PRECISION) - 1;
  right_trigger.max = (1 << right_trigger.BITS_OF_PRECISION) - 1;
}

void Device::ClassicState::SetCalibrationData(const WiimoteEmu::Classic::CalibrationData& data,
                                              Checksum checksum)
{
  DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Set Classic Controller calibration.");

  calibration.emplace();

  if (checksum == Checksum::Bad)
    return;

  const auto left_stick_calibration = data.GetLeftStick();
  if (left_stick_calibration.IsSane())
  {
    calibration->left_stick = left_stick_calibration;
  }
  else
  {
    WARN_LOG_FMT(WIIMOTE,
                 "WiiRemote: CC left stick calibration is not sane. Using fallback values.");
  }

  const auto right_stick_calibration = data.GetRightStick();
  if (right_stick_calibration.IsSane())
  {
    calibration->right_stick = right_stick_calibration;
  }
  else
  {
    WARN_LOG_FMT(WIIMOTE,
                 "WiiRemote: CC right stick calibration is not sane. Using fallback values.");
  }
  calibration->left_trigger = data.GetLeftTrigger();
  calibration->right_trigger = data.GetRightTrigger();
}

void Device::ReadActiveExtensionID()
{
  static constexpr u16 EXT_ID_ADDR = 0xfa;
  static constexpr u16 EXT_ID_SIZE = 6;

  ReadData(AddressSpace::I2CBus, WiimoteEmu::ExtensionPort::REPORT_I2C_SLAVE, EXT_ID_ADDR,
           EXT_ID_SIZE, [this](ReadResponse response) {
             if (!response)
               return;

             auto& identifier = *response;

             // Check for M+ ID.
             if (identifier[5] == 0x05)
             {
               const auto passthrough_mode = MotionPlusState::PassthroughMode(identifier[4]);

               m_mplus_state.current_mode = passthrough_mode;

               INFO_LOG_FMT(WIIMOTE, "WiiRemote: M+ is active in mode: {}.", int(passthrough_mode));
             }
             else
             {
               m_mplus_state.current_mode = MotionPlusState::PassthroughMode{};

               ProcessExtensionID(identifier[0], identifier[4], identifier[5]);
             }
           });
}

bool Device::IRState::IsFullyConfigured() const
{
  return enabled && mode_set && current_sensitivity == GetDesiredIRSensitivity();
}

u32 Device::IRState::GetDesiredIRSensitivity()
{
  // Wii stores values from 1 to 5. (subtract 1)
  const u32 configured_level = Config::Get(Config::SYSCONF_SENSOR_BAR_SENSITIVITY) - 1;

  if (configured_level < IR_SENSITIVITY_LEVEL_COUNT)
    return configured_level;

  // Default to middle level on bad value.
  return 2;
}

void Device::SetIRSensitivity(u32 level)
{
  struct IRSensitivityConfig
  {
    std::array<u8, 9> block1;
    std::array<u8, 2> block2;
  };

  // Data for Wii levels 1 to 5.
  static constexpr std::array<IRSensitivityConfig, IR_SENSITIVITY_LEVEL_COUNT> sensitivity_configs =
      {{
          {{0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0x64, 0x00, 0xfe}, {0xfd, 0x05}},
          {{0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0x96, 0x00, 0xb4}, {0xb3, 0x04}},
          {{0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xaa, 0x00, 0x64}, {0x63, 0x03}},
          {{0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xc8, 0x00, 0x36}, {0x35, 0x03}},
          {{0x07, 0x00, 0x00, 0x71, 0x01, 0x00, 0x72, 0x00, 0x20}, {0x1f, 0x03}},
      }};

  static constexpr u16 BLOCK1_ADDR = 0x00;
  static constexpr u16 BLOCK2_ADDR = 0x1a;

  DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Setting IR sensitivity: {}.", level + 1);

  const auto& sensitivity_config = sensitivity_configs[level];

  WriteData(AddressSpace::I2CBus, WiimoteEmu::CameraLogic::I2C_ADDR, BLOCK1_ADDR,
            sensitivity_config.block1, [&sensitivity_config, level, this](ErrorCode block_result) {
              if (block_result != ErrorCode::Success)
              {
                WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to write IR block 1.");
                return;
              }

              WriteData(AddressSpace::I2CBus, WiimoteEmu::CameraLogic::I2C_ADDR, BLOCK2_ADDR,
                        sensitivity_config.block2, [&, level, this](ErrorCode block2_result) {
                          if (block2_result != ErrorCode::Success)
                          {
                            WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to write IR block 2.");
                            return;
                          }

                          DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: IR sensitivity set.");

                          m_ir_state.current_sensitivity = level;
                        });
            });
}

void Device::ConfigureIRCamera()
{
  if (!m_ir_state.enabled)
  {
    OutputReportIRLogicEnable2 ir_logic2 = {};
    ir_logic2.ack = 1;
    ir_logic2.enable = 1;
    QueueReport(ir_logic2, [this](ErrorCode result) {
      if (result != ErrorCode::Success)
      {
        WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to enable IR.");
        return;
      }

      OutputReportIRLogicEnable ir_logic = {};
      ir_logic.ack = 1;
      ir_logic.enable = 1;
      QueueReport(ir_logic, [this](ErrorCode ir_result) {
        if (ir_result != ErrorCode::Success)
        {
          WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to enable IR.");
          return;
        }

        DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: IR enabled.");

        m_ir_state.enabled = true;
      });
    });

    return;
  }

  if (const u32 desired_level = IRState::GetDesiredIRSensitivity();
      desired_level != m_ir_state.current_sensitivity)
  {
    SetIRSensitivity(desired_level);

    return;
  }

  if (!m_ir_state.mode_set)
  {
    static constexpr u16 MODE_ADDR = 0x33;

    // We only support "Basic" mode (it's all that fits in ReportCoreAccelIR10Ext6).
    WriteData(AddressSpace::I2CBus, WiimoteEmu::CameraLogic::I2C_ADDR, MODE_ADDR,
              {WiimoteEmu::CameraLogic::IR_MODE_BASIC}, [this](ErrorCode mode_result) {
                if (mode_result != ErrorCode::Success)
                {
                  WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to set IR mode.");
                  return;
                }

                // This seems to enable object tracking.
                static constexpr u16 ENABLE_ADDR = 0x30;
                static constexpr u8 ENABLE_VALUE = 0x08;

                WriteData(AddressSpace::I2CBus, WiimoteEmu::CameraLogic::I2C_ADDR, ENABLE_ADDR,
                          {ENABLE_VALUE}, [this](ErrorCode result) {
                            if (result != ErrorCode::Success)
                            {
                              WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to enable object tracking.");
                              return;
                            }

                            DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: IR mode set.");

                            m_ir_state.mode_set = true;
                          });
              });
  }
}

void Device::ConfigureSpeaker()
{
  OutputReportSpeakerMute mute = {};
  mute.enable = 1;
  mute.ack = 1;
  QueueReport(mute, [this](ErrorCode mute_result) {
    if (mute_result != ErrorCode::Success)
    {
      WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to mute speaker.");
      return;
    }

    OutputReportSpeakerEnable spkr = {};
    spkr.enable = 0;
    spkr.ack = 1;
    QueueReport(spkr, [this](ErrorCode enable_result) {
      if (enable_result != ErrorCode::Success)
      {
        WARN_LOG_FMT(WIIMOTE, "WiiRemote: Failed to disable speaker.");
        return;
      }

      DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Speaker muted and disabled.");

      m_speaker_configured = true;
    });
  });
}

void Device::TriggerMotionPlusModeChange()
{
  if (!m_mplus_desired_mode.has_value())
    return;

  const u8 passthrough_mode = u8(*m_mplus_desired_mode);

  const u8 device_addr = IsMotionPlusActive() ? WiimoteEmu::MotionPlus::ACTIVE_DEVICE_ADDR :
                                                WiimoteEmu::MotionPlus::INACTIVE_DEVICE_ADDR;

  WriteData(AddressSpace::I2CBus, device_addr, WiimoteEmu::MotionPlus::PASSTHROUGH_MODE_OFFSET,
            {passthrough_mode}, [this](ErrorCode activation_result) {
              DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: M+ activation: {}.", int(activation_result));

              WaitForMotionPlus();

              // Normally M+ will be seen performing a reset here. (extension port events)
              // But sometimes (rarely) M+ activation does not cause an extension port event.
              // We'll consider the mode unknown. It will be read back after some time.
              m_mplus_state.current_mode = std::nullopt;
            });
}

void Device::TriggerMotionPlusCalibration()
{
  static constexpr u16 CALIBRATION_TRIGGER_ADDR = 0xf2;
  static constexpr u8 CALIBRATION_TRIGGER_VALUE = 0x00;

  // This triggers a hardware "zero" calibration.
  // The effect is notiecable but output still strays from calibration data.
  // It seems we're better off just manually determining "zero".
  WriteData(AddressSpace::I2CBus, WiimoteEmu::MotionPlus::ACTIVE_DEVICE_ADDR,
            CALIBRATION_TRIGGER_ADDR, {CALIBRATION_TRIGGER_VALUE}, [](ErrorCode result) {
              DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: M+ calibration trigger done: {}.", int(result));
            });
}

bool Device::IsMotionPlusStateKnown() const
{
  return m_mplus_state.current_mode.has_value();
}

bool Device::IsMotionPlusActive() const
{
  return m_mplus_state.current_mode != MotionPlusState::PassthroughMode{};
}

bool Device::IsMotionPlusInDesiredMode() const
{
  return m_mplus_state.current_mode.has_value() &&
         (m_mplus_state.current_mode == m_mplus_desired_mode);
}

void Device::ProcessInputReport(WiimoteReal::Report& report)
{
  if (report.size() < WiimoteReal::REPORT_HID_HEADER_SIZE)
  {
    WARN_LOG_FMT(WIIMOTE, "WiiRemote: Bad report size.");
    return;
  }

  auto report_id = InputReportID(report[WiimoteReal::REPORT_HID_HEADER_SIZE]);

  for (auto it = m_report_handlers.begin(); true;)
  {
    if (it == m_report_handlers.end())
    {
      if (report_id == InputReportID::Status)
      {
        if (report.size() - WiimoteReal::REPORT_HID_HEADER_SIZE <
            sizeof(TypedInputData<InputReportStatus>))
        {
          WARN_LOG_FMT(WIIMOTE, "WiiRemote: Bad report size.");
        }
        else
        {
          ProcessStatusReport(Common::BitCastPtr<InputReportStatus>(report.data() + 2));
        }
      }
      else if (report_id < InputReportID::ReportCore)
      {
        WARN_LOG_FMT(WIIMOTE, "WiiRemote: Unhandled input report: {}.",
                     ArrayToString(report.data(), u32(report.size())));
      }

      break;
    }

    if (it->IsExpired())
    {
      WARN_LOG_FMT(WIIMOTE, "WiiRemote: Removing expired handler.");
      it = m_report_handlers.erase(it);
      continue;
    }

    if (const auto result = it->TryToHandleReport(report);
        result == ReportHandler::HandlerResult::Handled)
    {
      it = m_report_handlers.erase(it);
      break;
    }

    ++it;
  }

  if (report_id < InputReportID::ReportCore)
  {
    // Normal input reports can be processed as "ReportCore".
    report_id = InputReportID::ReportCore;
  }
  else
  {
    // We can assume the last received input report is the current reporting mode.
    // FYI: This logic fails to properly handle the (never used) "interleaved" reports.
    m_reporting_mode = InputReportID(report_id);
  }

  auto manipulator = MakeDataReportManipulator(
      report_id, report.data() + WiimoteReal::REPORT_HID_HEADER_SIZE + sizeof(InputReportID));

  if (manipulator->GetDataSize() + WiimoteReal::REPORT_HID_HEADER_SIZE > report.size())
  {
    WARN_LOG_FMT(WIIMOTE, "WiiRemote: Bad report size.");
    return;
  }

  // Read buttons.
  manipulator->GetCoreData(&m_core_data);

  // Process accel data.
  if (manipulator->HasAccel() && m_accel_calibration.has_value())
  {
    // FYI: This logic fails to properly handle the (never used) "interleaved" reports.
    AccelData accel_data = {};
    manipulator->GetAccelData(&accel_data);

    m_accel_data =
        accel_data.GetNormalizedValue(*m_accel_calibration) * float(MathUtil::GRAVITY_ACCELERATION);
  }

  // Process IR data.
  if (manipulator->HasIR() && m_ir_state.IsFullyConfigured())
  {
    m_ir_state.ProcessData(*manipulator);
  }

  // Process extension data.
  if (IsMotionPlusStateKnown())
  {
    const auto ext_data = manipulator->GetExtDataPtr();
    const auto ext_size = manipulator->GetExtDataSize();

    if (IsMotionPlusActive())
      ProcessMotionPlusExtensionData(ext_data, ext_size);
    else
      ProcessNormalExtensionData(ext_data, ext_size);
  }

  UpdateOrientation();
}

void Device::UpdateOrientation()
{
  const auto current_report_time = Clock::now();
  const auto elapsed_time = std::chrono::duration_cast<std::chrono::duration<float>>(
      current_report_time - m_last_report_time);
  m_last_report_time = current_report_time;

  // Apply M+ gyro data to our orientation.
  m_orientation =
      WiimoteEmu::GetRotationFromGyroscope(m_mplus_state.gyro_data * -1 * elapsed_time.count()) *
      m_orientation;

  // When M+ data is not available give accel/ir data more weight.
  // ComplementaryFilter will then just smooth out our data a bit.
  const bool is_mplus_active = IsMotionPlusStateKnown() && IsMotionPlusActive();

  // With non-zero acceleration data we can perform pitch and roll correction.
  if (m_accel_data.LengthSquared())
  {
    const auto accel_weight = is_mplus_active ? 0.04 : 0.5f;

    m_orientation = WiimoteEmu::ComplementaryFilter(m_orientation, m_accel_data, accel_weight);
  }

  // If IR objects are visible we can perform yaw and pitch correction.
  if (!m_ir_state.is_hidden)
  {
    // FYI: We could do some roll correction from multiple IR objects.

    const auto ir_rotation =
        Common::Vec3(m_ir_state.center_position.y * WiimoteEmu::CameraLogic::CAMERA_FOV_Y, 0,
                     m_ir_state.center_position.x * WiimoteEmu::CameraLogic::CAMERA_FOV_X) /
        2;
    const auto ir_normal = Common::Vec3(0, 1, 0);
    const auto ir_vector = WiimoteEmu::GetRotationFromGyroscope(-ir_rotation) * ir_normal;

    // Pitch correction will be slightly wrong based on sensorbar height.
    // Keep weight below accelerometer weight for that reason.
    // Correction will only happen near pitch zero when the sensorbar is actually in view.
    const auto ir_weight = is_mplus_active ? 0.035 : 0.45f;

    m_orientation = WiimoteEmu::ComplementaryFilter(m_orientation, ir_vector, ir_weight, ir_normal);
  }

  // Normalize for floating point inaccuracies.
  m_orientation = m_orientation.Normalized();

  // Update our (pitch, roll, yaw) inputs now that orientation has been adjusted.
  m_rotation_inputs =
      Common::Vec3{WiimoteEmu::GetPitch(m_orientation), WiimoteEmu::GetRoll(m_orientation),
                   WiimoteEmu::GetYaw(m_orientation)} /
      float(MathUtil::PI);
}

void Device::IRState::ProcessData(const DataReportManipulator& manipulator)
{
  // A better implementation might extrapolate points when they fall out of camera view.
  // But just averaging visible points actually seems to work very well.

  using IRObject = WiimoteEmu::IRObject;

  MathUtil::RunningVariance<Common::Vec2> points;

  const auto camera_max = IRObject(WiimoteEmu::CameraLogic::CAMERA_RES_X - 1,
                                   WiimoteEmu::CameraLogic::CAMERA_RES_Y - 1);

  const auto add_point = [&](IRObject point, u8 size, size_t idx) {
    // Non-visible points are 0xFF-filled.
    if (point.y > camera_max.y)
    {
      raw_ir_object_position[idx].x = 0.0f;
      raw_ir_object_position[idx].y = 0.0f;
      raw_ir_object_size[idx] = 0.0f;
      return;
    }

    raw_ir_object_position[idx].x = static_cast<float>(point.x) / camera_max.x;
    raw_ir_object_position[idx].y = static_cast<float>(point.y) / camera_max.y;
    raw_ir_object_size[idx] = static_cast<float>(size) / 15.0f;

    points.Push(Common::Vec2(point));
  };

  size_t object_index = 0;
  switch (manipulator.GetIRReportFormat())
  {
  case IRReportFormat::Basic:
  {
    const std::array<WiimoteEmu::IRBasic, 2> data =
        Common::BitCastPtr<std::array<WiimoteEmu::IRBasic, 2>>(manipulator.GetIRDataPtr());
    for (const auto& block : data)
    {
      // size is not reported by IRBasic, just assume a typical size
      add_point(block.GetObject1(), 2, object_index);
      ++object_index;
      add_point(block.GetObject2(), 2, object_index);
      ++object_index;
    }
    break;
  }
  case IRReportFormat::Extended:
  {
    const std::array<WiimoteEmu::IRExtended, 4> data =
        Common::BitCastPtr<std::array<WiimoteEmu::IRExtended, 4>>(manipulator.GetIRDataPtr());
    for (const auto& object : data)
    {
      add_point(object.GetPosition(), object.size, object_index);
      ++object_index;
    }
    break;
  }
  default:
    // unsupported format
    return;
  }

  is_hidden = !points.Count();

  if (points.Count() >= 2)
  {
    const auto variance = points.PopulationVariance();
    // Adjusts Y coorinate to match horizontal FOV.
    const auto separation =
        Common::Vec2(std::sqrt(variance.x), std::sqrt(variance.y)) /
        Common::Vec2(WiimoteEmu::CameraLogic::CAMERA_RES_X,
                     WiimoteEmu::CameraLogic::CAMERA_RES_Y * WiimoteEmu::CameraLogic::CAMERA_AR) *
        2;

    distance = WiimoteEmu::CameraLogic::SENSOR_BAR_LED_SEPARATION / separation.Length() / 2 /
               std::tan(WiimoteEmu::CameraLogic::CAMERA_FOV_X / 2);
  }

  if (points.Count())
  {
    center_position = points.Mean() / Common::Vec2(camera_max) * 2.f - Common::Vec2(1, 1);
  }
  else
  {
    center_position = {};
  }
}

void Device::ProcessMotionPlusExtensionData(const u8* ext_data, u32 ext_size)
{
  if (ext_size < sizeof(WiimoteEmu::MotionPlus::DataFormat))
    return;

  const WiimoteEmu::MotionPlus::DataFormat mplus_data =
      Common::BitCastPtr<WiimoteEmu::MotionPlus::DataFormat>(ext_data);

  const bool is_ext_connected = mplus_data.extension_connected;

  // Handle passthrough extension change.
  if (is_ext_connected != m_mplus_state.passthrough_port)
  {
    m_mplus_state.passthrough_port = is_ext_connected;

    DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: M+ passthrough port event: {}.", is_ext_connected);

    // With no passthrough extension we'll be happy with the current mode.
    if (!is_ext_connected)
      m_mplus_desired_mode = m_mplus_state.current_mode;

    ProcessExtensionEvent(is_ext_connected);
  }

  if (mplus_data.is_mp_data)
  {
    m_mplus_state.ProcessData(mplus_data);
    return;
  }

  if (!IsMotionPlusInDesiredMode())
  {
    DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Ignoring unwanted passthrough data.");
    return;
  }

  std::array<u8, sizeof(WiimoteEmu::Nunchuk::DataFormat)> data;
  std::copy_n(ext_data, ext_size, data.begin());

  // Undo bit-hacks of M+ passthrough.
  WiimoteEmu::MotionPlus::ReversePassthroughModifications(*m_mplus_state.current_mode, data.data());

  ProcessNormalExtensionData(data.data(), u32(data.size()));
}

void Device::ProcessNormalExtensionData(const u8* ext_data, u32 ext_size)
{
  if (m_extension_id == ExtensionID::Nunchuk)
  {
    if (ext_size < sizeof(WiimoteEmu::MotionPlus::DataFormat))
      return;

    const WiimoteEmu::Nunchuk::DataFormat nunchuk_data =
        Common::BitCastPtr<WiimoteEmu::Nunchuk::DataFormat>(ext_data);

    m_nunchuk_state.ProcessData(nunchuk_data);
  }
  else if (m_extension_id == ExtensionID::Classic)
  {
    if (ext_size < sizeof(WiimoteEmu::Classic::DataFormat))
      return;

    const WiimoteEmu::Classic::DataFormat cc_data =
        Common::BitCastPtr<WiimoteEmu::Classic::DataFormat>(ext_data);

    m_classic_state.ProcessData(cc_data);
  }
}

void Device::UpdateRumble()
{
  static constexpr auto rumble_period = std::chrono::milliseconds(100);

  const auto on_time =
      std::chrono::duration_cast<Clock::duration>(rumble_period * m_rumble_level.load());
  const auto off_time = rumble_period - on_time;

  const auto now = Clock::now();

  if (m_rumble && (now < m_last_rumble_change + on_time || !off_time.count()))
    return;

  if (!m_rumble && (now < m_last_rumble_change + off_time || !on_time.count()))
    return;

  m_last_rumble_change = now;
  m_rumble ^= true;

  // Rumble flag will be set within QueueReport.
  QueueReport(OutputReportRumble{});
}

Core::DeviceRemoval Device::UpdateInput()
{
  if (!m_wiimote->IsConnected())
    return Core::DeviceRemoval::Remove;

  UpdateRumble();
  RunTasks();

  WiimoteReal::Report report;
  while (m_wiimote->GetNextReport(&report))
  {
    ProcessInputReport(report);
    RunTasks();
  }

  return Core::DeviceRemoval::Keep;
}

void Device::MotionPlusState::ProcessData(const WiimoteEmu::MotionPlus::DataFormat& data)
{
  // We need the calibration block read to know the sensor orientations.
  if (!calibration.has_value())
    return;

  gyro_data = data.GetData().GetAngularVelocity(*calibration);
}

bool Device::IsWaitingForMotionPlus() const
{
  return Clock::now() < m_mplus_wait_time;
}

void Device::WaitForMotionPlus()
{
  DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Wait for M+.");
  m_mplus_wait_time = Clock::now() + std::chrono::seconds{2};
}

void Device::NunchukState::ProcessData(const WiimoteEmu::Nunchuk::DataFormat& data)
{
  buttons = data.GetButtons();

  // Stick/accel require calibration data.
  if (!calibration.has_value())
    return;

  stick = data.GetStick().GetNormalizedValue(calibration->stick);
  accel = data.GetAccel().GetNormalizedValue(calibration->accel) *
          float(MathUtil::GRAVITY_ACCELERATION);
}

void Device::ClassicState::ProcessData(const WiimoteEmu::Classic::DataFormat& data)
{
  buttons = data.GetButtons();

  // Sticks/triggers require calibration data.
  if (!calibration.has_value())
    return;

  sticks[0] = data.GetLeftStick().GetNormalizedValue(calibration->left_stick);
  sticks[1] = data.GetRightStick().GetNormalizedValue(calibration->right_stick);
  triggers[0] = data.GetLeftTrigger().GetNormalizedValue(calibration->left_trigger);
  triggers[1] = data.GetRightTrigger().GetNormalizedValue(calibration->right_trigger);
}

void Device::ReadData(AddressSpace space, u8 slave, u16 address, u16 size,
                      std::function<void(ReadResponse)> callback)
{
  OutputReportReadData read_data{};
  read_data.space = u8(space);
  read_data.slave_address = slave;
  read_data.address[0] = u8(address >> 8);
  read_data.address[1] = u8(address);
  read_data.size[0] = u8(size >> 8);
  read_data.size[1] = u8(size);
  QueueReport(read_data);

  AddReadDataReplyHandler(space, slave, address, size, {}, std::move(callback));
}

void Device::AddReadDataReplyHandler(AddressSpace space, u8 slave, u16 address, u16 size,
                                     std::vector<u8> starting_data,
                                     std::function<void(ReadResponse)> callback)
{
  // Data read may return a busy ack.
  auto ack_handler = MakeAckHandler(OutputReportID::ReadData, [callback](ErrorCode result) {
    DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Read ack error: {}.", int(result));
    callback(ReadResponse{});
  });

  // Or more normally a "ReadDataReply".
  auto read_handler = [this, space, slave, address, size, data = std::move(starting_data),
                       callback =
                           std::move(callback)](const InputReportReadDataReply& reply) mutable {
    if (Common::swap16(reply.address) != address)
      return ReportHandler::HandlerResult::NotHandled;

    if (reply.error != u8(ErrorCode::Success))
    {
      DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Read reply error: {}.", int(reply.error));
      callback(ReadResponse{});

      return ReportHandler::HandlerResult::Handled;
    }

    const auto read_count = reply.size_minus_one + 1;

    data.insert(data.end(), reply.data, reply.data + read_count);

    if (read_count < size)
    {
      // We have more data to acquire.
      AddReadDataReplyHandler(space, slave, address + read_count, size - read_count,
                              std::move(data), std::move(callback));
    }
    else
    {
      // We have all the data.
      callback(std::move(data));
    }

    return ReportHandler::HandlerResult::Handled;
  };

  AddReportHandler(
      std::function<ReportHandler::HandlerResult(const InputReportReadDataReply& reply)>(
          std::move(read_handler)),
      std::move(ack_handler));
}

template <typename T, typename C>
void Device::WriteData(AddressSpace space, u8 slave, u16 address, T&& data, C&& callback)
{
  OutputReportWriteData write_data = {};
  write_data.space = u8(space);
  write_data.slave_address = slave;
  write_data.address[0] = u8(address >> 8);
  write_data.address[1] = u8(address);

  static constexpr auto MAX_DATA_SIZE = std::size(write_data.data);
  write_data.size = u8(std::min(std::size(data), MAX_DATA_SIZE));

  std::copy_n(std::begin(data), write_data.size, write_data.data);

  // Writes of more than 16 bytes must be split into multiple reports.
  if (std::size(data) > MAX_DATA_SIZE)
  {
    auto next_write = [this, space, slave, address,
                       additional_data =
                           std::vector<u8>(std::begin(data) + MAX_DATA_SIZE, std::end(data)),
                       callback = std::forward<C>(callback)](ErrorCode result) mutable {
      if (result != ErrorCode::Success)
        callback(result);
      else
        WriteData(space, slave, address + MAX_DATA_SIZE, additional_data, std::move(callback));
    };

    QueueReport(write_data, std::move(next_write));
  }
  else
  {
    QueueReport(write_data, std::forward<C>(callback));
  }
}

Device::ReportHandler::ReportHandler(Clock::time_point expired_time) : m_expired_time(expired_time)
{
}

template <typename... T>
void Device::AddReportHandler(T&&... callbacks)
{
  auto& handler = m_report_handlers.emplace_back(Clock::now() + std::chrono::seconds{5});
  (handler.AddHandler(std::forward<T>(callbacks)), ...);
}

template <typename R, typename T>
void Device::ReportHandler::AddHandler(std::function<R(const T&)> handler)
{
  m_callbacks.emplace_back([handler = std::move(handler)](const WiimoteReal::Report& report) {
    if (report[WiimoteReal::REPORT_HID_HEADER_SIZE] != u8(T::REPORT_ID))
      return ReportHandler::HandlerResult::NotHandled;

    T data;

    if (report.size() < sizeof(T) + WiimoteReal::REPORT_HID_HEADER_SIZE + 1)
    {
      // Off-brand "NEW 2in1" Wii Remote likes to shorten read data replies.
      WARN_LOG_FMT(WIIMOTE, "WiiRemote: Bad report size ({}) for report {:#x}. Zero-filling.",
                   report.size(), int(T::REPORT_ID));

      data = {};
      std::memcpy(&data, report.data() + WiimoteReal::REPORT_HID_HEADER_SIZE + 1,
                  report.size() - WiimoteReal::REPORT_HID_HEADER_SIZE + 1);
    }
    else
    {
      data = Common::BitCastPtr<T>(report.data() + WiimoteReal::REPORT_HID_HEADER_SIZE + 1);
    }

    if constexpr (std::is_same_v<decltype(handler(data)), void>)
    {
      handler(data);
      return ReportHandler::HandlerResult::Handled;
    }
    else
    {
      return handler(data);
    }
  });
}

auto Device::ReportHandler::TryToHandleReport(const WiimoteReal::Report& report) -> HandlerResult
{
  for (auto& callback : m_callbacks)
  {
    if (const auto result = callback(report); result != HandlerResult::NotHandled)
      return result;
  }

  return HandlerResult::NotHandled;
}

bool Device::ReportHandler::IsExpired() const
{
  return Clock::now() >= m_expired_time;
}

auto Device::MakeAckHandler(OutputReportID report_id,
                            std::function<void(WiimoteCommon::ErrorCode)> callback)
    -> AckReportHandler
{
  return [report_id, callback = std::move(callback)](const InputReportAck& reply) {
    if (reply.rpt_id != report_id)
      return ReportHandler::HandlerResult::NotHandled;

    callback(reply.error_code);
    return ReportHandler::HandlerResult::Handled;
  };
}

bool Device::IsPerformingTask() const
{
  return !m_report_handlers.empty();
}

void Device::ProcessStatusReport(const InputReportStatus& status)
{
  // Update status periodically to keep battery level value up to date.
  m_status_outdated_time = Clock::now() + std::chrono::seconds(10);

  m_battery = status.GetEstimatedCharge() * BATTERY_INPUT_MAX_VALUE;
  m_leds = status.leds;

  if (!status.ir)
    m_ir_state = {};

  const bool is_ext_connected = status.extension;

  // Handle extension port state change.
  if (is_ext_connected != m_extension_port)
  {
    DEBUG_LOG_FMT(WIIMOTE, "WiiRemote: Extension port event: {}.", is_ext_connected);

    m_extension_port = is_ext_connected;

    // Data reporting stops on an extension port event.
    m_reporting_mode = InputReportID::ReportDisabled;

    ProcessExtensionEvent(is_ext_connected);

    // The M+ is now in an unknown state.
    m_mplus_state = {};

    if (is_ext_connected)
    {
      // We can assume the M+ is settled on an attachment event.
      m_mplus_wait_time = Clock::now();
    }
    else
    {
      // "Nunchuk" will be the most used mode and also works with no passthrough extension.
      m_mplus_desired_mode = MotionPlusState::PassthroughMode::Nunchuk;

      // If an extension is not connected the M+ is either disabled or resetting.
      m_mplus_state.current_mode = MotionPlusState::PassthroughMode{};
    }
  }
}

}  // namespace ciface::WiimoteController
