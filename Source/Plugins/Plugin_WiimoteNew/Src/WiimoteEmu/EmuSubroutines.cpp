// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


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

#include <vector>
#include <string>

#include "Common.h" // Common
#include "pluginspecs_wiimote.h"

#include "WiimoteEmu.h"
#include "Attachment/Attachment.h"

/* Bit shift conversions */
u32 convert24bit(const u8* src)
{
	return (src[0] << 16) | (src[1] << 8) | src[2];
}

u16 convert16bit(const u8* src)
{
	return (src[0] << 8) | src[1];
}

namespace WiimoteEmu
{

void Wiimote::ReportMode(u16 _channelID, wm_report_mode* dr) 
{
	//INFO_LOG(WIIMOTE, "Set data report mode");
	//DEBUG_LOG(WIIMOTE, "  Rumble: %x", dr->rumble);
	//DEBUG_LOG(WIIMOTE, "  Continuous: %x", dr->continuous);
	//DEBUG_LOG(WIIMOTE, "  All The Time: %x", dr->all_the_time);
	//DEBUG_LOG(WIIMOTE, "  Mode: 0x%02x", dr->mode);

	// should I use the rumble var in here?
	//m_rumble->controls[0]->control_ref->State( dr->rumble );

	m_reporting_auto = dr->all_the_time;
	m_reporting_mode = dr->mode;
	m_reporting_channel = _channelID;

	if (0 == dr->all_the_time)
		PanicAlert("Wiimote: Reporting Always is set to OFF! Everything should be fine, but games never do this.");

	// Validation check
	switch (dr->mode)
	{
	case WM_REPORT_CORE :
	case WM_REPORT_CORE_ACCEL :
	case WM_REPORT_CORE_ACCEL_IR12 :
	case WM_REPORT_CORE_ACCEL_EXT16 :
	case WM_REPORT_CORE_ACCEL_IR10_EXT6 :
		break;
	default:
		PanicAlert("Wiimote: Unsupported reporting mode 0x%x", dr->mode);
		break;
	}
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
void Wiimote::HidOutputReport(u16 _channelID, wm_report* sr)
{
	INFO_LOG(WIIMOTE, "HidOutputReport (page: %i, cid: 0x%02x, wm: 0x%02x)", m_index, _channelID, sr->wm);

	switch (sr->wm)
	{
	case WM_RUMBLE : // 0x10
		m_rumble->controls[0]->control_ref->State( sr->data[0] > 0 );
		return;	// no ack
		break;

	case WM_LEDS : // 0x11
		//INFO_LOG(WIIMOTE, "Set LEDs: 0x%02x", sr->data[0]);
		m_status.leds = sr->data[0] >> 4;
		break;

	case WM_REPORT_MODE :  // 0x12
		ReportMode(_channelID, (wm_report_mode*)sr->data);
		break;

	case WM_IR_PIXEL_CLOCK : // 0x13
		//INFO_LOG(WIIMOTE, "WM IR Clock: 0x%02x", sr->data[0]);
		//m_ir_clock = (sr->data[0] & 0x04) ? 1 : 0;
		break;

	case WM_SPEAKER_ENABLE : // 0x14
		//INFO_LOG(WIIMOTE, "WM Speaker Enable: 0x%02x", sr->data[0]);
		m_status.speaker = (sr->data[0] & 0x04) ? 1 : 0;
		break;

	case WM_REQUEST_STATUS : // 0x15
		RequestStatus(_channelID, (wm_request_status*)sr->data);
		return;	// sends its own ack
		break;

	case WM_WRITE_DATA : // 0x16
		WriteData(_channelID, (wm_write_data*)sr->data);
		break;

	case WM_READ_DATA : // 0x17
		ReadData(_channelID, (wm_read_data*)sr->data);
		return;	// sends its own ack
		break;

	case WM_WRITE_SPEAKER_DATA : // 0x18
		// TODO: Does this need an ack?
		return;	// no ack
		break;

	case WM_SPEAKER_MUTE : // 0x19
		//INFO_LOG(WIIMOTE, "WM Speaker Mute: 0x%02x", sr->data[0]);
		//m_speaker_mute = (sr->data[0] & 0x04) ? 1 : 0;
		break;

	case WM_IR_LOGIC: // 0x1a
		// comment from old plugin:
		// This enables or disables the IR lights, we update the global variable g_IR
	    // so that WmRequestStatus() knows about it
		//INFO_LOG(WIIMOTE, "WM IR Enable: 0x%02x", sr->data[0]);
		m_status.ir = (sr->data[0] & 0x04) ? 1 : 0;
		break;

	default:
		PanicAlert("HidOutputReport: Unknown channel 0x%02x", sr->wm);
		return; // no ack
		break;
	}

	// send ack
	SendAck(_channelID, sr->wm);
}

/* This will generate the 0x22 acknowledgement for most Input reports.
   It has the form of "a1 22 00 00 _reportID 00".
   The first two bytes are the core buttons data,
   00 00 means nothing is pressed.
   The last byte is the success code 00. */
void Wiimote::SendAck(u16 _channelID, u8 _reportID)
{
	u8 data[6];

	data[0] = 0xA1;
	data[1] = WM_ACK_DATA;

	wm_acknowledge* const ack = (wm_acknowledge*)(data + 2);

	ack->buttons = m_status.buttons;
	ack->reportID = _reportID;
	ack->errorID = 0;

	m_wiimote_init->pWiimoteInput( m_index, _channelID, data, sizeof(data));
}

/* Here we produce a 0x20 status report to send to the Wii. We currently ignore
   the status request rs and all its eventual instructions it may include (for
   example turn off rumble or something else) and just send the status
   report. */
void Wiimote::RequestStatus(u16 _channelID, wm_request_status* rs, int _Extension)
{
	// handle switch extension
	if ( m_extension->active_extension != m_extension->switch_extension )
	{
		// if an extension is currently connected and we want to switch to a different extension
		if ( (m_extension->active_extension > 0) && m_extension->switch_extension )
			// detach extension first, wait til next Update() or RequestStatus() call to change to the new extension
			m_extension->active_extension = 0;
		else
			// set the wanted extension
			m_extension->active_extension = m_extension->switch_extension;

		// update status struct
		m_status.extension = m_extension->active_extension ? 1 : 0;

		// set register, I hate this line
		m_register[ 0xa40000 ] = ((WiimoteEmu::Attachment*)m_extension->attachments[ m_extension->active_extension ])->reg;
	}

	// set up report
	u8 data[8];
	data[0] = 0xA1;
	data[1] = WM_STATUS_REPORT;

	// status values
	*(wm_status_report*)(data + 2) = m_status;

	// send report
	m_wiimote_init->pWiimoteInput(m_index, _channelID, data, sizeof(data));
}

/* Write data to Wiimote and Extensions registers. */
void Wiimote::WriteData(u16 _channelID, wm_write_data* wd) 
{
	u32 address = convert24bit(wd->address);
	// this is done in ReadDate, but not here in WriteData ?
	//u16 size = convert16bit(rd->size);

	if (wd->size > 16)
	{
		PanicAlert("WriteData: size is > 16 bytes");
		return;
	}

	switch (wd->space)
	{
	case WM_SPACE_EEPROM :
		{
			static bool first = true;
			if (first)
			{
				PanicAlert("WriteData: first write to EEPROM");
				first = false;
			}
			// Write to EEPROM
			if (address + wd->size > WIIMOTE_EEPROM_SIZE)
			{
				ERROR_LOG(WIIMOTE, "WriteData: address + size out of bounds!");
				PanicAlert("WriteData: address + size out of bounds!");
				return;
			}
			memcpy(m_eeprom + address, wd->data, wd->size);
		}
		break;
	case WM_SPACE_REGS1 :
	case WM_SPACE_REGS2 :
		{
			// Write to Control Register

			// ignore the 0x010000 bit
			address &= 0xFEFFFF;

			// ignore second byte for extension area
			if (0xA4 == (address >> 16))
				address &= 0xFF00FF;

			// write to the register
			m_register.Write(address, wd->data, wd->size);

			switch (address >> 16)
			{
			// extension register
			case 0xa4 :
				{
					// Run the key generation on all writes in the key area, it doesn't matter 
					// that we send it parts of a key, only the last full key will have an effect
					// I might have f'ed this up
					if ( address >= 0xa40040 && address <= 0xa4004c )
					{
						u8 data[WIIMOTE_REG_EXT_SIZE];
						m_register.Read( 0xa40000, data, WIIMOTE_REG_EXT_SIZE );
						wiimote_gen_key( &m_ext_key, data + 0x40 );
					}
				}
				break;
			}

		}
		break;
	default:
		PanicAlert("WriteData: unimplemented parameters!");
		break;
	}
}

/* Read data from Wiimote and Extensions registers. */
void Wiimote::ReadData(u16 _channelID, wm_read_data* rd) 
{
	u32 address = convert24bit(rd->address);
	u16 size = convert16bit(rd->size);

	switch (rd->space)
	{
	case WM_SPACE_EEPROM :
		{
			//PanicAlert("ReadData: reading from EEPROM: address: 0x%x size: 0x%x", address, size);
			// Read from EEPROM
			if (address + size > WIIMOTE_EEPROM_FREE_SIZE) 
			{
				if (address + size > WIIMOTE_EEPROM_SIZE) 
				{
					PanicAlert("ReadData: address + size out of bounds");
					return;
				}
				// generate a read error
				size = 0;
			}
			SendReadDataReply(_channelID, m_eeprom + address, address, size);
		}
		break;
	case WM_SPACE_REGS1 :
	case WM_SPACE_REGS2 :
		{
			// Read from Control Register

			// ignore the 0x010000 bit
			address &= 0xFEFFFF;

			// ignore second byte for extension area
			if (0xA4 == (address >> 16))
				address &= 0xFF00FF;

			u8* const block = new u8[ size ];
			m_register.Read( address, block, size );

			switch (address >> 16)
			{
			case 0xa4 :
				{
					// Encrypt data read from extension register
					// Check if encrypted reads is on
					if ( m_reg_ext[0xf0] == 0xaa )
					{
						// I probably totally f'ed this up
						wiimote_encrypt(&m_ext_key, block, address & 0xffff, (u8)size);
					}
				}
				break;
			case 0xa6 :
				{
					// motion plus crap copied from old wiimote plugin
					//block[0xFC] = 0xA6;
					//block[0xFD] = 0x20;
					//block[0xFE] = 0x00;
					//block[0xFF] = 0x05;
				}
				break;
			}

			// Let this function process the message and send it to the Wii
			SendReadDataReply(_channelID, block, address, (int)size);

			delete[] block;
		} 
		break;
	default :
		PanicAlert("WmReadData: unimplemented parameters (size: %i, addr: 0x%x)!", size, rd->space);
		break;
	}
}

/* Here we produce the actual 0x21 Input report that we send to the Wii. The
   message is divided into 16 bytes pieces and sent piece by piece. There will
   be five formatting bytes at the begging of all reports. A common format is
   00 00 f0 00 20, the 00 00 means that no buttons are pressed, the f means 16
   bytes in the message, the 0 means no error, the 00 20 means that the message
   is at the 00 20 offest in the registry that was read.
   
   _Base: The data beginning at _Base[0]
   _Address: The starting address inside the registry, this is used to check for out of bounds reading
   _Size: The total size to send
*/
void Wiimote::SendReadDataReply(u16 _channelID, const void* _Base, unsigned int _Address, unsigned int _Size)
{
	u8 data[23];
	data[0] = 0xA1;
	data[1] = WM_READ_DATA_REPLY;
	wm_read_data_reply* const reply = (wm_read_data_reply*)(data + 2);
	reply->buttons = m_status.buttons;
	reply->error = 0;

	// Out of bounds. The real Wiimote generate an error for the first
	// request to 0x1770 if we dont't replicate that the game will never
	// read the calibration data at the beginning of Eeprom. I think this
	// error is supposed to occur when we try to read above the freely
	// usable space that ends at 0x16ff.
	if (0 == _Size)
	{
		reply->size = 0x0f;
		reply->error = 0x08;
		reply->address = Common::swap16(_Address);

		memset(reply->data, 0, sizeof(reply->data));
		m_wiimote_init->pWiimoteInput(m_index, _channelID, data, sizeof(data));
	}

	while (_Size)
	{
		// Limit the amt to 16 bytes
		// AyuanX: the MTU is 640B though... what a waste!
		const int amt = std::min( (unsigned int)16, _Size );

		// 0x1 means two bytes, 0xf means 16 bytes
		reply->size = amt - 1;
		reply->address = Common::swap16(_Address);

		// Clear the mem first
		memset(reply->data, 0, sizeof(reply->data));

		// copy piece of mem
		memcpy(reply->data, _Base, amt);

		// Send a piece
		m_wiimote_init->pWiimoteInput(m_index, _channelID, data, sizeof(data));

		// advance pointers
		_Size -= amt;
		_Base = (u8*)_Base + amt;
		_Address += amt;
	}
}

}
