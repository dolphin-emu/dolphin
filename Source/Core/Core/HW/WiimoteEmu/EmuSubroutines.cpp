// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

/* HID reports access guide. */

/* 0x10 - 0x1a   Output   EmuMain.cpp: HidOutputReport()
       0x10 - 0x14: General
     0x15: Status report request from the Wii
     0x16 and 0x17: Write and read memory or registers
       0x19 and 0x1a: General
   0x20 - 0x22   Input    EmuMain.cpp: HidOutputReport() to the destination
       0x15 leads to a 0x20 Input report
       0x17 leads to a 0x21 Input report
     0x10 - 0x1a leads to a 0x22 Input report
   0x30 - 0x3f   Input    This file: Update() */

#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "InputCommon/ControllerEmu/ControlGroup/Extension.h"
#include "InputCommon/ControllerEmu/ControlGroup/ModifySettingsButton.h"

namespace WiimoteEmu
{
void Wiimote::ReportMode(const wm_report_mode* const dr)
{
  if (dr->mode < RT_REPORT_CORE || dr->mode > RT_REPORT_INTERLEAVE2 ||
           (dr->mode > RT_REPORT_CORE_ACCEL_IR10_EXT6 && dr->mode < RT_REPORT_EXT21))
  {
    // A real wiimote ignores the entire message if the mode is invalid.
    WARN_LOG(WIIMOTE, "Game requested invalid report mode: 0x%02x", dr->mode);
    return;
  }
  else if (dr->mode > RT_REPORT_CORE_ACCEL_IR10_EXT6)
  {
    PanicAlert("Wiimote: Unsupported Reporting mode: 0x%02x", dr->mode);
  }

  // TODO: A real wiimote sends a report immediately.
  // even on REPORT_CORE and continuous off when the buttons haven't changed.

  // INFO_LOG(WIIMOTE, "Set data report mode");
  // DEBUG_LOG(WIIMOTE, "  Rumble: %x", dr->rumble);
  // DEBUG_LOG(WIIMOTE, "  Continuous: %x", dr->continuous);
  // DEBUG_LOG(WIIMOTE, "  All The Time: %x", dr->all_the_time);
  // DEBUG_LOG(WIIMOTE, "  Mode: 0x%02x", dr->mode);

  // TODO: what does the 'all_the_time' bit really do?
  // m_reporting_auto = dr->all_the_time;
  m_reporting_auto = dr->continuous;
  m_reporting_mode = dr->mode;
}

/* Here we process the Output Reports that the Wii sends. Our response will be
   an Input Report back to the Wii. Input and Output is from the Wii's
   perspective, Output means data to the Wiimote (from the Wii), Input means
   data from the Wiimote.

   The call browser:

   1. Wiimote_InterruptChannel > InterruptChannel > HidOutputReport
   2. Wiimote_ControlChannel > ControlChannel > HidOutputReport

   The IR enable/disable and speaker enable/disable and mute/unmute values are
    bit2: 0 = Disable (0x02), 1 = Enable (0x06)
*/
void Wiimote::HidOutputReport(const wm_report* const sr, const bool send_ack)
{
  DEBUG_LOG(WIIMOTE, "HidOutputReport (page: %i, cid: 0x%02x, wm: 0x%02x)", m_index,
            m_reporting_channel, sr->wm);

  // WiiBrew:
  // In every single Output Report, bit 0 (0x01) of the first byte controls the Rumble feature.
  m_rumble_on = sr->rumble;

  switch (sr->wm)
  {
  case RT_RUMBLE:  // 0x10
    // this is handled above
    return;  // no ack
    break;

  case RT_LEDS:  // 0x11
    // INFO_LOG(WIIMOTE, "Set LEDs: 0x%02x", sr->data[0]);
    m_status.leds = sr->data[0] >> 4;
    break;

  case RT_REPORT_MODE:  // 0x12
    ReportMode(reinterpret_cast<const wm_report_mode*>(sr->data));
    break;

  case RT_IR_PIXEL_CLOCK:  // 0x13
    // INFO_LOG(WIIMOTE, "WM IR Clock: 0x%02x", sr->data[0]);
    // Camera data is currently always updated. Ignoring pixel clock status.
    // m_ir_clock = sr->enable;
    if (false == sr->ack)
      return;
    break;

  case RT_SPEAKER_ENABLE:  // 0x14
    // ERROR_LOG(WIIMOTE, "WM Speaker Enable: %02x", sr->enable);
    // PanicAlert( "WM Speaker Enable: %d", sr->data[0] );
    m_status.speaker = sr->enable;
    if (false == sr->ack)
      return;
    break;

  case RT_REQUEST_STATUS:  // 0x15
    RequestStatus(reinterpret_cast<const wm_request_status*>(sr->data));
    return;  // sends its own ack
    break;

  case RT_WRITE_DATA:  // 0x16
    WriteData(reinterpret_cast<const wm_write_data*>(sr->data));
    return;  // sends its own ack
    break;

  case RT_READ_DATA:  // 0x17
    ReadData(reinterpret_cast<const wm_read_data*>(sr->data));
    return;  // sends its own ack/reply
    break;

  case RT_WRITE_SPEAKER_DATA:  // 0x18
    // Not sure if speaker mute stops the bus write on real hardware, but it's not that important
    if (!m_speaker_mute)
    {
      auto sd = reinterpret_cast<const wm_speaker_data*>(sr->data);
      if (sd->length > 20)
        PanicAlert("EmuWiimote: bad speaker data length!");
      SpeakerData(sd->data, sd->length);
    }
    return;  // no ack
    break;

  case RT_SPEAKER_MUTE:  // 0x19
    m_speaker_mute = sr->enable;
    if (false == sr->ack)
      return;
    break;

  case RT_IR_LOGIC:  // 0x1a
    // Camera data is currently always updated. Just saving this for status requests.
    m_status.ir = sr->enable;
    if (false == sr->ack)
      return;
    break;

  default:
    PanicAlert("HidOutputReport: Unknown channel 0x%02x", sr->wm);
    return;  // no ack
    break;
  }

  SendAck(sr->wm);
}

/* This will generate the 0x22 acknowledgement for most Input reports.
   It has the form of "a1 22 00 00 _reportID 00".
   The first two bytes are the core buttons data,
   00 00 means nothing is pressed.
   The last byte is the success code 00. */
void Wiimote::SendAck(u8 report_id, u8 error_code)
{
  TypedHidPacket<wm_acknowledge> rpt;
  rpt.type = HID_TYPE_DATA;
  rpt.param = HID_PARAM_INPUT;
  rpt.report_id = RT_ACK_DATA;

  auto ack = &rpt.data;

  ack->buttons = m_status.buttons;
  ack->reportID = report_id;
  ack->errorID = error_code;

  Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, rpt.GetData(),
                                         rpt.GetSize());
}

