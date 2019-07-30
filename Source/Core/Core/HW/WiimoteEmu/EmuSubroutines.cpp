// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <fstream>
#include <iterator>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControlGroup/ModifySettingsButton.h"

namespace WiimoteEmu
{
using namespace WiimoteCommon;

void Wiimote::HandleReportMode(const OutputReportMode& dr)
{
  if (!DataReportBuilder::IsValidMode(dr.mode))
  {
    // A real wiimote ignores the entire message if the mode is invalid.
    WARN_LOG(WIIMOTE, "Game requested invalid report mode: 0x%02x", int(dr.mode));
    return;
  }

  // TODO: A real wiimote sends a report immediately.
  // even on REPORT_CORE and continuous off when the buttons haven't changed.
  // But.. it is sent after the ACK

  // DEBUG_LOG(WIIMOTE, "Set data report mode");
  // DEBUG_LOG(WIIMOTE, "  Rumble: %x", dr->rumble);
  // DEBUG_LOG(WIIMOTE, "  Continuous: %x", dr->continuous);
  // DEBUG_LOG(WIIMOTE, "  Mode: 0x%02x", dr->mode);

  m_reporting_continuous = dr.continuous;
  m_reporting_mode = dr.mode;

  if (dr.ack)
    SendAck(OutputReportID::ReportMode, ErrorCode::Success);
}

// Tests that we have enough bytes for the report before we run the handler.
template <typename T, typename H>
void Wiimote::InvokeHandler(H&& handler, const WiimoteCommon::OutputReportGeneric& rpt, u32 size)
{
  if (size < sizeof(T))
  {
    ERROR_LOG(WIIMOTE, "InvokeHandler: report: 0x%02x invalid size: %d", int(rpt.rpt_id), size);
    return;
  }

  (this->*handler)(Common::BitCastPtr<T>(rpt.data));
}

// Here we process the Output Reports that the Wii sends. Our response will be
// an Input Report back to the Wii. Input and Output is from the Wii's
// perspective, Output means data to the Wiimote (from the Wii), Input means
// data from the Wiimote.
//
// The call browser:
//
// 1. Wiimote_InterruptChannel > InterruptChannel > HIDOutputReport
// 2. Wiimote_ControlChannel > ControlChannel > HIDOutputReport

void Wiimote::HIDOutputReport(const void* data, u32 size)
{
  if (!size)
  {
    ERROR_LOG(WIIMOTE, "HIDOutputReport: zero sized data");
    return;
  }

  auto& rpt = *static_cast<const OutputReportGeneric*>(data);
  const int rpt_size = size - OutputReportGeneric::HEADER_SIZE;

  DEBUG_LOG(WIIMOTE, "HIDOutputReport (page: %i, cid: 0x%02x, wm: 0x%02x)", m_index,
            m_reporting_channel, int(rpt.rpt_id));

  // WiiBrew:
  // In every single Output Report, bit 0 (0x01) of the first byte controls the Rumble feature.
  InvokeHandler<OutputReportRumble>(&Wiimote::HandleReportRumble, rpt, rpt_size);

  switch (rpt.rpt_id)
  {
  case OutputReportID::Rumble:
    // This is handled above.
    break;
  case OutputReportID::LED:
    InvokeHandler<OutputReportLeds>(&Wiimote::HandleReportLeds, rpt, rpt_size);
    break;
  case OutputReportID::ReportMode:
    InvokeHandler<OutputReportMode>(&Wiimote::HandleReportMode, rpt, rpt_size);
    break;
  case OutputReportID::IRLogicEnable:
    InvokeHandler<OutputReportEnableFeature>(&Wiimote::HandleIRLogicEnable, rpt, rpt_size);
    break;
  case OutputReportID::SpeakerEnable:
    InvokeHandler<OutputReportEnableFeature>(&Wiimote::HandleSpeakerEnable, rpt, rpt_size);
    break;
  case OutputReportID::RequestStatus:
    InvokeHandler<OutputReportRequestStatus>(&Wiimote::HandleRequestStatus, rpt, rpt_size);
    break;
  case OutputReportID::WriteData:
    InvokeHandler<OutputReportWriteData>(&Wiimote::HandleWriteData, rpt, rpt_size);
    break;
  case OutputReportID::ReadData:
    InvokeHandler<OutputReportReadData>(&Wiimote::HandleReadData, rpt, rpt_size);
    break;
  case OutputReportID::SpeakerData:
    InvokeHandler<OutputReportSpeakerData>(&Wiimote::HandleSpeakerData, rpt, rpt_size);
    break;
  case OutputReportID::SpeakerMute:
    InvokeHandler<OutputReportEnableFeature>(&Wiimote::HandleSpeakerMute, rpt, rpt_size);
    break;
  case OutputReportID::IRLogicEnable2:
    InvokeHandler<OutputReportEnableFeature>(&Wiimote::HandleIRLogicEnable2, rpt, rpt_size);
    break;
  default:
    PanicAlert("HidOutputReport: Unknown report ID 0x%02x", int(rpt.rpt_id));
    break;
  }
}

void Wiimote::CallbackInterruptChannel(const u8* data, u32 size)
{
  Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, data, size);
}

