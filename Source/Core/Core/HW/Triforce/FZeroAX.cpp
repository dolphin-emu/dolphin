// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/FZeroAX.h"

#include <numeric>

#include <fmt/ranges.h>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "Core/HW/GCPad.h"

#include "InputCommon/GCPadStatus.h"

namespace Triforce
{

void FZeroAXCommon_IOAdapter::Update()
{
  auto* const io_ports = GetIOPorts();

  // Horizontal Scanning Frequency switch.
  // Required for booting via Sega Boot.
  io_ports->GetStatusSwitches()[0] &= ~0x20u;

  const GCPadStatus pad_status = Pad::GetStatus(0);

  const auto switch_inputs_0 = io_ports->GetSwitchInputs(0);
  // Start
  if (pad_status.button & PAD_BUTTON_START)
    switch_inputs_0[0] |= 0x80;
  // View Change 1
  if (pad_status.button & PAD_BUTTON_RIGHT)
    switch_inputs_0[0] |= 0x20;
  // View Change 2
  if (pad_status.button & PAD_BUTTON_LEFT)
    switch_inputs_0[0] |= 0x10;
  // View Change 3
  if (pad_status.button & PAD_BUTTON_UP)
    switch_inputs_0[0] |= 0x08;
  // View Change 4
  if (pad_status.button & PAD_BUTTON_DOWN)
    switch_inputs_0[0] |= 0x04;
  // Boost
  if (pad_status.button & PAD_BUTTON_A)
    switch_inputs_0[0] |= 0x02;

  const auto switch_inputs_1 = io_ports->GetSwitchInputs(1);
  //  Paddle left
  if (pad_status.button & PAD_BUTTON_X)
    switch_inputs_1[0] |= 0x20;
  //  Paddle right
  if (pad_status.button & PAD_BUTTON_Y)
    switch_inputs_1[0] |= 0x10;

  const auto analog_inputs = io_ports->GetAnalogInputs();

  // Steering
  if (m_steering_wheel->IsInitializing())
  {
    // Override X position during initialization to make the calibration happy.
    analog_inputs[0] =
        (m_steering_wheel->GetServoPosition() * (1 << 9)) + IOPorts::NEUTRAL_ANALOG_VALUE;
  }
  else
  {
    analog_inputs[0] = Common::ExpandValue<u16>(pad_status.stickX, 8);
  }

  analog_inputs[1] = Common::ExpandValue<u16>(pad_status.stickY, 8);

  // Gas
  analog_inputs[4] = Common::ExpandValue<u16>(pad_status.triggerRight, 8);
  // Brake
  analog_inputs[5] = Common::ExpandValue<u16>(pad_status.triggerLeft, 8);
  // Seat Motion
  analog_inputs[6] = IOPorts::NEUTRAL_ANALOG_VALUE;
}

void FZeroAXCommon_IOAdapter::HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                                          std::span<const u8> bits_cleared)
{
  const u8 bits_changed_0 = bits_set[0] | bits_cleared[0];

  constexpr auto LED_NAMES = std::to_array<std::pair<u8, const char*>>({
      {0x80, "START BUTTON"},
      {0x20, "VIEW CHANGE 1"},
      {0x10, "VIEW CHANGE 2"},
      {0x08, "VIEW CHANGE 3"},
      {0x04, "VIEW CHANGE 4"},
      {0x40, "BOOST"},
  });

  for (const auto& [led_value, led_name] : LED_NAMES)
  {
    if (bits_changed_0 & led_value)
    {
      INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "JVS-IO: {}: {}", led_name,
                   (bits_set[0] & led_value) ? "ON" : "OFF");
    }
  }
}

