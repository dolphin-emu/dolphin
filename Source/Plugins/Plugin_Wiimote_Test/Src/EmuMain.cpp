// Copyright (C) 2003-2008 Dolphin Project.

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


#include "pluginspecs_wiimote.h"

#include <vector>
#include <string>
#include "Common.h"
#include "wiimote_hid.h"
#include "EmuSubroutines.h"
#include "EmuDefinitions.h"
#include "Encryption.h" // for extension encryption
#include "Console.h" // for startConsoleWin, wprintf, GetConsoleHwnd
#include "Config.h" // for g_Config

extern SWiimoteInitialize g_WiimoteInitialize;
//extern void __Log(int log, const char *format, ...);
//extern void __Log(int log, int v, const char *format, ...);


namespace WiiMoteEmu
{

//******************************************************************************
// Subroutines
//******************************************************************************


//wm_request_status global_struct;
//extern wm_request_status global_struct;



// ===================================================
/* Here we process the Output Reports that the Wii sends. Our response will be an Input Report
   back to the Wii. Input and Output is from the Wii's perspective, Output means data to
   the Wiimote (from the Wii), Input means data from the Wiimote. */
// ----------------
void HidOutputReport(u16 _channelID, wm_report* sr) {
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");

	LOGV(WII_IPC_WIIMOTE, 0, "HidOutputReport(0x%02x)", sr->channel);

	switch(sr->channel)
	{
	case 0x10:
		LOGV(WII_IPC_WIIMOTE, 0, "HidOutputReport: unknown sr->channel 0x10");
		break;
	case WM_LEDS: // 0x11
		WmLeds(_channelID, (wm_leds*)sr->data);
		break;
	case WM_DATA_REPORTING:  // 0x12
		WmDataReporting(_channelID, (wm_data_reporting*)sr->data);
		break;
	case WM_REQUEST_STATUS: // 0x15
		//global_struct = (wm_request_status*)sr->data;
		WmRequestStatus(_channelID, (wm_request_status*)sr->data);
		break;
	case WM_READ_DATA: // 0x17
		WmReadData(_channelID, (wm_read_data*)sr->data);
		break;
	case WM_IR_PIXEL_CLOCK: // 0x13
	case WM_IR_LOGIC: // 0x1a
		LOGV(WII_IPC_WIIMOTE, 0, "  IR Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;
	case WM_WRITE_DATA: // 0x16
		WmWriteData(_channelID, (wm_write_data*)sr->data);
		break;
	case WM_SPEAKER_ENABLE:
		LOGV(WII_IPC_WIIMOTE, 1, "  WM Speaker Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;
	case WM_SPEAKER_MUTE:
		LOGV(WII_IPC_WIIMOTE, 1, "  WM Mute Enable 0x%02x 0x%02x", sr->channel, sr->data[0]);
		break;
	default:
		PanicAlert("HidOutputReport: Unknown channel 0x%02x", sr->channel);
		return;
	}
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
}


// ===================================================
/* LED (blue lights) report. */
// ----------------
void WmLeds(u16 _channelID, wm_leds* leds) {
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");

	LOG(WII_IPC_WIIMOTE, " Set LEDs");
	LOG(WII_IPC_WIIMOTE, "  Leds: %x", leds->leds);
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", leds->rumble);

	g_Leds = leds->leds;
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
}


void WmSendAck(u16 _channelID, u8 _reportID)
{
	u8 DataFrame[1024];

	u32 Offset = 0;

	// header
	hid_packet* pHidHeader = (hid_packet*)(DataFrame + Offset);
	pHidHeader->type = HID_TYPE_DATA;
	pHidHeader->param = HID_PARAM_INPUT;
	Offset += sizeof(hid_packet);

	wm_acknowledge* pData = (wm_acknowledge*)(DataFrame + Offset);
	pData->Channel = WM_WRITE_DATA_REPLY;
	pData->unk0 = 0;
	pData->unk1 = 0;
	pData->reportID = _reportID;
	pData->errorID = 0;
	Offset += sizeof(wm_acknowledge);


	LOGV(WII_IPC_WIIMOTE, 2, "  WMSendAck()");

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
}


// ===================================================
/* Read data from Wiimote and Extensions registers. */
// ----------------
void WmReadData(u16 _channelID, wm_read_data* rd) 
{
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
	u32 address = convert24bit(rd->address);
	u16 size = convert16bit(rd->size);
	LOG(WII_IPC_WIIMOTE, " Read data");
	LOG(WII_IPC_WIIMOTE, "  Address space: %x", rd->space);
	LOG(WII_IPC_WIIMOTE, "  Address: 0x%06x", address);
	LOG(WII_IPC_WIIMOTE, "  Size: 0x%04x", size);
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", rd->rumble);

	if(rd->space == 0) 
	{
		if (address + size > WIIMOTE_EEPROM_SIZE) 
		{
			PanicAlert("WmReadData: address + size out of bounds!");
			return;
		}
		SendReadDataReply(_channelID, g_Eeprom+address, address, (u8)size);
	} 
	else if(rd->space == WM_SPACE_REGS1 || rd->space == WM_SPACE_REGS2)
	{
		u8* block;
		u32 blockSize;
		switch((address >> 16) & 0xFE) 
		{
/**/	case 0xA2:
			block = g_RegSpeaker;
			blockSize = WIIMOTE_REG_SPEAKER_SIZE;
			LOGV(WII_IPC_WIIMOTE, 0, "WmReadData: 0xB0 g_RegSpeaker");
			break;
		case 0xA4:
			block = g_RegExt;
			blockSize = WIIMOTE_REG_EXT_SIZE;
			LOGV(WII_IPC_WIIMOTE, 0, "  Case 0xA4: Read ExtReg ****************************");
			//wprintf("WmReadData\n"); ReadExt();
			break;
/**/	case 0xB0:
			block = g_RegIr;
			blockSize = WIIMOTE_REG_IR_SIZE;
			//PanicAlert("WmReadData: 0xB0 g_RegIr");
			LOGV(WII_IPC_WIIMOTE, 0, "WmReadData: 0xB0 g_RegIr");
			break;
		default:
			PanicAlert("WmWriteData: bad register block!");
			return;
		}


		// -----------------------------------------
		// Encrypt data that is read from the Wiimote Extension Register
		// -------------
		if(((address >> 16) & 0xfe) == 0xa4)
		{
			wprintf("\n\nWmReadData  Address: %08x Offset: %08x Size: %i byte\n",
				address, address & 0xffff, (u8)size);
			
			// Debugging
			u32 offset = address & 0xffff;
			wprintf("Unencrypted data:\n");
			for (int i = 0; i < (u8)size; i++)
			{			
				wprintf("%02x ", g_RegExt[i + offset]);
				if((i + 1) % 20 == 0) wprintf("\n");
			}

			// Check if encrypted reads is on
			if(g_RegExt[0xf0] == 0xaa)
			{
				// Copy g_RegExt to g_RegExtTmp
				memcpy(g_RegExtTmp, g_RegExt, sizeof(g_RegExt));

				// Copy the registry to a temporary space
				memcpy(g_RegExtTmp, g_RegExt, sizeof(g_RegExt));

				// Encrypt the read
				wiimote_encrypt(&g_ExtKey, &g_RegExtTmp[address & 0xffff], (address & 0xffff), (u8)size);

				// Debugging
				wprintf("\nEncrypted data:\n");
				for (int i = 0; i < (u8)size; i++)
				{			
					wprintf("%02x ", g_RegExtTmp[i + offset]);
					if((i + 1) % 20 == 0) wprintf("\n");
				}

				//wprintf("\ng_RegExtTmp after: \n");
				//ReadExtTmp();
				//wprintf("\nFrom g_RegExt: \n");
				//ReadExt();

				// Update the block that SendReadDataReply will eventually send to the Wii
				block = g_RegExtTmp;
			}

			wprintf("\n\n\n");
		}
		//---------


		address &= 0xFFFF;
		if(address + size > blockSize) {
			PanicAlert("WmReadData: address + size out of bounds!");
			return;
		}



		SendReadDataReply(_channelID, block+address, address, (u8)size);
	} 
	else
	{
		PanicAlert("WmReadData: unimplemented parameters (size: %i, addr: 0x%x!", size, rd->space);
	}
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
}



// ===================================================
/* Write data to Wiimote and Extensions registers. */
// ----------------
void WmWriteData(u16 _channelID, wm_write_data* wd) 
{
	LOGV(WII_IPC_WIIMOTE, 0, "========================================================");
	u32 address = convert24bit(wd->address);
	LOG(WII_IPC_WIIMOTE, " Write data");
	LOG(WII_IPC_WIIMOTE, "  Address space: %x", wd->space);
	LOG(WII_IPC_WIIMOTE, "  Address: 0x%06x", address);
	LOG(WII_IPC_WIIMOTE, "  Size: 0x%02x", wd->size);
	LOG(WII_IPC_WIIMOTE, "  Rumble: %x", wd->rumble);

	// Write to EEPROM
	if(wd->size <= 16 && wd->space == WM_SPACE_EEPROM)
	{
		if(address + wd->size > WIIMOTE_EEPROM_SIZE) {
			PanicAlert("WmWriteData: address + size out of bounds!");
			return;
		}
		memcpy(g_Eeprom + address, wd->data, wd->size);

	//	WmSendAck(_channelID, WM_WRITE_DATA);
	}
	// Write to registers
	else if(wd->size <= 16 && (wd->space == WM_SPACE_REGS1 || wd->space == WM_SPACE_REGS2))
	{
		u8* block;
		u32 blockSize;
		switch((address >> 16) & 0xFE)
		{
			case 0xA2:
				block = g_RegSpeaker;
				blockSize = WIIMOTE_REG_SPEAKER_SIZE;
				break;
			case 0xA4:
				block = g_RegExt; // Extension Controller register
				blockSize = WIIMOTE_REG_EXT_SIZE;
				LOGV(WII_IPC_WIIMOTE, 0, "  *******************************************************");
				LOGV(WII_IPC_WIIMOTE, 0, "");
				LOGV(WII_IPC_WIIMOTE, 0, "  Case 0xA4: Write ExtReg");
				LOGV(WII_IPC_WIIMOTE, 0, "");
				LOGV(WII_IPC_WIIMOTE, 0, "  *******************************************************");
				wprintf("\n\nWmWriteData  Size: %i  Address: %08x  Offset: %08x \n",
					wd->size, address, (address & 0xffff));
				break;
			case 0xB0:
				block = g_RegIr;
				blockSize = WIIMOTE_REG_IR_SIZE;
				break;
			default:
				PanicAlert("WmWriteData: bad register block!");
				return;
		}

		
		 // Remove for example 0xa40000 from the address
		address &= 0xFFFF;

		// Check if the address is within bounds
		if(address + wd->size > blockSize) {
			PanicAlert("WmWriteData: address + size out of bounds!");
			return;
		}

		// Finally write the registers to the right structure
		memcpy(block + address, wd->data, wd->size);

		// -----------------------------------------
		// Generate key for the Wiimote Extension
		// -------------
		if(blockSize == WIIMOTE_REG_EXT_SIZE)
		{
			// Debugging
			// Write the data 
			wprintf("Data: ");
			for (int i = 0; i < wd->size; i++)
			{			
				wprintf("%02x ", wd->data[i]);
				if((i + 1) % 25 == 0) wprintf("\n");
			}
			wprintf("\n");

			//wprintf("Current address: %08x\n", address);

			//wprintf("g_RegExt:\n");
			//ReadExt();


			/* Run the key generation on all writes in the key area, it doesn't matter 
			   that we send it parts of a key, only the last full key will have an
			   effect */
			//if(address >= 0xa40040 && address <= 0xa4004c)
			if(address >= 0x40 && address <= 0x4c)
			//if(address == 0x4c)
					wiimote_gen_key(&g_ExtKey, &g_RegExt[0x40]);
		}
		// -------------


	} else {
		PanicAlert("WmWriteData: unimplemented parameters!");
	}

	// just added for home brew.... hmmmm
	WmSendAck(_channelID, WM_WRITE_DATA);
	LOGV(WII_IPC_WIIMOTE, 0, "==========================================================");
}


// ===================================================
/* Generate the right address for wm reports. */
// ----------------
int WriteWmReport(u8* dst, u8 channel) {
	u32 Offset = 0;
	hid_packet* pHidHeader = (hid_packet*)(dst + Offset);
	Offset += sizeof(hid_packet);
	pHidHeader->type = HID_TYPE_DATA;
	pHidHeader->param = HID_PARAM_INPUT;

	wm_report* pReport = (wm_report*)(dst + Offset);
	Offset += sizeof(wm_report);
	pReport->channel = channel;
	return Offset;
}


// Custom call version
void WmRequestStatus_(u16 _channelID, int a)
{
	//PanicAlert("WmRequestStatus");
	LOGV(WII_IPC_WIIMOTE, 0, "================================================");
	LOGV(WII_IPC_WIIMOTE, 0, " Request Status");
	LOGV(WII_IPC_WIIMOTE, 0, "  Channel: %04x", _channelID);

	//SendStatusReport();
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_STATUS_REPORT);

	wm_status_report* pStatus = (wm_status_report*)(DataFrame + Offset);
	Offset += sizeof(wm_status_report);
	memset(pStatus, 0, sizeof(wm_status_report)); // fill the status report with zeroes

	// Status values
	pStatus->leds = g_Leds;
	pStatus->ir = 1;
	pStatus->battery = 0x4F;	//arbitrary number


	if(a == 1)
	{
		wprintf("pStatus->extension = 1\n");
		pStatus->extension = 1;
	}
	else
	{
		wprintf("pStatus->extension = 0\n");
		pStatus->extension = 0;
	}


	LOGV(WII_IPC_WIIMOTE, 0, "  Extension: %x", pStatus->extension);
	LOGV(WII_IPC_WIIMOTE, 0, "  SendStatusReport()");
	LOGV(WII_IPC_WIIMOTE, 0, "    Flags: 0x%02x", pStatus->padding1[2]);
	LOGV(WII_IPC_WIIMOTE, 0, "    Battery: %d", pStatus->battery);

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
	LOGV(WII_IPC_WIIMOTE, 0, "=================================================");
}


// ===================================================
/* Here we produce a status report to send to the Wii. We currently ignore the status
   request rs and all its eventual instructions it may include (for example turn off
   rumble or something else) and just send the status report. */
// ----------------
void WmRequestStatus(u16 _channelID, wm_request_status* rs)
{
	//PanicAlert("WmRequestStatus");
	LOGV(WII_IPC_WIIMOTE, 0, "================================================");
	LOGV(WII_IPC_WIIMOTE, 0, " Request Status");
	LOGV(WII_IPC_WIIMOTE, 0, "  Rumble: %x", rs->rumble);
	LOGV(WII_IPC_WIIMOTE, 0, "  Channel: %04x", _channelID);

	//SendStatusReport();
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_STATUS_REPORT);

	wm_status_report* pStatus = (wm_status_report*)(DataFrame + Offset);
	Offset += sizeof(wm_status_report);
	memset(pStatus, 0, sizeof(wm_status_report)); // fill the status report with zeroes

	// Status values
	pStatus->leds = g_Leds;
	pStatus->ir = 1;
	pStatus->battery = 0x4F;	//arbitrary number

	// Read config value for this one
	if(g_Config.bExtensionConnected)
		pStatus->extension = 1;
	else
		pStatus->extension = 0;

	LOGV(WII_IPC_WIIMOTE, 0, "  Extension: %x", pStatus->extension);
	LOGV(WII_IPC_WIIMOTE, 0, "  SendStatusReport()");
	LOGV(WII_IPC_WIIMOTE, 0, "    Flags: 0x%02x", pStatus->padding1[2]);
	LOGV(WII_IPC_WIIMOTE, 0, "    Battery: %d", pStatus->battery);

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
	LOGV(WII_IPC_WIIMOTE, 0, "=================================================");
}


// ===================================================
/* Here we produce the actual reply that we send to the Wii. The message is divided
   into 16 bytes pieces and sent piece by piece. */
// ----------------
void SendReadDataReply(u16 _channelID, void* _Base, u16 _Address, u8 _Size)
{
	LOGV(WII_IPC_WIIMOTE, 0, "=========================================");
	int dataOffset = 0;
	while (_Size > 0)
	{
		u8 DataFrame[1024];
		u32 Offset = WriteWmReport(DataFrame, WM_READ_DATA_REPLY);
		
		int copySize = _Size;
		if (copySize > 16)
		{
			copySize = 16;
		}

		wm_read_data_reply* pReply = (wm_read_data_reply*)(DataFrame + Offset);
		Offset += sizeof(wm_read_data_reply);
		pReply->buttons = 0;
		pReply->error = 0;
		pReply->size = (copySize - 1) & 0xF;
		pReply->address = Common::swap16(_Address + dataOffset);


		// Write a pice
		memcpy(pReply->data + dataOffset, _Base, copySize);

		if(copySize < 16) // check if we have less than 16 bytes left to send
		{
			memset(pReply->data + copySize, 0, 16 - copySize);
		}
		dataOffset += copySize;


		LOG(WII_IPC_WIIMOTE, "  SendReadDataReply()");
		LOG(WII_IPC_WIIMOTE, "    Buttons: 0x%04x", pReply->buttons);
		LOG(WII_IPC_WIIMOTE, "    Error: 0x%x", pReply->error);
		LOG(WII_IPC_WIIMOTE, "    Size: 0x%x", pReply->size);
		LOG(WII_IPC_WIIMOTE, "    Address: 0x%04x", pReply->address);

		// Debugging
		/*
		wprintf("SendReadDataReply\n");
		for (int i = 0; i < sizeof(DataFrame); i++)
		{			
			wprintf("%02x ", g_RegExt[i]);
			if((i + 1) % 25 == 0) wprintf("\n");
		}
		wprintf("\n\n");
		*/
		//---------

		// Send a piece
		g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);

		_Size -= copySize;
	}

	if (_Size != 0)
	{
		PanicAlert("WiiMote-Plugin: SendReadDataReply() failed");
	}
	LOGV(WII_IPC_WIIMOTE, 0, "==========================================");
}


} // end of namespace