void Wiimote::SendAck(OutputReportID rpt_id, ErrorCode error_code)
{
  TypedHIDInputData<InputReportAck> rpt(InputReportID::Ack);
  auto& ack = rpt.data;

  ack.buttons = m_status.buttons;
  ack.rpt_id = rpt_id;
  ack.error_code = error_code;

  CallbackInterruptChannel(rpt.GetData(), rpt.GetSize());
}

void Wiimote::HandleExtensionSwap()
{
  if (WIIMOTE_BALANCE_BOARD == m_index)
  {
    // Prevent M+ or anything else silly from being attached to a balance board.
    // In the future if we support an emulated balance board we can force the BB "extension" here.
    return;
  }

  ExtensionNumber desired_extension_number =
      static_cast<ExtensionNumber>(m_attachments->GetSelectedAttachment());

  const bool desired_motion_plus = m_motion_plus_setting.GetValue();

  // FYI: AttachExtension also connects devices to the i2c bus

  if (m_is_motion_plus_attached && !desired_motion_plus)
  {
    // M+ is attached and it's not wanted, so remove it.
    m_extension_port.AttachExtension(GetNoneExtension());
    m_is_motion_plus_attached = false;

    // Also remove extension (if any) from the M+'s ext port.
    m_active_extension = ExtensionNumber::NONE;
    m_motion_plus.GetExtPort().AttachExtension(GetNoneExtension());

    // Don't do anything else this update cycle.
    return;
  }

  if (desired_motion_plus && !m_is_motion_plus_attached)
  {
    // M+ is wanted and it's not attached

    if (GetActiveExtensionNumber() != ExtensionNumber::NONE)
    {
      // But an extension is attached. Remove it first.
      // (handled below)
      desired_extension_number = ExtensionNumber::NONE;
    }
    else
    {
      // No extension attached so attach M+.
      m_is_motion_plus_attached = true;
      m_extension_port.AttachExtension(&m_motion_plus);
      m_motion_plus.Reset();

      // Also attach extension below if desired:
    }
  }

  if (GetActiveExtensionNumber() != desired_extension_number)
  {
    // A different extension is wanted (either by user or by the M+ logic above)
    if (GetActiveExtensionNumber() != ExtensionNumber::NONE)
    {
      // First we must detach the current extension.
      // The next call will change to the new extension if needed.
      m_active_extension = ExtensionNumber::NONE;
    }
    else
    {
      m_active_extension = desired_extension_number;
    }

    if (m_is_motion_plus_attached)
    {
      // M+ is attached so attach to it.
      m_motion_plus.GetExtPort().AttachExtension(GetActiveExtension());
    }
    else
    {
      // M+ is not attached so attach directly.
      m_extension_port.AttachExtension(GetActiveExtension());
    }

    GetActiveExtension()->Reset();
  }
}