void Wiimote::HandleExtensionSwap()
{
  // handle switch extension
  if (m_extension->active_extension != m_extension->switch_extension)
  {
    // if an extension is currently connected and we want to switch to a different extension
    if ((m_extension->active_extension > 0) && m_extension->switch_extension)
      // detach extension first, wait til next Update() or RequestStatus() call to change to the new
      // extension
      m_extension->active_extension = 0;
    else
      // set the wanted extension
      m_extension->active_extension = m_extension->switch_extension;

    // reset register
    ((WiimoteEmu::Attachment*)m_extension->attachments[m_extension->active_extension].get())
        ->Reset();
  }
}

void Wiimote::RequestStatus(const wm_request_status* const rs)
{
  INFO_LOG(WIIMOTE, "Wiimote::RequestStatus");

  // update status struct
  m_status.extension = m_extension_port.IsDeviceConnected();
  // Battery levels in voltage
  //   0x00 - 0x32: level 1
  //   0x33 - 0x43: level 2
  //   0x33 - 0x54: level 3
  //   0x55 - 0xff: level 4
  m_status.battery = (u8)(m_battery_setting->GetValue() * 0xff);
  // TODO: this right?
  m_status.battery_low = m_status.battery < 0x33;

  // set up report
  TypedHidPacket<wm_status_report> rpt;
  rpt.type = HID_TYPE_DATA;
  rpt.param = HID_PARAM_INPUT;
  rpt.report_id = RT_STATUS_REPORT;

  // status values
  rpt.data = m_status;

  // send report
  Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, rpt.GetData(),
                                         rpt.GetSize());
}

