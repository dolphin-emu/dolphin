// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>

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

  SendAck(OutputReportID::REPORT_MODE, ErrorCode::SUCCESS);
}

// Tests that we have enough bytes for the report before we run the handler.
template <typename T, typename H>
void Wiimote::InvokeHandler(H&& handler, const WiimoteCommon::OutputReportGeneric& rpt, u32 size)
{
  if (size < sizeof(T))
  {
    ERROR_LOG(WIIMOTE, "InvokeHandler: report: 0x%02x invalid size: %d", int(rpt.rpt_id), size);
  }

  (this->*handler)(*reinterpret_cast<const T*>(rpt.data));
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
  const int rpt_size = size - rpt.HEADER_SIZE;

  DEBUG_LOG(WIIMOTE, "HIDOutputReport (page: %i, cid: 0x%02x, wm: 0x%02x)", m_index,
            m_reporting_channel, int(rpt.rpt_id));

  // WiiBrew:
  // In every single Output Report, bit 0 (0x01) of the first byte controls the Rumble feature.
  InvokeHandler<OutputReportRumble>(&Wiimote::HandleReportRumble, rpt, rpt_size);

  switch (rpt.rpt_id)
  {
  case OutputReportID::RUMBLE:
    // This is handled above.
    break;
  case OutputReportID::LEDS:
    InvokeHandler<OutputReportLeds>(&Wiimote::HandleReportLeds, rpt, rpt_size);
    break;
  case OutputReportID::REPORT_MODE:
    InvokeHandler<OutputReportMode>(&Wiimote::HandleReportMode, rpt, rpt_size);
    break;
  case OutputReportID::IR_PIXEL_CLOCK:
    InvokeHandler<OutputReportEnableFeature>(&Wiimote::HandleIRPixelClock, rpt, rpt_size);
    break;
  case OutputReportID::SPEAKER_ENABLE:
    InvokeHandler<OutputReportEnableFeature>(&Wiimote::HandleSpeakerEnable, rpt, rpt_size);
    break;
  case OutputReportID::REQUEST_STATUS:
    InvokeHandler<OutputReportRequestStatus>(&Wiimote::HandleRequestStatus, rpt, rpt_size);
    break;
  case OutputReportID::WRITE_DATA:
    InvokeHandler<OutputReportWriteData>(&Wiimote::HandleWriteData, rpt, rpt_size);
    break;
  case OutputReportID::READ_DATA:
    InvokeHandler<OutputReportReadData>(&Wiimote::HandleReadData, rpt, rpt_size);
    break;
  case OutputReportID::SPEAKER_DATA:
    InvokeHandler<OutputReportSpeakerData>(&Wiimote::HandleSpeakerData, rpt, rpt_size);
    break;
  case OutputReportID::SPEAKER_MUTE:
    InvokeHandler<OutputReportEnableFeature>(&Wiimote::HandleSpeakerMute, rpt, rpt_size);
    break;
  case OutputReportID::IR_LOGIC:
    InvokeHandler<OutputReportEnableFeature>(&Wiimote::HandleIRLogic, rpt, rpt_size);
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
  TypedHIDInputData<InputReportAck> rpt(InputReportID::ACK);
  auto& ack = rpt.data;

  ack.buttons = m_status.buttons;
  ack.rpt_id = rpt_id;
  ack.error_code = error_code;

  CallbackInterruptChannel(rpt.GetData(), rpt.GetSize());
}

void Wiimote::HandleExtensionSwap()
{
  const ExtensionNumber desired_extension =
      static_cast<ExtensionNumber>(m_attachments->GetSelectedAttachment());

  if (GetActiveExtensionNumber() != desired_extension)
  {
    if (GetActiveExtensionNumber())
    {
      // First we must detach the current extension.
      // The next call will change to the new extension if needed.
      m_active_extension = ExtensionNumber::NONE;
    }
    else
    {
      m_active_extension = desired_extension;
    }

    // TODO: Attach directly when not using M+.
    m_motion_plus.AttachExtension(GetActiveExtension());
    GetActiveExtension()->Reset();
  }
}

void Wiimote::HandleRequestStatus(const OutputReportRequestStatus&)
{
  // FYI: buttons are updated in Update() for determinism

  // Update status struct
  m_status.extension = m_extension_port.IsDeviceConnected();

  // TODO: Battery level will break determinism in TAS/Netplay

  // Battery levels in voltage
  //   0x00 - 0x32: level 1
  //   0x33 - 0x43: level 2
  //   0x33 - 0x54: level 3
  //   0x55 - 0xff: level 4
  m_status.battery = (u8)(m_battery_setting->GetValue() * 0xff);
  // Less than 0x20 triggers the low-battery flag:
  m_status.battery_low = m_status.battery < 0x20;

  TypedHIDInputData<InputReportStatus> rpt(InputReportID::STATUS);
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

  ErrorCode error_code = ErrorCode::SUCCESS;

  switch (static_cast<AddressSpace>(wd.space))
  {
  case AddressSpace::EEPROM:
  {
    if (address + wd.size > EEPROM_FREE_SIZE)
    {
      WARN_LOG(WIIMOTE, "WriteData: address + size out of bounds!");
      error_code = ErrorCode::INVALID_ADDRESS;
    }
    else
    {
      std::copy_n(wd.data, wd.size, m_eeprom.data.data() + address);

      // Write mii data to file
      if (address >= 0x0FCA && address < 0x12C0)
      {
        // TODO: Only write parts of the Mii block
        std::ofstream file;
        File::OpenFStream(file, File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/mii.bin",
                          std::ios::binary | std::ios::out);
        file.write((char*)m_eeprom.data.data() + 0x0FCA, 0x02f0);
        file.close();
      }
    }
  }
  break;

  case AddressSpace::I2C_BUS:
  case AddressSpace::I2C_BUS_ALT:
  {
    // Attempting to access the EEPROM directly over i2c results in error 8.
    if (EEPROM_I2C_ADDR == m_read_request.slave_address)
    {
      WARN_LOG(WIIMOTE, "Attempt to write EEPROM directly.");
      error_code = ErrorCode::INVALID_ADDRESS;
      break;
    }

    // Top byte of address is ignored on the bus.
    auto const bytes_written = m_i2c_bus.BusWrite(wd.slave_address, (u8)address, wd.size, wd.data);
    if (bytes_written != wd.size)
    {
      // A real wiimote gives error 7 for failed write to i2c bus (mainly a non-existant slave)
      error_code = ErrorCode::NACK;
    }
  }
  break;

  default:
    WARN_LOG(WIIMOTE, "WriteData: invalid address space: 0x%x", wd.space);
    // A real wiimote gives error 6:
    error_code = ErrorCode::INVALID_SPACE;
    break;
  }

  SendAck(OutputReportID::WRITE_DATA, error_code);
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
    SendAck(OutputReportID::LEDS, ErrorCode::SUCCESS);
}

void Wiimote::HandleIRPixelClock(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  // INFO_LOG(WIIMOTE, "WM IR Clock: %02x", erpt.enable);

  // FYI: Camera data is currently always updated. Ignoring pixel clock status.

  if (rpt.ack)
    SendAck(OutputReportID::IR_PIXEL_CLOCK, ErrorCode::SUCCESS);
}

void Wiimote::HandleIRLogic(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  // FYI: Camera data is currently always updated. We just save this for status reports.

  m_status.ir = rpt.enable;

  if (rpt.ack)
    SendAck(OutputReportID::IR_LOGIC, ErrorCode::SUCCESS);
}

void Wiimote::HandleSpeakerMute(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  m_speaker_mute = rpt.enable;

  if (rpt.ack)
    SendAck(OutputReportID::SPEAKER_MUTE, ErrorCode::SUCCESS);
}

void Wiimote::HandleSpeakerEnable(const WiimoteCommon::OutputReportEnableFeature& rpt)
{
  // INFO_LOG(WIIMOTE, "WM Speaker Enable: %02x", erpt.enable);
  m_status.speaker = rpt.enable;

  if (rpt.ack)
    SendAck(OutputReportID::SPEAKER_ENABLE, ErrorCode::SUCCESS);
}

void Wiimote::HandleSpeakerData(const WiimoteCommon::OutputReportSpeakerData& rpt)
{
  // TODO: Does speaker_mute stop speaker data processing?
  // and what about speaker_enable?
  // (important to keep decoder in proper state)
  if (!m_speaker_mute)
  {
    if (rpt.length > ArraySize(rpt.data))
    {
      ERROR_LOG(WIIMOTE, "Bad speaker data length: %d", rpt.length);
    }
    else
    {
      // Speaker Pan
      // GUI clamps pan setting from -127 to 127. Why?
      const auto pan = int(m_options->numeric_settings[0]->GetValue() * 100);

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

  TypedHIDInputData<InputReportReadDataReply> rpt(InputReportID::READ_DATA_REPLY);
  auto& reply = rpt.data;

  reply.buttons = m_status.buttons;
  reply.address = Common::swap16(m_read_request.address);

  // Pre-fill with zeros in case of read-error or read < 16-bytes:
  std::fill(std::begin(reply.data), std::end(reply.data), 0x00);

  ErrorCode error_code = ErrorCode::SUCCESS;

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
      error_code = ErrorCode::INVALID_ADDRESS;
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

  case AddressSpace::I2C_BUS:
  case AddressSpace::I2C_BUS_ALT:
  {
    // Attempting to access the EEPROM directly over i2c results in error 8.
    if (EEPROM_I2C_ADDR == m_read_request.slave_address)
    {
      WARN_LOG(WIIMOTE, "Attempt to read EEPROM directly.");
      error_code = ErrorCode::INVALID_ADDRESS;
      break;
    }

    // Top byte of address is ignored on the bus, but it IS maintained in the read-reply.
    auto const bytes_read = m_i2c_bus.BusRead(
        m_read_request.slave_address, (u8)m_read_request.address, bytes_to_read, reply.data);

    if (bytes_read != bytes_to_read)
    {
      DEBUG_LOG(WIIMOTE, "Responding with read error 7 @ 0x%x @ 0x%x (%d)",
                m_read_request.slave_address, m_read_request.address, m_read_request.size);
      error_code = ErrorCode::NACK;
      break;
    }

    reply.size_minus_one = bytes_read - 1;
  }
  break;

  default:
    WARN_LOG(WIIMOTE, "ReadData: invalid address space: 0x%x", int(m_read_request.space));
    // A real wiimote gives error 6:
    error_code = ErrorCode::INVALID_SPACE;
    break;
  }

  if (ErrorCode::SUCCESS != error_code)
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
  m_motion_plus.DoState(p);
  m_camera_logic.DoState(p);

  p.Do(m_active_extension);
  GetActiveExtension()->DoState(p);

  // TODO: Handle motion plus being disabled.
  m_motion_plus.AttachExtension(GetActiveExtension());

  // Dynamics
  // TODO: clean this up:
  p.Do(m_shake_step);

  p.DoMarker("Wiimote");

  if (p.GetMode() == PointerWrap::MODE_READ)
    RealState();
}

ExtensionNumber Wiimote::GetActiveExtensionNumber() const
{
  return m_active_extension;
}

void Wiimote::RealState()
{
  using namespace WiimoteReal;

  if (g_wiimotes[m_index])
  {
    g_wiimotes[m_index]->SetChannel(m_reporting_channel);
  }
}

}  // namespace WiimoteEmu
