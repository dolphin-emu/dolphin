// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Camera.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/MotionPlus.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ciface::WiimoteController
{
using namespace WiimoteCommon;

void AddDevice(std::unique_ptr<WiimoteReal::Wiimote>);
void ReleaseDevices(std::optional<u32> count = std::nullopt);

class Device final : public Core::Device
{
public:
  Device(std::unique_ptr<WiimoteReal::Wiimote> wiimote);
  ~Device();

  std::string GetName() const override;
  std::string GetSource() const override;
  int GetSortPriority() const override;

  Core::DeviceRemoval UpdateInput() override;

private:
  using Clock = std::chrono::steady_clock;

  enum class ExtensionID
  {
    Nunchuk,
    Classic,
    Unsupported,
  };

  enum class Checksum
  {
    Good,
    Bad,
  };

  class MotionPlusState
  {
  public:
    void SetCalibrationData(const WiimoteEmu::MotionPlus::CalibrationData&);
    void ProcessData(const WiimoteEmu::MotionPlus::DataFormat&);

    using PassthroughMode = WiimoteEmu::MotionPlus::PassthroughMode;

    // State is unknown by default.
    std::optional<PassthroughMode> current_mode;

    // The last known state of the passthrough port flag.
    // Used to detect passthrough extension port events.
    std::optional<bool> passthrough_port;

    Common::Vec3 gyro_data = {};

    std::optional<WiimoteEmu::MotionPlus::CalibrationBlocks> calibration;
  };

  struct NunchukState
  {
    using CalibrationData = WiimoteEmu::Nunchuk::CalibrationData;

    void SetCalibrationData(const CalibrationData&, Checksum);
    void ProcessData(const WiimoteEmu::Nunchuk::DataFormat&);

    Common::Vec2 stick = {};
    Common::Vec3 accel = {};

    u8 buttons = 0;

    struct Calibration
    {
      Calibration();

      CalibrationData::AccelCalibration accel;
      CalibrationData::StickCalibration stick;
    };

    std::optional<Calibration> calibration;
  };

  struct ClassicState
  {
    using CalibrationData = WiimoteEmu::Classic::CalibrationData;

    void SetCalibrationData(const CalibrationData&, Checksum);
    void ProcessData(const WiimoteEmu::Classic::DataFormat&);

    std::array<Common::Vec2, 2> sticks = {};
    std::array<float, 2> triggers = {};

    u16 buttons = 0;

    struct Calibration
    {
      Calibration();

      CalibrationData::StickCalibration left_stick;
      CalibrationData::StickCalibration right_stick;

      CalibrationData::TriggerCalibration left_trigger;
      CalibrationData::TriggerCalibration right_trigger;
    };

    std::optional<Calibration> calibration;
  };

  struct IRState
  {
    static u32 GetDesiredIRSensitivity();

    void ProcessData(const std::array<WiimoteEmu::IRBasic, 2>&);
    bool IsFullyConfigured() const;

    u32 current_sensitivity = u32(-1);
    bool enabled = false;
    bool mode_set = false;

    // Average of visible IR "objects".
    Common::Vec2 center_position = {};

    float distance = 0;

    bool is_hidden = true;
  };

  class ReportHandler
  {
  public:
    enum class HandlerResult
    {
      Handled,
      NotHandled,
    };

    ReportHandler(Clock::time_point expired_time);

    template <typename R, typename T>
    void AddHandler(std::function<R(const T&)>);

    HandlerResult TryToHandleReport(const WiimoteReal::Report& report);

    bool IsExpired() const;

  private:
    const Clock::time_point m_expired_time;
    std::vector<std::function<HandlerResult(const WiimoteReal::Report& report)>> m_callbacks;
  };

  using AckReportHandler = std::function<ReportHandler::HandlerResult(const InputReportAck& reply)>;

  static AckReportHandler MakeAckHandler(OutputReportID report_id,
                                         std::function<void(WiimoteCommon::ErrorCode)> callback);

  // TODO: Make parameter const. (need to modify DataReportManipulator)
  void ProcessInputReport(WiimoteReal::Report& report);
  void ProcessMotionPlusExtensionData(const u8* data, u32 size);
  void ProcessNormalExtensionData(const u8* data, u32 size);
  void ProcessExtensionEvent(bool connected);
  void ProcessExtensionID(u8 id_0, u8 id_4, u8 id_5);
  void ProcessStatusReport(const InputReportStatus&);

  void RunTasks();

  bool IsPerformingTask() const;

  template <typename T>
  void QueueReport(T&& report, std::function<void(ErrorCode)> ack_callback = {});

  template <typename... T>
  void AddReportHandler(T&&... callbacks);

  using ReadResponse = std::optional<std::vector<u8>>;

  void ReadData(AddressSpace space, u8 slave, u16 address, u16 size,
                std::function<void(ReadResponse)> callback);

  void AddReadDataReplyHandler(AddressSpace space, u8 slave, u16 address, u16 size,
                               std::vector<u8> starting_data,
                               std::function<void(ReadResponse)> callback);

  template <typename T = std::initializer_list<u8>, typename C>
  void WriteData(AddressSpace space, u8 slave, u16 address, T&& data, C&& callback);

  void ReadActiveExtensionID();
  void SetIRSensitivity(u32 level);
  void ConfigureSpeaker();
  void ConfigureIRCamera();

  u8 GetDesiredLEDValue() const;

  void TriggerMotionPlusModeChange();
  void TriggerMotionPlusCalibration();

  bool IsMotionPlusStateKnown() const;
  bool IsMotionPlusActive() const;
  bool IsMotionPlusInDesiredMode() const;

  bool IsWaitingForMotionPlus() const;
  void WaitForMotionPlus();
  void HandleMotionPlusNonResponse();

  void UpdateRumble();
  void UpdateOrientation();
  void UpdateExtensionNumberInput();

  std::unique_ptr<WiimoteReal::Wiimote> m_wiimote;

  // Buttons.
  DataReportManipulator::CoreData m_core_data = {};

  // Accelerometer.
  Common::Vec3 m_accel_data = {};
  std::optional<AccelCalibrationData::Calibration> m_accel_calibration;

  // Pitch, Roll, Yaw inputs.
  Common::Vec3 m_rotation_inputs = {};

  MotionPlusState m_mplus_state = {};
  NunchukState m_nunchuk_state = {};
  ClassicState m_classic_state = {};
  IRState m_ir_state = {};

  // Used to poll for M+ periodically and wait for it to reset.
  Clock::time_point m_mplus_wait_time = Clock::now();

  // The desired mode is set based on the attached normal extension.
  std::optional<MotionPlusState::PassthroughMode> m_mplus_desired_mode;

  // Status report is requested every so often to update the battery level.
  Clock::time_point m_status_outdated_time = Clock::now();
  float m_battery = 0;
  u8 m_leds = 0;

  bool m_speaker_configured = false;

  // The last known state of the extension port status flag.
  // Used to detect extension port events.
  std::optional<bool> m_extension_port;

  // Note this refers to the passthrough extension when M+ is active.
  std::optional<ExtensionID> m_extension_id;

  // Rumble state must be saved to set the proper flag in every output report.
  bool m_rumble = false;

  // For pulse of rumble motor to simulate multiple levels.
  std::atomic<ControlState> m_rumble_level;
  Clock::time_point m_last_rumble_change = Clock::now();

  // Assume mode is disabled so one gets set.
  InputReportID m_reporting_mode = InputReportID::ReportDisabled;

  // Used only to provide a value for a specialty "input". (for attached extension passthrough)
  WiimoteEmu::ExtensionNumber m_extension_number_input = WiimoteEmu::ExtensionNumber::NONE;
  bool m_mplus_attached_input = false;

  // Holds callbacks for output report replies.
  std::list<ReportHandler> m_report_handlers;

  // World rotation. (used to rotate IR data and provide pitch, roll, yaw inputs)
  Common::Quaternion m_orientation = Common::Quaternion::Identity();
  Clock::time_point m_last_report_time = Clock::now();
};

}  // namespace ciface::WiimoteController
