// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"

namespace WiimoteEmu
{
struct DesiredWiimoteState;
}

namespace WiimoteCommon
{
// Source: HID_010_SPC_PFL/1.0 (official HID specification)

constexpr u8 HID_TYPE_HANDSHAKE = 0;
constexpr u8 HID_TYPE_SET_REPORT = 5;
constexpr u8 HID_TYPE_DATA = 0xA;

constexpr u8 HID_HANDSHAKE_SUCCESS = 0;

constexpr u8 HID_PARAM_INPUT = 1;
constexpr u8 HID_PARAM_OUTPUT = 2;

class HIDWiimote
{
public:
  using InterruptCallbackType = std::function<void(u8 hid_type, const u8* data, u32 size)>;

  virtual ~HIDWiimote() = default;

  virtual void EventLinked() = 0;
  virtual void EventUnlinked() = 0;

  virtual u8 GetWiimoteDeviceIndex() const = 0;
  virtual void SetWiimoteDeviceIndex(u8 index) = 0;

  enum class SensorBarState : bool
  {
    Disabled,
    Enabled
  };

  // Called every ~200hz after HID channels are established.
  virtual void PrepareInput(WiimoteEmu::DesiredWiimoteState* target_state,
                            SensorBarState sensor_bar_state) = 0;
  virtual void Update(const WiimoteEmu::DesiredWiimoteState& target_state) = 0;

  void SetInterruptCallback(InterruptCallbackType callback) { m_callback = std::move(callback); }

  // HID report type:0xa2 (data output) payloads sent to the wiimote interrupt channel.
  // Does not include HID-type header.
  virtual void InterruptDataOutput(const u8* data, u32 size) = 0;

  // Get a snapshot of the current state of the Wiimote's buttons.
  // Note that only the button bits of the return value are meaningful, the rest should be ignored.
  // This is used to query a disconnected Wiimote whether it wants to reconnect.
  virtual ButtonData GetCurrentlyPressedButtons() = 0;

protected:
  void InterruptDataInputCallback(const u8* data, u32 size)
  {
    InterruptCallback((WiimoteCommon::HID_TYPE_DATA << 4) | WiimoteCommon::HID_PARAM_INPUT, data,
                      size);
  }

  void InterruptCallback(u8 hid_type, const u8* data, u32 size)
  {
    m_callback(hid_type, data, size);
  }

private:
  InterruptCallbackType m_callback;
};

#pragma pack(push, 1)

template <typename T>
struct TypedInputData
{
  TypedInputData(InputReportID _rpt_id) : report_id(_rpt_id) {}

  InputReportID report_id;
  T payload = {};

  static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>);

  u8* GetData() { return reinterpret_cast<u8*>(this); }
  const u8* GetData() const { return reinterpret_cast<const u8*>(this); }
  constexpr u32 GetSize() const { return sizeof(*this); }
};

#pragma pack(pop)

}  // namespace WiimoteCommon
