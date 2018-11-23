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

namespace WiimoteEmu
{
void Wiimote::ReportMode(const wm_report_mode* const dr)
{
  // INFO_LOG(WIIMOTE, "Set data report mode");
  // DEBUG_LOG(WIIMOTE, "  Rumble: %x", dr->rumble);
  // DEBUG_LOG(WIIMOTE, "  Continuous: %x", dr->continuous);
  // DEBUG_LOG(WIIMOTE, "  All The Time: %x", dr->all_the_time);
  // DEBUG_LOG(WIIMOTE, "  Mode: 0x%02x", dr->mode);

  // TODO: what does the 'all_the_time' bit really do?
  // m_reporting_auto = dr->all_the_time;
  m_reporting_auto = dr->continuous;
  m_reporting_mode = dr->mode;
  // m_reporting_channel = _channelID;	// this is set in every Interrupt/Control Channel now

  // reset IR camera
  // memset(m_reg_ir, 0, sizeof(*m_reg_ir));  //ugly hack

  if (dr->mode > 0x37)
    PanicAlert("Wiimote: Unsupported Reporting mode.");
  else if (dr->mode < RT_REPORT_CORE)
    PanicAlert("Wiimote: Reporting mode < 0x30.");
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
    break;

  case RT_READ_DATA:  // 0x17
    ReadData(reinterpret_cast<const wm_read_data*>(sr->data));
    return;  // sends its own ack
    break;

  case RT_WRITE_SPEAKER_DATA:  // 0x18
    // Not sure if speaker mute stops the bus write on real hardware, but it's not that important
    if (!m_speaker_mute)
    {
      auto sd = reinterpret_cast<const wm_speaker_data*>(sr->data);
      if (sd->length > 20)
        PanicAlert("EmuWiimote: bad speaker data length!");
      m_i2c_bus.BusWrite(0x51, 0x00, sd->length, sd->data);
    }
    return;  // no ack
    break;

  case RT_SPEAKER_MUTE:  // 0x19
    m_speaker_mute = sr->enable;
    if (false == sr->ack)
      return;
    break;

  case RT_IR_LOGIC:  // 0x1a
    // comment from old plugin:
    // This enables or disables the IR lights, we update the global variable g_IR
    // so that WmRequestStatus() knows about it
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
void Wiimote::SendAck(u8 report_id)
{
  u8 data[6];

  data[0] = 0xA1;
  data[1] = RT_ACK_DATA;

  wm_acknowledge* ack = reinterpret_cast<wm_acknowledge*>(data + 2);

  ack->buttons = m_status.buttons;
  ack->reportID = report_id;
  ack->errorID = 0;

  Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, data, sizeof(data));
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
  HandleExtensionSwap();

  // update status struct
  m_status.extension = m_extension->active_extension ? 1 : 0;

  // set up report
  u8 data[8];
  data[0] = 0xA1;
  data[1] = RT_STATUS_REPORT;

  // status values
  *reinterpret_cast<wm_status_report*>(data + 2) = m_status;

  // send report
  Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, data, sizeof(data));
}

/* Write data to Wiimote and Extensions registers. */
void Wiimote::WriteData(const wm_write_data* const wd)
{
  u16 address = Common::swap16(wd->address);

  INFO_LOG(WIIMOTE, "Wiimote::WriteData: 0x%02x @ 0x%02x (%d)", wd->space, address, wd->size);

  if (wd->size > 16)
  {
    PanicAlert("WriteData: size is > 16 bytes");
    return;
  }

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

    // TODO: generate a writedata error reply, 7 == no such slave (no ack)
    m_i2c_bus.BusWrite(wd->slave_address >> 1, address & 0xff, wd->size, wd->data);

    return;


    //else if (&m_reg_motion_plus == region_ptr)
    //{
    //  // activate/deactivate motion plus
    //  if (0x55 == m_reg_motion_plus.activated)
    //  {
    //    // maybe hacky
    //    m_reg_motion_plus.activated = 0;

    //    RequestStatus();
    //  }
    //}
  }
  break;

  default:
    PanicAlert("WriteData: unimplemented parameters!");
    break;
  }
}