void Wiimote::HandleRequestStatus(const OutputReportRequestStatus&)
{
  // FYI: buttons are updated in Update() for determinism

  // Update status struct
  m_status.extension = m_extension_port.IsDeviceConnected();

  // Based on testing, old WiiLi.org docs, and WiiUse library:
  // Max battery level seems to be 0xc8 (decimal 200)
  constexpr u8 MAX_BATTERY_LEVEL = 0xc8;

  m_status.battery = u8(std::lround(m_battery_setting.GetValue() / 100 * MAX_BATTERY_LEVEL));

  if (Core::WantsDeterminism())
  {
    // One less thing to break determinism:
    m_status.battery = MAX_BATTERY_LEVEL;
  }

  // Less than 0x20 triggers the low-battery flag:
  m_status.battery_low = m_status.battery < 0x20;

  TypedHIDInputData<InputReportStatus> rpt(InputReportID::Status);
  rpt.data = m_status;
  CallbackInterruptChannel(rpt.GetData(), rpt.GetSize());
}

void Wiimote::HandleWriteData(const OutputReportWriteData& wd)
{
  // TODO: Are writes ignored during an active read request?

  u16 address = Common::swap16(wd.address);

  DEBUG_LOG(WIIMOTE, "Wiimote::WriteData: 0x%02x @ 0x%02x @ 0x%02x (%d)", wd.space,
            wd.slave_address, address, wd.size);

  if (0 == wd.size || wd.size > 16)
  {
    WARN_LOG(WIIMOTE, "WriteData: invalid size: %d", wd.size);
    // A real wiimote silently ignores such a request:
    return;
  }

  ErrorCode error_code = ErrorCode::Success;

  switch (static_cast<AddressSpace>(wd.space))
  {
  case AddressSpace::EEPROM:
  {
    if (address + wd.size > EEPROM_FREE_SIZE)
    {
      WARN_LOG(WIIMOTE, "WriteData: address + size out of bounds!");
      error_code = ErrorCode::InvalidAddress;
    }
    else
    {
      std::copy_n(wd.data, wd.size, m_eeprom.data.data() + address);

      // Write mii data to file
      if (address >= 0x0FCA && address < 0x12C0)
      {
        // TODO: Only write parts of the Mii block.
        // TODO: Use different files for different wiimote numbers.
        std::ofstream file;
        File::OpenFStream(file, File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/mii.bin",
                          std::ios::binary | std::ios::out);
        file.write((char*)m_eeprom.data.data() + 0x0FCA, 0x02f0);
        file.close();
      }
    }
  }
  break;

  case AddressSpace::I2CBus:
  case AddressSpace::I2CBusAlt:
  {
    // Attempting to access the EEPROM directly over i2c results in error 8.
    if (EEPROM_I2C_ADDR == m_read_request.slave_address)
    {
      WARN_LOG(WIIMOTE, "Attempt to write EEPROM directly.");
      error_code = ErrorCode::InvalidAddress;
      break;
    }

    // Top byte of address is ignored on the bus.
    auto const bytes_written = m_i2c_bus.BusWrite(wd.slave_address, (u8)address, wd.size, wd.data);
    if (bytes_written != wd.size)
    {
      // A real wiimote gives error 7 for failed write to i2c bus (mainly a non-existant slave)
      error_code = ErrorCode::Nack;
    }
  }
  break;

  default:
    WARN_LOG(WIIMOTE, "WriteData: invalid address space: 0x%x", wd.space);
    // A real wiimote gives error 6:
    error_code = ErrorCode::InvalidSpace;
    break;
  }

  // Real wiimotes seem to always ACK data writes.
  SendAck(OutputReportID::WriteData, error_code);
}

void Wiimote::HandleReportRumble(const WiimoteCommon::OutputReportRumble& rpt)
{
  SetRumble(rpt.rumble);

  // FYI: A real wiimote never seems to ACK a rumble report:
}

void Wiimote::HandleReportLeds(const WiimoteCommon::OutputReportLeds& rpt)
{
  m_status.leds = rpt.leds;

  if (rpt.ack)
    SendAck(OutputReportID::LED, ErrorCode::Success);
}

void Wiimote::HandleIRLogicEnable2(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  // FYI: We ignore this and update camera data regardless.

  if (rpt.ack)
    SendAck(OutputReportID::IRLogicEnable2, ErrorCode::Success);
}