void FZeroAXDeluxe_IOAdapter::Update()
{
  auto* const io_ports = GetIOPorts();
  const auto generic_outputs = io_ports->GetGenericOutputs();

  // Not fully understood trickery to satisfy the game's initialization sequence.
  const u16 seat_state = Common::swap16(generic_outputs.data() + 1) >> 2;
  switch (seat_state)
  {
  case 0x70:
    ++m_delay;
    if ((m_delay % 10) == 0)
    {
      m_rx_reply = 0xFB;
    }
    break;
  case 0xF0:
    m_rx_reply = 0xF0;
    break;
  default:
  case 0xA0:
  case 0x60:
    break;
  }

  constexpr bool seatbelt = true;
  constexpr bool motion_stop = false;
  constexpr bool sensor_left = false;
  constexpr bool sensor_right = false;

  const auto switch_inputs_p0 = io_ports->GetSwitchInputs(0);

  if (seatbelt)
    switch_inputs_p0[0] |= 0x01;

  switch_inputs_p0[1] = m_rx_reply & 0xF0;

  const auto switch_inputs_p1 = io_ports->GetSwitchInputs(1);

  if (sensor_left)
    switch_inputs_p1[0] |= 0x08;
  if (sensor_right)
    switch_inputs_p1[0] |= 0x04;
  if (motion_stop)
    switch_inputs_p1[0] |= 0x02;

  switch_inputs_p1[1] = m_rx_reply << 4;
}

void FZeroAXMonster_IOAdapter::Update()
{
  auto* const io_ports = GetIOPorts();

  constexpr bool sensor = false;
  constexpr bool emergency = false;
  constexpr bool service = false;
  constexpr bool seatbelt = true;

  const auto switch_inputs_p0 = io_ports->GetSwitchInputs(0);

  if (sensor)
    switch_inputs_p0[0] |= 0x01;

  const auto switch_inputs_p1 = io_ports->GetSwitchInputs(1);

  if (emergency)
    switch_inputs_p1[0] |= 0x08;
  if (service)
    switch_inputs_p1[0] |= 0x04;
  if (seatbelt)
    switch_inputs_p1[0] |= 0x02;
}

void FZeroAXDeluxe_IOAdapter::DoState(PointerWrap& p)
{
  p.Do(m_delay);
  p.Do(m_rx_reply);
}

void FZeroAXSteeringWheel::Update()
{
  constexpr std::size_t REQUEST_SIZE = 4;

  std::size_t rx_position = 0;
  while (true)
  {
    const auto rx_span = GetRxByteSpan().subspan(rx_position);

    if (rx_span.size() < REQUEST_SIZE)
      break;  // Wait for more data.

    const auto request = rx_span.first<REQUEST_SIZE>();

    // The first byte is XOR'd with 0x80.
    // The last byte is an XOR of the previous bytes.
    if (std::accumulate(request.begin(), request.end(), u8{0x80}, std::bit_xor{}) != 0)
    {
      WARN_LOG_FMT(SERIALINTERFACE_AMBB, "SteeringWheel: Bad checksum!");
      ++rx_position;
      continue;
    }

    ProcessRequest(request);
    rx_position += REQUEST_SIZE;
  }

  ConsumeRxBytes(rx_position);
}

void FZeroAXSteeringWheel::ProcessRequest(std::span<const u8> request)
{
  DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "SteeringWheel: Request: {:02x}", fmt::join(request, " "));

  // The first byte is XOR'd with 0x80.
  const u8 cmd = request[0] ^ 0x80u;
  switch (cmd)
  {
  case 0:  // Power on/off ?
           // Sent before force commands: 00 01
           // Sent after force commands:  00 00
  case 1:  // Set Maximum?
  case 2:
    break;

  case 4:  // Move Steering Wheel
  {
    // This seems to be a u8 value but the MSb is the LSb of the previous byte.
    // e.g. 01 7f -> ff

    // This produces a value in the range around [-56, +56].
    m_servo_position = s8(0x80 - (u8(request[1] << 7u) | request[2]));

    DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "SteeringWheel: servo_position: {}", m_servo_position);
    break;
  }

  case 6:  // nice
  case 9:
  default:
    break;

  // Switch back to normal controls
  case 7:
    m_init_state = 2;
    break;

  // Reset
  case 0x7F:
    m_init_state = 1;
    break;
  }

  // Simple 4 byte response.
  WriteTxBytes(std::array<u8, 4>{});
}

bool FZeroAXSteeringWheel::IsInitializing() const
{
  return m_init_state == 1;
}

void FZeroAXSteeringWheel::DoState(PointerWrap& p)
{
  p.Do(m_init_state);
  p.Do(m_servo_position);
}

}  // namespace Triforce