/* Read data from Wiimote and Extensions registers. */
void Wiimote::ReadData(const wm_read_data* const rd)
{
  u16 address = Common::swap16(rd->address);
  u16 size = Common::swap16(rd->size);

  //INFO_LOG(WIIMOTE, "Wiimote::ReadData: %d @ 0x%02x @ 0x%02x (%d)", rd->space, rd->slave_address, address, size);

  ReadRequest rr;
  u8* const block = new u8[size];

  switch (rd->space)
  {
  case WS_EEPROM:
  {
    // Read from EEPROM
    if (address + size >= WIIMOTE_EEPROM_FREE_SIZE)
    {
      if (address + size > WIIMOTE_EEPROM_SIZE)
      {
        PanicAlert("ReadData: address + size out of bounds");
        delete[] block;
        return;
      }

      // generate a read error, even if the start of the block is readable a real wiimote just sends error code 8
      size = 0;
    }

    // read mii data from file
    if (address >= 0x0FCA && address < 0x12C0)
    {
      // TODO Only read the Mii block parts required
      std::ifstream file;
      File::OpenFStream(file, (File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/mii.bin").c_str(),
                        std::ios::binary | std::ios::in);
      file.read((char*)m_eeprom + 0x0FCA, 0x02f0);
      file.close();
    }

    // read memory to be sent to Wii
    memcpy(block, m_eeprom + address, size);
  }
  break;

  case WS_REGS1:
  case WS_REGS2:
  {
    // Read from Control Register

    m_i2c_bus.BusRead(rd->slave_address >> 1, address & 0xff, size, block);
    // TODO: generate read errors, 7 == no such slave (no ack)
  }
  break;

  default:
    PanicAlert("Wiimote::ReadData: unimplemented address space (space: 0x%x)!", rd->space);
    break;
  }

  rr.address = address;
  rr.size = size;
  rr.position = 0;
  rr.data = block;

  // TODO: read requests suppress normal input reports
  // TODO: if there is currently an active read request ignore new ones

  // send up to 16 bytes
  SendReadDataReply(rr);

  // if there is more data to be sent, add it to the queue
  if (rr.size)
    m_read_requests.push(rr);
  else
    delete[] rr.data;
}

void Wiimote::SendReadDataReply(ReadRequest& request)
{
  u8 data[23];
  data[0] = 0xA1;
  data[1] = RT_READ_DATA_REPLY;

  wm_read_data_reply* const reply = reinterpret_cast<wm_read_data_reply*>(data + 2);
  reply->buttons = m_status.buttons;
  reply->address = Common::swap16(request.address);

  // generate a read error
  // Out of bounds. The real Wiimote generate an error for the first
  // request to 0x1770 if we dont't replicate that the game will never
  // read the calibration data at the beginning of Eeprom. I think this
  // error is supposed to occur when we try to read above the freely
  // usable space that ends at 0x16ff.
  if (0 == request.size)
  {
    reply->size = 0x0f;
    reply->error = 0x08;

    memset(reply->data, 0, sizeof(reply->data));
  }
  else
  {
    // Limit the amt to 16 bytes
    // AyuanX: the MTU is 640B though... what a waste!
    const int amt = std::min((u16)16, request.size);

    // no error
    reply->error = 0;

    // 0x1 means two bytes, 0xf means 16 bytes
    reply->size = amt - 1;

    // Clear the mem first
    memset(reply->data, 0, sizeof(reply->data));

    // copy piece of mem
    memcpy(reply->data, request.data + request.position, amt);

    // update request struct
    request.size -= amt;
    request.position += amt;
    request.address += amt;
  }

  // Send a piece
  Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, data, sizeof(data));
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
  p.Do(m_reg_motion_plus);
  p.Do(m_camera_logic.reg_data);
  p.Do(m_ext_logic.reg_data);
  p.Do(m_speaker_logic.reg_data);

  // Do 'm_read_requests' queue
  {
    u32 size = 0;
    if (p.mode == PointerWrap::MODE_READ)
    {
      // clear
      while (!m_read_requests.empty())
      {
        delete[] m_read_requests.front().data;
        m_read_requests.pop();
      }

      p.Do(size);
      while (size--)
      {
        ReadRequest tmp;
        p.Do(tmp.address);
        p.Do(tmp.position);
        p.Do(tmp.size);
        tmp.data = new u8[tmp.size];
        p.DoArray(tmp.data, tmp.size);
        m_read_requests.push(tmp);
      }
    }
    else
    {
      std::queue<ReadRequest> tmp_queue(m_read_requests);
      size = (u32)(m_read_requests.size());
      p.Do(size);
      while (!tmp_queue.empty())
      {
        ReadRequest tmp = tmp_queue.front();
        p.Do(tmp.address);
        p.Do(tmp.position);
        p.Do(tmp.size);
        p.DoArray(tmp.data, tmp.size);
        tmp_queue.pop();
      }
    }
  }
  p.DoMarker("Wiimote");

  if (p.GetMode() == PointerWrap::MODE_READ)
    RealState();
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
}