/* Write data to Wiimote and Extensions registers. */
void Wiimote::WriteData(const wm_write_data* const wd)
{
  u16 address = Common::swap16(wd->address);

  if (0 == wd->size)
  {
    // Ignore requests of zero size
    return;
  }

  INFO_LOG(WIIMOTE, "Wiimote::WriteData: 0x%02x @ 0x%02x @ 0x%02x (%d)", wd->space, wd->slave_address, address, wd->size);

  if (wd->size > 16)
  {
    PanicAlert("WriteData: size is > 16 bytes");
    return;
  }

  u8 error_code = 0;

  switch (wd->space)
  {
  case WS_EEPROM:
  {
    // Write to EEPROM

    if (address + wd->size > WIIMOTE_EEPROM_SIZE)
    {
      ERROR_LOG(WIIMOTE, "WriteData: address + size out of bounds!");
      PanicAlert("WriteData: address + size out of bounds!");
      return;
    }
    memcpy(m_eeprom + address, wd->data, wd->size);

    // write mii data to file
    if (address >= 0x0FCA && address < 0x12C0)
    {
      // TODO Only write parts of the Mii block
      std::ofstream file;
      File::OpenFStream(file, File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/mii.bin",
                        std::ios::binary | std::ios::out);
      file.write((char*)m_eeprom + 0x0FCA, 0x02f0);
      file.close();
    }
  }
  break;

  case WS_REGS1:
  case WS_REGS2:
  {
    // Write to Control Register

    // Top byte of address is ignored on the bus.
    auto const bytes_written = m_i2c_bus.BusWrite(wd->slave_address >> 1, (u8)address, wd->size, wd->data);
    if (bytes_written != wd->size)
    {
      // A real wiimote gives error 7 for failed write to i2c bus (mainly a non-existant slave)
      error_code = 0x07;
    }
  }
  break;

  default:
    PanicAlert("WriteData: unimplemented parameters!");
    break;
  }

  SendAck(RT_WRITE_DATA, error_code);
}

/* Read data from Wiimote and Extensions registers. */
void Wiimote::ReadData(const wm_read_data* const rd)
{
  if (m_read_request.size)
  {
    // There is already an active read request.
    // a real wiimote ignores the new one.
    return;
  }

  // Save the request and process it on the next "Update()" calls
  m_read_request.space = rd->space;
  m_read_request.slave_address = rd->slave_address;
  m_read_request.address = Common::swap16(rd->address);
  // A zero size request is just ignored, like on the real wiimote.
  m_read_request.size = Common::swap16(rd->size);

  INFO_LOG(WIIMOTE, "Wiimote::ReadData: %d @ 0x%02x @ 0x%02x (%d)", m_read_request.space,
          m_read_request.slave_address, m_read_request.address, m_read_request.size);

  // Send up to one read-data-reply.
  // If more data needs to be sent it will happen on the next "Update()"
  ProcessReadDataRequest();
}

