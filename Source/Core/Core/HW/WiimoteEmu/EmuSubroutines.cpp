// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include <cmath>
#include <fstream>
#include <iterator>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/EnumUtils.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"

namespace WiimoteEmu
{
using namespace WiimoteCommon;

void WiimoteBase::HandleReportMode(const OutputReportMode& dr)
{
  if (!DataReportBuilder::IsValidMode(dr.mode))
  {
    // A real wiimote ignores the entire message if the mode is invalid.
    WARN_LOG_FMT(WIIMOTE, "Game requested invalid report mode: {:#04x}", static_cast<u8>(dr.mode));
    return;
  }

  // TODO: A real wiimote sends a report immediately.
  // even on REPORT_CORE and continuous off when the buttons haven't changed.
  // But.. it is sent after the ACK

  m_reporting_continuous = dr.continuous;
  m_reporting_mode = dr.mode;

  if (dr.ack)
    SendAck(OutputReportID::ReportMode, ErrorCode::Success);
}

// Tests that we have enough bytes for the report before we run the handler.
template <typename T, typename H>
void WiimoteBase::InvokeHandler(H&& handler, const WiimoteCommon::OutputReportGeneric& rpt,
                                u32 size)
{
  if (size < sizeof(T))
  {
    ERROR_LOG_FMT(WIIMOTE, "InvokeHandler: report: {:#04x} invalid size: {}",
                  static_cast<u8>(rpt.rpt_id), size);
    return;
  }

  (this->*handler)(Common::BitCastPtr<T>(&rpt.data[0]));
}

void WiimoteBase::EventLinked()
{
  Reset();
}

void WiimoteBase::EventUnlinked()
{
  Reset();
}

void WiimoteBase::InterruptDataOutput(const u8* data, u32 size)
{
  if (size == 0)
  {
    ERROR_LOG_FMT(WIIMOTE, "OutputData: zero sized data");
    return;
  }

  const auto& rpt = *reinterpret_cast<const OutputReportGeneric*>(data);
  const int rpt_size = size - OutputReportGeneric::HEADER_SIZE;

  if (rpt_size == 0)
  {
    ERROR_LOG_FMT(WIIMOTE, "OutputData: zero sized report");
    return;
  }

  // WiiBrew:
  // In every single Output Report, bit 0 (0x01) of the first byte controls the Rumble feature.
  InvokeHandler<OutputReportRumble>(&WiimoteBase::HandleReportRumble, rpt, rpt_size);

  switch (rpt.rpt_id)
  {
  case OutputReportID::Rumble:
    // This is handled above.
    break;
  case OutputReportID::LED:
    InvokeHandler<OutputReportLeds>(&WiimoteBase::HandleReportLeds, rpt, rpt_size);
    break;
  case OutputReportID::ReportMode:
    InvokeHandler<OutputReportMode>(&WiimoteBase::HandleReportMode, rpt, rpt_size);
    break;
  case OutputReportID::IRLogicEnable:
    InvokeHandler<OutputReportEnableFeature>(&WiimoteBase::HandleIRLogicEnable, rpt, rpt_size);
    break;
  case OutputReportID::SpeakerEnable:
    InvokeHandler<OutputReportEnableFeature>(&WiimoteBase::HandleSpeakerEnable, rpt, rpt_size);
    break;
  case OutputReportID::RequestStatus:
    InvokeHandler<OutputReportRequestStatus>(&WiimoteBase::HandleRequestStatus, rpt, rpt_size);
    break;
  case OutputReportID::WriteData:
    InvokeHandler<OutputReportWriteData>(&WiimoteBase::HandleWriteData, rpt, rpt_size);
    break;
  case OutputReportID::ReadData:
    InvokeHandler<OutputReportReadData>(&WiimoteBase::HandleReadData, rpt, rpt_size);
    break;
  case OutputReportID::SpeakerData:
    InvokeHandler<OutputReportSpeakerData>(&WiimoteBase::HandleSpeakerData, rpt, rpt_size);
    break;
  case OutputReportID::SpeakerMute:
    InvokeHandler<OutputReportEnableFeature>(&WiimoteBase::HandleSpeakerMute, rpt, rpt_size);
    break;
  case OutputReportID::IRLogicEnable2:
    InvokeHandler<OutputReportEnableFeature>(&WiimoteBase::HandleIRLogicEnable2, rpt, rpt_size);
    break;
  default:
    PanicAlertFmt("HidOutputReport: Unknown report ID {:#04x}", static_cast<u8>(rpt.rpt_id));
    break;
  }
}

void WiimoteBase::SendAck(OutputReportID rpt_id, ErrorCode error_code)
{
  TypedInputData<InputReportAck> rpt(InputReportID::Ack);
  auto& ack = rpt.payload;

  ack.buttons = m_status.buttons;
  ack.rpt_id = rpt_id;
  ack.error_code = error_code;

  InterruptDataInputCallback(rpt.GetData(), rpt.GetSize());
}

void Wiimote::HandleExtensionSwap(ExtensionNumber desired_extension_number,
                                  bool desired_motion_plus)
{
  // FYI: AttachExtension also connects devices to the i2c bus

  if (m_is_motion_plus_attached && !desired_motion_plus)
  {
    INFO_LOG_FMT(WIIMOTE, "Detaching Motion Plus (Wiimote {} in slot {})", m_index,
                 m_bt_device_index);

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
      INFO_LOG_FMT(WIIMOTE, "Attaching Motion Plus (Wiimote {} in slot {})", m_index,
                   m_bt_device_index);

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
      INFO_LOG_FMT(WIIMOTE, "Detaching Extension (Wiimote {} in slot {})", m_index,
                   m_bt_device_index);

      // First we must detach the current extension.
      // The next call will change to the new extension if needed.
      m_active_extension = ExtensionNumber::NONE;
    }
    else
    {
      INFO_LOG_FMT(WIIMOTE, "Switching to Extension {} (Wiimote {} in slot {})",
                   Common::ToUnderlying(desired_extension_number), m_index, m_bt_device_index);

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

void WiimoteBase::HandleRequestStatus(const OutputReportRequestStatus&)
{
  // FYI: buttons are updated in Update() for determinism

  // Update status struct
  m_status.extension = m_extension_port.IsDeviceConnected();
  m_status.SetEstimatedCharge(m_battery_setting.GetValue() / ciface::BATTERY_INPUT_MAX_VALUE);

  if (Core::WantsDeterminism())
  {
    // One less thing to break determinism:
    m_status.SetEstimatedCharge(1.f);
  }

  // Less than 0x20 triggers the low-battery flag:
  m_status.battery_low = m_status.battery < 0x20;

  TypedInputData<InputReportStatus> rpt(InputReportID::Status);
  rpt.payload = m_status;
  InterruptDataInputCallback(rpt.GetData(), rpt.GetSize());
}

void WiimoteBase::HandleWriteData(const OutputReportWriteData& wd)
{
  if (m_read_request.size)
  {
    // FYI: Writes during an active read will occasionally produce a "busy" (0x4) ack.
    // We won't simulate that as it often does work. Poorly programmed games may rely on it.
    WARN_LOG_FMT(WIIMOTE, "WriteData: write during active read request.");
  }

  const u16 address = Common::swap16(wd.address);

  DEBUG_LOG_FMT(WIIMOTE, "Wiimote::WriteData: {:#04x} @ {:#04x} @ {:#04x} ({})", wd.space,
                wd.slave_address, address, wd.size);

  if (0 == wd.size || wd.size > 16)
  {
    WARN_LOG_FMT(WIIMOTE, "WriteData: invalid size: {}", wd.size);
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
      WARN_LOG_FMT(WIIMOTE, "WriteData: address + size out of bounds!");
      error_code = ErrorCode::InvalidAddress;
    }
    else
    {
      std::copy_n(wd.data, wd.size, m_eeprom.data.data() + address);
      m_eeprom_dirty = true;
    }
  }
  break;

  case AddressSpace::I2CBus:
  case AddressSpace::I2CBusAlt:
  {
    // Attempting to access the EEPROM directly over i2c results in error 8.
    if (EEPROM_I2C_ADDR == m_read_request.slave_address)
    {
      WARN_LOG_FMT(WIIMOTE, "Attempt to write EEPROM directly.");
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
    WARN_LOG_FMT(WIIMOTE, "WriteData: invalid address space: {:#x}", wd.space);
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

void WiimoteBase::HandleReportLeds(const WiimoteCommon::OutputReportLeds& rpt)
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
      ERROR_LOG_FMT(WIIMOTE, "Bad speaker data length: {}", rpt.length);
    }
    else
    {
      // Speaker data reports result in a write to the speaker hardware at offset 0x00.
      m_i2c_bus.BusWrite(SpeakerLogic::I2C_ADDR, SpeakerLogic::SPEAKER_DATA_OFFSET, rpt.length,
                         rpt.data);
    }
  }

  // FYI: Speaker data reports normally do not ACK but I have seen them ACK with error codes
  // It seems some wiimotes do this when receiving data too quickly.
  // More investigation is needed.
}

void WiimoteBase::HandleReadData(const OutputReportReadData& rd)
{
  if (m_read_request.size)
  {
    // There is already an active read being processed.
    WARN_LOG_FMT(WIIMOTE, "ReadData: attempting read during active request.");

    // A real wm+ sends a busy ack in this situation.
    SendAck(OutputReportID::ReadData, ErrorCode::Busy);
    return;
  }

  // Save the request and process it on the next "Update()" call(s)
  m_read_request.space = static_cast<AddressSpace>(rd.space);
  m_read_request.slave_address = rd.slave_address;
  m_read_request.address = Common::swap16(rd.address);
  // A zero size request is just ignored, like on the real wiimote.
  m_read_request.size = Common::swap16(rd.size);

  DEBUG_LOG_FMT(WIIMOTE, "Wiimote::ReadData: {} @ {:#04x} @ {:#04x} ({})",
                static_cast<u8>(m_read_request.space), m_read_request.slave_address,
                m_read_request.address, m_read_request.size);

  // Send up to one read-data-reply.
  // If more data needs to be sent it will happen on the next "Update()"
  // TODO: should this be removed and let Update() take care of it?
  ProcessReadDataRequest();

  // FYI: No "ACK" is sent under normal situations.
}

bool WiimoteBase::ProcessReadDataRequest()
{
  // Limit the amt to 16 bytes
  // AyuanX: the MTU is 640B though... what a waste!
  const u16 bytes_to_read = std::min<u16>(16, m_read_request.size);

  if (0 == bytes_to_read)
  {
    // No active request:
    return false;
  }

  TypedInputData<InputReportReadDataReply> rpt(InputReportID::ReadDataReply);
  auto& reply = rpt.payload;

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
      WARN_LOG_FMT(WIIMOTE, "Attempt to read EEPROM directly.");
      error_code = ErrorCode::InvalidAddress;
      break;
    }

    // It is possible to bypass data reporting and directly read extension input.
    // While I am not aware of any games that actually do this,
    // our NetPlay and TAS methods are completely unprepared for it.
    const bool is_reading_ext = EncryptedExtension::I2C_ADDR == m_read_request.slave_address &&
                                m_read_request.address < EncryptedExtension::CONTROLLER_DATA_BYTES;
    const bool is_reading_ir =
        CameraLogic::I2C_ADDR == m_read_request.slave_address &&
        m_read_request.address < CameraLogic::REPORT_DATA_OFFSET + CameraLogic::CAMERA_DATA_BYTES &&
        m_read_request.address + m_read_request.size > CameraLogic::REPORT_DATA_OFFSET;

    if (is_reading_ext || is_reading_ir)
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::DIRECTLY_READS_WIIMOTE_INPUT);

