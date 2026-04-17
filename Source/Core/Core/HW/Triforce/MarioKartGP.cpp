// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/MarioKartGP.h"

#include <fmt/ranges.h>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "Core/HW/GCPad.h"

#include "InputCommon/GCPadStatus.h"

namespace Triforce
{

void MarioKartGPCommon_IOAdapter::Update()
{
  auto* const io_ports = GetIOPorts();

  // TODO: Devices (e.g. Camera, Networking) can be disabled but it's not yet fully figured out.
  constexpr u8 DISABLE_DEVICES = 0xff;
  io_ports->GetStatusSwitches()[0] &= DISABLE_DEVICES;

  const GCPadStatus pad_status = Pad::GetStatus(0);

  const auto switch_inputs = io_ports->GetSwitchInputs(0);
  // Start
  if (pad_status.button & PAD_BUTTON_START)
    switch_inputs[0] |= 0x80;

  // Item button
  if (pad_status.button & PAD_BUTTON_A)
    switch_inputs[1] |= 0x20;
  // VS-Cancel button
  if (pad_status.button & PAD_BUTTON_B)
    switch_inputs[1] |= 0x02;

  const auto analog_inputs = io_ports->GetAnalogInputs();
  // Steering
  analog_inputs[0] = Common::ExpandValue<u16>(pad_status.stickX, 8);
  // Gas
  analog_inputs[1] = Common::ExpandValue<u16>(pad_status.triggerRight, 8);
  // Brake
  analog_inputs[2] = Common::ExpandValue<u16>(pad_status.triggerLeft, 8);
}

void MarioKartGPCommon_IOAdapter::HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                                              std::span<const u8> bits_cleared)
{
  if (bits_set[0] & 0x80u)
    m_steering_wheel->Reset();

  const u8 bits_changed_0 = bits_set[0] | bits_cleared[0];

  constexpr auto LED_NAMES = std::to_array<std::pair<u8, const char*>>({
      {0x04, "ITEM BUTTON"},
      {0x08, "CANCEL BUTTON"},
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

void MarioKartGPSteeringWheel::Update()
{
  constexpr std::size_t REQUEST_SIZE = 10;

  std::size_t rx_position = 0;
  while (true)
  {
    const auto rx_span = GetRxByteSpan().subspan(rx_position);

    if (rx_span.size() < REQUEST_SIZE)
      break;  // Wait for more data.

    ProcessRequest(rx_span.first<REQUEST_SIZE>());
    rx_position += REQUEST_SIZE;
  }

  ConsumeRxBytes(rx_position);
}

void MarioKartGPSteeringWheel::ProcessRequest(std::span<const u8> request)
{
  DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "SteeringWheel: Request: {:02x}", fmt::join(request, " "));

  const u8 cmd = request[3];
  if (cmd != 0x01)
  {
    WARN_LOG_FMT(SERIALINTERFACE_AMBB, "SteeringWheel: Unknown command: {:02x}", cmd);
    return;
  }

  const u16 centering_force = Common::swap16(request.data() + 4);
  const u16 friction_force = Common::swap16(request.data() + 6);
  const u16 roll = Common::swap16(request.data() + 8);

  DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "SteeringWheel: FFB: {:04x} {:04x} {:04x}", centering_force,
                friction_force, roll);

  switch (m_init_state)
  {
  case 0:
    // The game seems to expect one 'E' response on power up.
    WriteTxBytes(std::array<u8, 3>{'E', '0', '0'});  // Error
    ++m_init_state;
    break;

  default:
    // The game won't send non-zero forces unless the '1' response is observed.
    // After a race, the game gradually lowers the forces down to 0 and expects a '6' response.
    // The significance of these '6' and '1' responses is not really understood.
    // Cycling between '6' and '1' seems to make the game happy for now..

    if (m_init_state == 1)
    {
      WriteTxBytes(std::array<u8, 3>{'C', '0', '6'});
      ++m_init_state;
    }
    else
    {
      WriteTxBytes(std::array<u8, 3>{'C', '0', '1'});
      m_init_state = 1;
    }
  }
}

void MarioKartGPSteeringWheel::Reset()
{
  INFO_LOG_FMT(SERIALINTERFACE_AMBB, "SteeringWheel: Reset");
  m_init_state = 0;
}

void MarioKartGPSteeringWheel::DoState(PointerWrap& p)
{
  p.Do(m_init_state);
}

}  // namespace Triforce