bool Wiimote::ProcessReadDataRequest()
{
  // Limit the amt to 16 bytes
  // AyuanX: the MTU is 640B though... what a waste!
  u16 const bytes_to_read = std::min((u16)16, m_read_request.size);

  if (0 == bytes_to_read)
  {
    // No active request:
    return false;
  }

  TypedHidPacket<wm_read_data_reply> rpt;
  rpt.type = HID_TYPE_DATA;
  rpt.param = HID_PARAM_INPUT;
  rpt.report_id = RT_READ_DATA_REPLY;

  auto reply = &rpt.data;
  reply->buttons = m_status.buttons;
  reply->address = Common::swap16(m_read_request.address);

  switch (m_read_request.space)
  {
  case WS_EEPROM:
  {
    // Read from EEPROM
    if (m_read_request.address + m_read_request.size >= WIIMOTE_EEPROM_FREE_SIZE)
    {
      if (m_read_request.address + m_read_request.size > WIIMOTE_EEPROM_SIZE)
      {
        PanicAlert("ReadData: address + size out of bounds");
      }

      // generate a read error, even if the start of the block is readable a real wiimote just sends
      // error code 8

      // The real Wiimote generate an error for the first
      // request to 0x1770 if we dont't replicate that the game will never
      // read the calibration data at the beginning of Eeprom. I think this
      // error is supposed to occur when we try to read above the freely
      // usable space that ends at 0x16ff.
      reply->error = 0x08;
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
        file.read((char*)m_eeprom + 0x0FCA, 0x02f0);
        file.close();
      }

      // read memory to be sent to Wii
      std::copy_n(m_eeprom + m_read_request.address, bytes_to_read, reply->data);
      reply->size_minus_one = bytes_to_read - 1;
    }
  }
  break;

  case WS_REGS1:
  case WS_REGS2:
  {
    // Read from Control Register

    // Top byte of address is ignored on the bus, but it IS maintained in the read-reply.
    auto const bytes_read = m_i2c_bus.BusRead(m_read_request.slave_address >> 1,
                                        (u8)m_read_request.address, bytes_to_read, reply->data);

    reply->size_minus_one = bytes_read - 1;

    if (bytes_read != bytes_to_read)
    {
      // generate read error, 7 == no such slave (no ack)
      reply->error = 0x07;
    }
  }
  break;

  default:
    PanicAlert("Wiimote::ReadData: unimplemented address space (space: 0x%x)!", m_read_request.space);
    break;
  }

  // Modify the read request, zero size == complete
  m_read_request.address += bytes_to_read;
  m_read_request.size -= bytes_to_read;

  // Send the data
  Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, rpt.GetData(),
                                         rpt.GetSize());

  return true;
}

void Wiimote::DoState(PointerWrap& p)
{
  p.Do(m_extension->active_extension);
  p.Do(m_extension->switch_extension);

  p.Do(m_accel);
  p.Do(m_index);
  p.Do(ir_sin);
  p.Do(ir_cos);
  p.Do(m_rumble_on);
  p.Do(m_speaker_mute);
  p.Do(m_reporting_auto);
  p.Do(m_reporting_mode);
  p.Do(m_reporting_channel);
  p.Do(m_shake_step);
  p.Do(m_sensor_bar_on_top);
  p.Do(m_status);
  p.Do(m_speaker_logic.adpcm_state);
  p.Do(m_ext_logic.ext_key);
  p.DoArray(m_eeprom);
  p.Do(m_motion_plus_logic.reg_data);
  p.Do(m_camera_logic.reg_data);
  p.Do(m_ext_logic.reg_data);
  p.Do(m_speaker_logic.reg_data);
  p.Do(m_read_request);
  p.DoMarker("Wiimote");

  if (p.GetMode() == PointerWrap::MODE_READ)
    RealState();

  // TODO: rebuild i2c bus state after state-change
}

// load real Wiimote state
void Wiimote::RealState()
{
  using namespace WiimoteReal;

  if (g_wiimotes[m_index])
  {
    g_wiimotes[m_index]->SetChannel(m_reporting_channel);
    g_wiimotes[m_index]->EnableDataReporting(m_reporting_mode);
  }
}
}  // namespace WiimoteEmu