    // Top byte of address is ignored on the bus, but it IS maintained in the read-reply.
    auto const bytes_read = m_i2c_bus.BusRead(
        m_read_request.slave_address, (u8)m_read_request.address, bytes_to_read, reply.data);

    if (bytes_read != bytes_to_read)
    {
      DEBUG_LOG_FMT(WIIMOTE, "Responding with read error 7 @ {:#x} @ {:#x} ({})",
                    m_read_request.slave_address, m_read_request.address, m_read_request.size);
      error_code = ErrorCode::Nack;
      break;
    }

    reply.size_minus_one = bytes_read - 1;
  }
  break;

  default:
    WARN_LOG_FMT(WIIMOTE, "ReadData: invalid address space: {:#x}", int(m_read_request.space));
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

  InterruptDataInputCallback(rpt.GetData(), rpt.GetSize());

  return true;
}

void WiimoteBase::DoState(PointerWrap& p)
{
  // No need to sync. Index will not change.
  // p.Do(m_index);

  p.Do(m_reporting_mode);
  p.Do(m_reporting_continuous);

  p.Do(m_status);
  p.Do(m_eeprom);
  p.Do(m_read_request);

  p.DoMarker("WiimoteBase");
}

void Wiimote::DoState(PointerWrap& p)
{
  WiimoteBase::DoState(p);

  p.Do(m_speaker_mute);

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

  // Sub-devices:
  m_speaker_logic.DoState(p);
  m_camera_logic.DoState(p);

  if (p.IsReadMode())
    m_camera_logic.SetEnabled(m_status.ir);

  // Dynamics
  p.Do(m_swing_state);
  p.Do(m_tilt_state);
  p.Do(m_point_state);
  p.Do(m_shake_state);

  // We'll consider the IMU state part of the user's physical controller state and not sync it.
  // (m_imu_cursor_state)

  p.DoMarker("Wiimote");
}

