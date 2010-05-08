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
#include "StringUtil.h"
#include "pluginspecs_wiimote.h"

#include "EmuMain.h" // Local
#include "EmuSubroutines.h"
#include "Config.h" // for g_Config


extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{
extern void PAD_Rumble(u8 _numPAD, unsigned int _uType);

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
void HidOutputReport(u16 _channelID, wm_report* sr)
{
	INFO_LOG(WIIMOTE, "HidOutputReport (page: %i, cid: 0x%02x, wm: 0x%02x)", g_ID, _channelID, sr->wm);

	switch(sr->wm)
	{
	case WM_RUMBLE: // 0x10
		PAD_Rumble(g_ID, sr->data[0]);
		break;

	case WM_LEDS: // 0x11
		INFO_LOG(WIIMOTE, "Set LEDs: 0x%02x", sr->data[0]);
		g_Leds[g_ID] = sr->data[0] >> 4;
		break;

	case WM_REPORT_MODE:  // 0x12
		WmReportMode(_channelID, (wm_report_mode*)sr->data);
		break;

	case WM_IR_PIXEL_CLOCK: // 0x13
		INFO_LOG(WIIMOTE, "WM IR Clock: 0x%02x", sr->data[0]);
		//g_IRClock[g_ID] = (sr->data[0] & 0x04) ? 1 : 0;
		break;

	case WM_SPEAKER_ENABLE: // 0x14
		INFO_LOG(WIIMOTE, "WM Speaker Enable: 0x%02x", sr->data[0]);
		g_Speaker[g_ID] = (sr->data[0] & 0x04) ? 1 : 0;
		break;

	case WM_REQUEST_STATUS: // 0x15
		WmRequestStatus(_channelID, (wm_request_status*)sr->data);
		break;

	case WM_WRITE_DATA: // 0x16
		WmWriteData(_channelID, (wm_write_data*)sr->data);
		break;

	case WM_READ_DATA: // 0x17
		WmReadData(_channelID, (wm_read_data*)sr->data);
		break;

	case WM_WRITE_SPEAKER_DATA: // 0x18
		// TODO: Does this need an ack?
		break;

	case WM_SPEAKER_MUTE: // 0x19
		INFO_LOG(WIIMOTE, "WM Speaker Mute: 0x%02x", sr->data[0]);
		//g_SpeakerMute[g_ID] = (sr->data[0] & 0x04) ? 1 : 0;
		break;

	case WM_IR_LOGIC: // 0x1a
		// This enables or disables the IR lights, we update the global variable g_IR
	    // so that WmRequestStatus() knows about it
		INFO_LOG(WIIMOTE, "WM IR Enable: 0x%02x", sr->data[0]);
		g_IR[g_ID] = (sr->data[0] & 0x04) ? 1 : 0;
		break;

	default:
		PanicAlert("HidOutputReport: Unknown channel 0x%02x", sr->wm);
		return;
	}
	
	// Send general feedback except the following types
	// as these ones generate their own feedbacks
	// or don't send feedbacks
	if ((sr->wm != WM_RUMBLE)
			&& (sr->wm != WM_READ_DATA) 
			&& (sr->wm != WM_REQUEST_STATUS)
			&& (sr->wm != WM_WRITE_SPEAKER_DATA)
		)
	{
		WmSendAck(_channelID, sr->wm);
	}
}


/* Generate the right header for wm reports. The returned values is the length
   of the header before the data begins. It's always two for all reports 0x20 -
   0x22, 0x30 - 0x37 */
int WriteWmReportHdr(u8* dst, u8 wm)
{
	// Update the first byte to 0xa1
	u32 Offset = 0;
	hid_packet* pHidHeader = (hid_packet*)dst;
	Offset += sizeof(hid_packet);
	pHidHeader->type = HID_TYPE_DATA;
	pHidHeader->param = HID_PARAM_INPUT;

	// Update the second byte to the current report type 0x20 - 0x22, 0x30 - 0x37
	wm_report* pReport = (wm_report*)(dst + Offset);
	Offset += sizeof(wm_report);
	pReport->wm = wm;
	return Offset;
}

/* This will generate the 0x22 acknowledgement for most Input reports.
   It has the form of "a1 22 00 00 _reportID 00".
   The first two bytes are the core buttons data,
   00 00 means nothing is pressed.
   The last byte is the success code 00. */
void WmSendAck(u16 _channelID, u8 _reportID)
{
	u8 DataFrame[1024];
	// Write DataFrame header
	u32 Offset = WriteWmReportHdr(DataFrame, WM_ACK_DATA);
	wm_acknowledge* pData = (wm_acknowledge*)(DataFrame + Offset);
	memset(pData, 0, sizeof(wm_acknowledge));

#if defined(HAVE_WX) && HAVE_WX
	FillReportInfo(pData->buttons);
#endif
	pData->reportID = _reportID;
	pData->errorID = 0;
	Offset += sizeof(wm_acknowledge);

	DEBUG_LOG(WIIMOTE,  "WMSendAck");
	DEBUG_LOG(WIIMOTE,  "  Report ID: %02x", _reportID);

	g_WiimoteInitialize.pWiimoteInput(g_ID, _channelID, DataFrame, Offset);

	// Debugging
	//ReadDebugging(true, DataFrame, Offset);
}


/* Read data from Wiimote and Extensions registers. */
void WmReadData(u16 _channelID, wm_read_data* rd) 
{
	u32 address = convert24bit(rd->address);
	u16 size = convert16bit(rd->size);
    u8 addressHI = (address >> 16) & 0xFE;
	INFO_LOG(WIIMOTE,  "Read data");
	DEBUG_LOG(WIIMOTE, "  Read data Space: %x", rd->space);
	DEBUG_LOG(WIIMOTE, "  Read data Address: 0x%06x", address);
	DEBUG_LOG(WIIMOTE, "  Read data Size: 0x%04x", size);

	/* Now we determine what address space we are reading from. Space 0 is
	   Eeprom and space 1 and 2 are the registers. */
	if(rd->space == WM_SPACE_EEPROM) 
	{
		if (address + size > WIIMOTE_EEPROM_SIZE) 
		{
			PanicAlert("WmReadData: address + size out of bounds");
			return;
		}
		SendReadDataReply(_channelID, g_Eeprom[g_ID] + address, address, addressHI, (int)size);
		/*DEBUG_LOG(WIIMOTE, "Read RegEeprom: Size: %i, Address: %08x,  Offset: %08x",
				size, address, (address & 0xffff));*/
	} 
	else if(rd->space == WM_SPACE_REGS1 || rd->space == WM_SPACE_REGS2)
	{
		u8* block;
		u32 blockSize;
		switch(addressHI) 
		{
		case 0xA2:
			block = g_RegSpeaker[g_ID];
			blockSize = WIIMOTE_REG_SPEAKER_SIZE;
			DEBUG_LOG(WIIMOTE, "  Case 0xa2: g_RegSpeaker");
			break;

		case 0xA4:
			block = g_RegExt[g_ID];
			blockSize = WIIMOTE_REG_EXT_SIZE;
			DEBUG_LOG(WIIMOTE, "  Case 0xa4: ExtReg");
			break;

			// MotionPlus is pretty much just a dummy atm :p
		case 0xA6:
			block = g_RegMotionPlus[g_ID];
			blockSize = WIIMOTE_REG_EXT_SIZE;
			DEBUG_LOG(WIIMOTE, "  Case 0xa6: MotionPlusReg [%x]", address);
			break;

		case 0xB0:
			block = g_RegIr[g_ID];
			blockSize = WIIMOTE_REG_IR_SIZE;
			DEBUG_LOG(WIIMOTE,  "  Case 0xb0: g_RegIr");
			break;

		default:
			ERROR_LOG(WIIMOTE, "WmReadData: bad register block!");
			PanicAlert("WmReadData: bad register block!");
			return;
		}

		// Encrypt data that is read from the Wiimote Extension Register
		if(addressHI == 0xa4)
		{
			// Check if encrypted reads is on
			if(g_RegExt[g_ID][0xf0] == 0xaa)
			{
				/* Copy the registry to a temporary space. We don't want to change the unencrypted
				   data in the registry */
				memcpy(g_RegExtTmp, g_RegExt[g_ID], sizeof(g_RegExt[0]));

				// Encrypt g_RegExtTmp at that location
				wiimote_encrypt(&g_ExtKey[g_ID], &g_RegExtTmp[address & 0xffff], (address & 0xffff), (u8)size);

				// Update the block that SendReadDataReply will eventually send to the Wii
				block = g_RegExtTmp;
			}
		}

		address &= 0xFFFF;
		if(address + size > blockSize)
		{
			PanicAlert("WmReadData: address + size out of bounds! [%d %d %d]", address, size, blockSize);
			return;
		}
		//3x read error due activated(or not present device, which is not the case), WII will await a status report after init and attempting to read from write-only area @A600FE/FF
		if ((g_MotionPlusReadError[g_ID] == 2) && (g_RegExt[g_ID][0xFF]== 0x05)) { //if motionplus is active, its in the current ExtReg, 0xFF will be always 0x05
			WmRequestStatus(_channelID, (wm_request_status*) rd, 1); 
		}
		// Let this function process the message and send it to the Wii
		SendReadDataReply(_channelID, block + address, address, addressHI, (u8)size);
		
	} 
	else
	{
		PanicAlert("WmReadData: unimplemented parameters (size: %i, addr: 0x%x)!", size, rd->space);
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
void SendReadDataReply(u16 _channelID, void* _Base, u16 _Address, u8 _AddressHI, int _Size)
{
	int dataOffset = 0;
	const u8* data = (const u8*)_Base;

	while (_Size > 0)
	{
		u8 DataFrame[1024];
		// Write the first two bytes to DataFrame
		u32 Offset = WriteWmReportHdr(DataFrame, WM_READ_DATA_REPLY);
		
		// Limit the size to 16 bytes
		int copySize = (_Size > 16) ? 16 : _Size;
		// AyuanX: the MTU is 640B though... what a waste!

		wm_read_data_reply* pReply = (wm_read_data_reply*)(DataFrame + Offset);
		memset(pReply,0,sizeof(wm_read_data_reply));
		Offset += sizeof(wm_read_data_reply);

#if defined(HAVE_WX) && HAVE_WX
		FillReportInfo(pReply->buttons);
#endif
		pReply->error = 0;
		// 0x1 means two bytes, 0xf means 16 bytes
		pReply->size = copySize - 1;
		pReply->address = Common::swap16(_Address + dataOffset);

		// Clear the mem first
		memset(pReply->data, 0, 16);

		// Write a pice of _Base to DataFrame
		memcpy(pReply->data, data + dataOffset, copySize);

		// Update DataOffset for the next loop
		dataOffset += copySize;

		/* Out of bounds. The real Wiimote generate an error for the first
		   request to 0x1770 if we dont't replicate that the game will never
		   read the capibration data at the beginning of Eeprom. I think this
		   error is supposed to occur when we try to read above the freely
		   usable space that ends at 0x16ff. */
		if (Common::swap16(pReply->address + pReply->size) > WIIMOTE_EEPROM_FREE_SIZE)
		{
			pReply->size = 0x0f;
			pReply->error = 0x08;
		}
		if (WiiMapping[g_ID].bMotionPlusConnected)
		{
			//MP+ will try to read from this Registeraddress, expecting an error if a previous WM+ activation has been succesful
			//it also returns an error if there was no WM+ present at all
			if (((_Address == 0x00FE ) || (_Address == 0x00FF )) && (_AddressHI == 0xA6))  
			{
				//Check if MP+ is activated, resp. if its in the current RegExt.
				if ((g_RegExt[g_ID][0xFF] == 0x05) && (g_RegMotionPlus[g_ID][0xFF] != 0x05))
				{ 
					pReply->size = 0x0f;
					pReply->error = 0x07; //error: write-only area when activated/or not present
					// we use the read error at the same time as an indicator whether we need to send a faked 0x37 report or not
					g_MotionPlusReadError[g_ID]++;

				}
			}
		}
	

		// Logging
		DEBUG_LOG(WIIMOTE, "SendReadDataReply");
		DEBUG_LOG(WIIMOTE, "  Buttons: 0x%04x", pReply->buttons);
		DEBUG_LOG(WIIMOTE, "  Error: 0x%x", pReply->error);
		DEBUG_LOG(WIIMOTE, "  Size: 0x%x", pReply->size);
		DEBUG_LOG(WIIMOTE, "  Address: 0x%04x", pReply->address);		

#if defined(_DEBUG) || defined(DEBUGFAST)
		std::string Temp = ArrayToString(DataFrame, Offset);
		DEBUG_LOG(WIIMOTE, "Data: %s", Temp.c_str());
#endif

		// Send a piece
		g_WiimoteInitialize.pWiimoteInput(g_ID, _channelID, DataFrame, Offset);

		// Update the size that is left
		_Size -= copySize;

		// Debugging
		//ReadDebugging(true, DataFrame, Offset);
	}
}


/* Write data to Wiimote and Extensions registers. */
void WmWriteData(u16 _channelID, wm_write_data* wd) 
{
	u32 address = convert24bit(wd->address);
	u8	addressHI = (address >> 16) & 0xFE;
	INFO_LOG(WIIMOTE,  "Write data");
	DEBUG_LOG(WIIMOTE, "  Space: %x", wd->space);
	DEBUG_LOG(WIIMOTE, "  Address: 0x%06x", address);
	DEBUG_LOG(WIIMOTE, "  Size: 0x%02x", wd->size);
	// Write to EEPROM
	if(wd->size <= 16 && wd->space == WM_SPACE_EEPROM)
	{
		if(address + wd->size > WIIMOTE_EEPROM_SIZE) {
			ERROR_LOG(WIIMOTE, "WmWriteData: address + size out of bounds!");
			PanicAlert("WmWriteData: address + size out of bounds!");
			return;
		}
		memcpy(g_Eeprom[g_ID] + address, wd->data, wd->size);
	}
	// Write to registers
	else if(wd->size <= 16 && (wd->space == WM_SPACE_REGS1 || wd->space == WM_SPACE_REGS2))
	{
		u8* block;
		u32 blockSize;
		switch(addressHI)
		{
			case 0xA2:
				block = g_RegSpeaker[g_ID];
				blockSize = WIIMOTE_REG_SPEAKER_SIZE;
				DEBUG_LOG(WIIMOTE, "  Case 0xa2: RegSpeaker");
				break;

			case 0xA4:
				block = g_RegExt[g_ID]; // Extension Controller register
				blockSize = WIIMOTE_REG_EXT_SIZE;
				DEBUG_LOG(WIIMOTE, "  Case 0xa4: ExtReg");
				break;

			case 0xA6:
				block = g_RegMotionPlus[g_ID];
				blockSize = WIIMOTE_REG_EXT_SIZE;
				DEBUG_LOG(WIIMOTE, "  Case 0xa6: MotionPlusReg [%x]", address);
				break;

			case 0xB0:
				block = g_RegIr[g_ID];
				blockSize = WIIMOTE_REG_IR_SIZE;
				INFO_LOG(WIIMOTE, "  Case 0xb0: RegIr");
				break;

			default:
				ERROR_LOG(WIIMOTE, "WmWriteData: bad register block!");   
				PanicAlert("WmWriteData: bad register block!");
				return;
		}

		address &= 0xFFFF;

		// Check if the address is within bounds
		if(address + wd->size > blockSize) {
			PanicAlert("WmWriteData: address + size out of bounds!");
			return;
		}
		
		// Finally write the registers to the right structure
		memcpy(block + address, wd->data, wd->size);

		// Generate key for the Wiimote Extension
		if(blockSize == WIIMOTE_REG_EXT_SIZE)
		{
			// Run the key generation on all writes in the key area, it doesn't matter 
			// that we send it parts of a key, only the last full key will have an effect
			if(address >= 0x40 && address <= 0x4c)
				wiimote_gen_key(&g_ExtKey[g_ID], &g_RegExt[g_ID][0x40]);
		}
		if (WiiMapping[g_ID].bMotionPlusConnected) {
			HandlingMotionPlusWrites(wd->data, addressHI, address);
		}
		
	}
	else
	{
		PanicAlert("WmWriteData: unimplemented parameters!");
	}

	/* Just added for home brew... Isn't it enough that we call this from
	InterruptChannel()? Or is there a separate route here that don't pass
	though InterruptChannel()? */
}


/* Here we produce a 0x20 status report to send to the Wii. We currently ignore
   the status request rs and all its eventual instructions it may include (for
   example turn off rumble or something else) and just send the status
   report. */
void WmRequestStatus(u16 _channelID, wm_request_status* rs, int Extension)
{
	u8 DataFrame[1024];
	u32 Offset = WriteWmReportHdr(DataFrame, WM_STATUS_REPORT);

	wm_status_report* pStatus = (wm_status_report*)(DataFrame + Offset);
	Offset += sizeof(wm_status_report);
	memset(pStatus, 0, sizeof(wm_status_report)); // fill the status report with zeros

	// Status values
#if defined(HAVE_WX) && HAVE_WX
	FillReportInfo(pStatus->buttons);
#endif
	pStatus->leds = g_Leds[g_ID]; // leds are 4 bit
	pStatus->ir = g_IR[g_ID]; // 1 bit
	pStatus->speaker = g_Speaker[g_ID]; // 1 bit
	pStatus->battery_low = 0; // battery is okay
	pStatus->battery = 0x5f; // fully charged
	/* Battery levels in voltage
		  0x00 - 0x32: level 1
		  0x33 - 0x43: level 2
		  0x33 - 0x54: level 3
		  0x55 - 0xff: level 4 */

	// Check if we have a specific order about the extension status
	if (Extension == -1)
	{
		// Read config value for the first time
		pStatus->extension = ((g_MotionPlus[g_ID]) || (WiiMapping[g_ID].iExtensionConnected != EXT_NONE)) ? 1 : 0;
	}
	else
	{
		pStatus->extension = (Extension) ? 1 : 0;
	}

	INFO_LOG(WIIMOTE, "Request Status");
	DEBUG_LOG(WIIMOTE, "  Buttons: 0x%04x", pStatus->buttons);
	DEBUG_LOG(WIIMOTE, "  Extension: %x", pStatus->extension);
	DEBUG_LOG(WIIMOTE, "  Speaker: %x", pStatus->speaker);
	DEBUG_LOG(WIIMOTE, "  IR: %x", pStatus->ir);
	DEBUG_LOG(WIIMOTE, "  LEDs: %x", pStatus->leds);


	g_WiimoteInitialize.pWiimoteInput(g_ID, _channelID, DataFrame, Offset);

	// Debugging
	//ReadDebugging(true, DataFrame, Offset);
}


void HandlingMotionPlusWrites(u8* data, u8 addressHI, u32 address){
	switch (addressHI)
	{
	case 0xA4:
		switch (address)
		{
		case 0x00FE: 
			if (data[0] == 0x00) { 
				if ((g_RegExt[g_ID][0xFF] == 0x05) && (g_RegMotionPlus[g_ID][0xFF] != 0x05))
				{
					SwapExtRegisters();
					DEBUG_LOG(WIIMOTE, "Writing [0x%02x] to [0x%02x:%04x]:  Disabling WM+ and swapping registers back", data[0], addressHI, address);
				}
				else {
					DEBUG_LOG(WIIMOTE, "Writing [0x%02x] to [0x%02x:%04x]:  WM+ already inactive", data[0], addressHI, address);
				}
				g_MotionPlus[g_ID] = 1;
			}
			break;

		case 0x00FB:
			//1. Initializing the extension: writing 0x55 ->0xA400F0 and then 0x00 to 0xA400FB.
			//2. Disables an active wiimote; ext disconnect.
			if (data[0] == 0x00) { 
				//1. connecting extension
				if ((g_RegExt[g_ID][0xFF] != 0x05) && (g_RegMotionPlus[g_ID][0xFF] == 0x05)) 
				{
					g_RegMotionPlus[g_ID][0xFE] = 0x04;
					g_RegMotionPlus[g_ID][0xF7] = 0x08; //control init byte, ingame check
					SwapExtRegisters();
					DEBUG_LOG(WIIMOTE, "Writing [0x%02x] to [0x%02x:%04x]: Enabling WM+ and swapping rgisters", data[0], addressHI, address);
					g_MotionPlus[g_ID] = 0;
				} //2. disconnecting extension 
				else if ((g_RegExt[g_ID][0xFF] == 0x05) && (g_RegMotionPlus[g_ID][0xFF] != 0x05)){
					g_RegExt[g_ID][0xFE] = 0x04;
					g_MotionPlus[g_ID] = 1;
					DEBUG_LOG(WIIMOTE, "Writing [0x%02x] to [0x%02x:%04x]: Disabling WM+ and swapping registers back", data[0], addressHI, address);
				}
			}
			break;
		}
		break;

	case 0xA6:
		switch (address)
		{
		case 0x00FE:
			//Enabling WM+, swapping extension registers
			if (data[0] == 0x05) { 
				if ((g_RegExt[g_ID][0xFF] != 0x05) && (g_RegMotionPlus[g_ID][0xFF] == 0x05) ) {
					//The WII will try to read from the readprotected  A6 WM+ register now, we need to reply with an error each time,
					//plus sent an statusreport 0x20(depending on if nunchuk inserted or not)
					g_MotionPlusReadError[g_ID] = 0;
					g_MotionPlus[g_ID] = 1;
					SwapExtRegisters();
					DEBUG_LOG(WIIMOTE, "Writing [0x%02x] to [0x%02x:%04x]: Enabling WM+ and swapping rgisters back", data[0], addressHI, address);
				}
				else {
					DEBUG_LOG(WIIMOTE, "Writing [0x%02x] to [0x%02x:%04x]: WM already enabled no register swapping", data[0], addressHI, address);
				}
			}
			break;

		case 0x00F0: //init() WM+, this will change some control bits in the WM+ register
			if (data[0] == 0x55) { //enables passthroug, convert to 0405* A6 swap
				if ((g_RegMotionPlus[g_ID][0xFF] == 0x05))
				{
					 //control init byte, ingame check
					g_RegMotionPlus[g_ID][0xF7] = 0x08;

					 //motion plus id
					g_RegMotionPlus[g_ID][0xFE] = 0x05;

					//we will swap the register on write to 0x00FE
				}
				else if (g_RegExt[g_ID][0xFF] == 0x05) { //if the wiimote is already active, we will init() the WM+ directly in the ExtReg
					g_RegExt[g_ID][0xFE] = 0x05;
					g_RegExt[g_ID][0xF7] = 0x08;
				}
				g_MotionPlus[g_ID] = 0;
			}
			break;

		default:
			DEBUG_LOG(WIIMOTE, "Writing [0x%02x] to [0x%02x:%04x]: unknown reason", data[0], addressHI, address);
			break;
		}
		break;
	}
}

//Swapping Ext/WM+-registers
void SwapExtRegisters(){

	memset(g_RegExtTmp, 0, sizeof(g_RegExtTmp));
	memcpy(g_RegExtTmp, g_RegExt[g_ID], sizeof(g_RegExt[0]));
	memset(g_RegExt[0], 0, sizeof(g_RegExt[0]));
	memcpy(g_RegExt[g_ID], g_RegMotionPlus[g_ID], sizeof(g_RegMotionPlus[0]));
	memset(g_RegMotionPlus[0], 0, sizeof(g_RegMotionPlus[0]));
	memcpy(g_RegMotionPlus[g_ID], g_RegExtTmp, sizeof(g_RegExtTmp));

	if (g_RegMotionPlus[g_ID][0xFC]) {
		g_RegMotionPlus[g_ID][0xFC] = 0xa6;
	}
	if (g_RegExt[g_ID][0xFC]) {
		g_RegExt[g_ID][0xFC] = 0xa4;
	}
}

} // WiiMoteEmu