void Wiimote::HandleIRLogicEnable(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  // Note: Wiibrew currently refers to this report (0x13) as "Enable IR Pixel Clock"
  // however my testing shows this affects the relevant status bit and whether or not
  // the camera responds on the I2C bus.

  m_status.ir = rpt.enable;

  m_camera_logic.SetEnabled(m_status.ir);

  if (rpt.ack)
    SendAck(OutputReportID::IRLogicEnable, ErrorCode::Success);
}

void Wiimote::HandleSpeakerMute(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  m_speaker_mute = rpt.enable;

  if (rpt.ack)
    SendAck(OutputReportID::SpeakerMute, ErrorCode::Success);
}

void Wiimote::HandleSpeakerEnable(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  m_status.speaker = rpt.enable;

  if (rpt.ack)
    SendAck(OutputReportID::SpeakerEnable, ErrorCode::Success);
}

void Wiimote::HandleSpeakerData(const WiimoteCommon::OutputReportSpeakerData& rpt)
{
  // TODO: Does speaker_mute stop speaker data processing?
  // and what about speaker_enable?
  // (important to keep decoder in proper state)
  if (!m_speaker_mute)
  {
    if (rpt.length > std::size(rpt.data))
    {
      ERROR_LOG(WIIMOTE, "Bad speaker data length: %d", rpt.length);
    }
    else
    {
      // Speaker Pan
      const auto pan = m_speaker_pan_setting.GetValue() / 100;

      m_speaker_logic.SpeakerData(rpt.data, rpt.length, pan);
    }
  }

  // FYI: Speaker data reports normally do not ACK but I have seen them ACK with error codes
  // It seems some wiimotes do this when receiving data too quickly.
  // More investigation is needed.
}

void Wiimote::HandleReadData(const OutputReportReadData& rd)
{
  if (m_read_request.size)
  {
    // There is already an active read request.
    // A real wiimote ignores the new one.
    WARN_LOG(WIIMOTE, "ReadData: ignoring read during active request.");
    return;
  }

  // Save the request and process it on the next "Update()" call(s)
  m_read_request.space = static_cast<AddressSpace>(rd.space);
  m_read_request.slave_address = rd.slave_address;
  m_read_request.address = Common::swap16(rd.address);
  // A zero size request is just ignored, like on the real wiimote.
  m_read_request.size = Common::swap16(rd.size);

  DEBUG_LOG(WIIMOTE, "Wiimote::ReadData: %d @ 0x%02x @ 0x%02x (%d)", int(m_read_request.space),
            m_read_request.slave_address, m_read_request.address, m_read_request.size);

  // Send up to one read-data-reply.
  // If more data needs to be sent it will happen on the next "Update()"
  // TODO: should this be removed and let Update() take care of it?
  ProcessReadDataRequest();

  // FYI: No "ACK" is sent.
}