void BalanceBoard::DoState(PointerWrap& p)
{
  WiimoteBase::DoState(p);
  m_ext.DoState(p);
  p.DoMarker("BalanceBoard");
}

// TODO: Are these implemented correctly?  How does the balance board actually handle missing
// hardware?
// Seems like using Nack causes things not to work, so Success probably is correct.
void BalanceBoard::HandleReportRumble(const WiimoteCommon::OutputReportRumble& rpt)
{
  // rpt.ack does not exist ("A real wiimote never seems to ACK a rumble report")
}
void BalanceBoard::HandleIRLogicEnable(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  // Not called?
  if (rpt.ack)
    SendAck(OutputReportID::IRLogicEnable, ErrorCode::Success);
}
void BalanceBoard::HandleIRLogicEnable2(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  if (rpt.ack)
    SendAck(OutputReportID::IRLogicEnable2, ErrorCode::Success);
}
void BalanceBoard::HandleSpeakerMute(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  if (rpt.ack)
    SendAck(OutputReportID::SpeakerMute, ErrorCode::Success);
}
void BalanceBoard::HandleSpeakerEnable(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  if (rpt.ack)
    SendAck(OutputReportID::SpeakerEnable, ErrorCode::Success);
}
void BalanceBoard::HandleSpeakerData(const WiimoteCommon::OutputReportSpeakerData& rpt)
{
  // rpt.ack does not exist
  // ("Speaker data reports normally do not ACK but I have seen them ACK with error codes")
}

bool Wiimote::IsMotionPlusAttached() const
{
  return m_is_motion_plus_attached;
}

}  // namespace WiimoteEmu