bool Wiimote::ProcessReadDataRequest()
{
  // Limit the amt to 16 bytes
  // AyuanX: the MTU is 640B though... what a waste!
  const u16 bytes_to_read = std::min<u16>(16, m_read_request.size);

  if (0 == bytes_to_read)
  {
    // No active request:
    return false;
  }

  TypedHIDInputData<InputReportReadDataReply> rpt(InputReportID::ReadDataReply);
  auto& reply = rpt.data;

  reply.buttons = m_status.buttons;
  reply.address = Common::swap16(m_read_request.address);

  // Pre-fill with zeros in case of read-error or read < 16-bytes:
  std::fill(std::begin(reply.data), std::end(reply.data), 0x00);

  ErrorCode error_code = ErrorCode::Success;

  switch (m_read_request.space)
  {
  case AddressSpace::EEPROM:
  {
    if (m_read_request.address + m_read_request.size > EEPROM_FREE_SIZE)
    {
      // Generate a read error. Even if the start of the block is readable a real wiimote just sends
      // error code 8

      // The real Wiimote generate an error for the first
      // request to 0x1770 if we dont't replicate that the game will never
      // read the calibration data at the beginning of Eeprom.
      error_code = ErrorCode::InvalidAddress;
    }
    else
    {
      // Mii block handling:
      // TODO: different filename for each wiimote?
      if (m_read_request.address >= 0x0FCA && m_read_request.address < 0x12C0)
      {
        // TODO: Only read the Mii block parts required
        std::ifstream file;
        File::OpenFStream(file, (File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/mii.bin").c_str(),
                          std::ios::binary | std::ios::in);
        file.read((char*)m_eeprom.data.data() + 0x0FCA, 0x02f0);
        file.close();
      }

      // Read memory to be sent to Wii
      std::copy_n(m_eeprom.data.data() + m_read_request.address, bytes_to_read, reply.data);
      reply.size_minus_one = bytes_to_read - 1;
    }
  }
  break;

  case AddressSpace::I2CBus:
  case AddressSpace::I2CBusAlt:
  {
    // Attempting to access the EEPROM directly over i2c results in error 8.
    if (EEPROM_I2C_ADDR == m_read_request.slave_address)
    {
      WARN_LOG(WIIMOTE, "Attempt to read EEPROM directly.");
      error_code = ErrorCode::InvalidAddress;
      break;
    }

    // Top byte of address is ignored on the bus, but it IS maintained in the read-reply.
    auto const bytes_read = m_i2c_bus.BusRead(
        m_read_request.slave_address, (u8)m_read_request.address, bytes_to_read, reply.data);

    if (bytes_read != bytes_to_read)
    {
      DEBUG_LOG(WIIMOTE, "Responding with read error 7 @ 0x%x @ 0x%x (%d)",
                m_read_request.slave_address, m_read_request.address, m_read_request.size);
      error_code = ErrorCode::Nack;
      break;
    }

    reply.size_minus_one = bytes_read - 1;
  }
  break;

  default:
    WARN_LOG(WIIMOTE, "ReadData: invalid address space: 0x%x", int(m_read_request.space));
    // A real wiimote gives error 6:
    error_code = ErrorCode::InvalidSpace;
    break;
  }

  if (ErrorCode::Success != error_code)
  {
    // Stop processing request on read error:
    m_read_request.size = 0;
    // Real wiimote seems to set size to max value on read errors:
    reply.size_minus_one = 0xf;
  }
  else
  {
    // Modify the active read request, zero size == complete
    m_read_request.address += bytes_to_read;
    m_read_request.size -= bytes_to_read;
  }

  reply.error = static_cast<u8>(error_code);

  CallbackInterruptChannel(rpt.GetData(), rpt.GetSize());

  return true;
}

void Wiimote::DoState(PointerWrap& p)
{
  // No need to sync. Index will not change.
  // p.Do(m_index);

  // No need to sync. This is not wiimote state.
  // p.Do(m_sensor_bar_on_top);

  p.Do(m_reporting_channel);
  p.Do(m_reporting_mode);
  p.Do(m_reporting_continuous);

  p.Do(m_speaker_mute);

  p.Do(m_status);
  p.Do(m_eeprom);
  p.Do(m_read_request);

  // Sub-devices:
  m_speaker_logic.DoState(p);
  m_camera_logic.DoState(p);

  if (p.GetMode() == PointerWrap::MODE_READ)
    m_camera_logic.SetEnabled(m_status.ir);

  p.Do(m_is_motion_plus_attached);
  p.Do(m_active_extension);

  // Attach M+/Extensions.
  m_extension_port.AttachExtension(m_is_motion_plus_attached ? &m_motion_plus : GetNoneExtension());
  (m_is_motion_plus_attached ? m_motion_plus.GetExtPort() : m_extension_port)
      .AttachExtension(GetActiveExtension());

  if (m_is_motion_plus_attached)
    m_motion_plus.DoState(p);

  if (m_active_extension != ExtensionNumber::NONE)
    GetActiveExtension()->DoState(p);

  // Dynamics
  p.Do(m_swing_state);
  p.Do(m_tilt_state);
  p.Do(m_cursor_state);
  p.Do(m_shake_state);

  p.DoMarker("Wiimote");
}

ExtensionNumber Wiimote::GetActiveExtensionNumber() const
{
  return m_active_extension;
}

}  // namespace WiimoteEmu
