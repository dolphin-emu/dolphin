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




// ===================================================
/* HID reports access guide. */
// ----------------

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

// ================


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <vector>
#include <string>

#include "Common.h" // Common
#include "StringUtil.h"
#include "pluginspecs_wiimote.h"

#include "main.h" // Local
#include "wiimote_hid.h"
#include "EmuMain.h"
#include "EmuSubroutines.h"
#include "EmuDefinitions.h"
#include "Logging.h" // for startConsoleWin, Console::Print, GetConsoleHwnd
#include "Config.h" // for g_Config
/////////////////////////////////


extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{

//******************************************************************************
// Subroutines
//******************************************************************************


// ===================================================
/* Here we process the Output Reports that the Wii sends. Our response will be an Input Report
   back to the Wii. Input and Output is from the Wii's perspective, Output means data to
   the Wiimote (from the Wii), Input means data from the Wiimote.
   
   The call browser:

   1. Wiimote_InterruptChannel > InterruptChannel > HidOutputReport
   2. Wiimote_ControlChannel > ControlChannel > HidOutputReport
   
   The IR lights and speaker enable/disable and mute/unmute values are
		0x2 = Disable
		0x6 = Enable */
// ----------------
void HidOutputReport(u16 _channelID, wm_report* sr) {
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
	LOGV(WII_IPC_WIIMOTE, 0, "HidOutputReport (0x%02x)", sr->channel);
	std::string Temp;

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
		if (!g_Config.bUseRealWiimote || !g_RealWiiMotePresent) WmRequestStatus(_channelID, (wm_request_status*)sr->data);
		//Temp = ArrayToString(sr->data, sizeof(wm_request_status), 0);
		//Console::Print("\n%s: InterruptChannel: %s\n", Tm().c_str(), Temp.c_str());
		break;
	case WM_READ_DATA: // 0x17
		if (!g_Config.bUseRealWiimote || !g_RealWiiMotePresent) WmReadData(_channelID, (wm_read_data*)sr->data);
		break;

	/* This enables or disables the IR lights, we update the global variable g_IR
	   so that WmRequestStatus() knows about it */
	case WM_IR_PIXEL_CLOCK: // 0x13
	case WM_IR_LOGIC: // 0x1a
		LOGV(WII_IPC_WIIMOTE, 0, "  IR Enable 0x%02x: 0x%02x", sr->channel, sr->data[0]);
		Console::Print("IR Enable/Disable 0x%02x: 0x%02x\n", sr->channel, sr->data[0]);
		if(sr->data[0] == 0x02) g_IR = 0;
			else if(sr->data[0] == 0x06) g_IR = 1;
		break;

	case WM_WRITE_DATA: // 0x16
		if (!g_Config.bUseRealWiimote || !g_RealWiiMotePresent) WmWriteData(_channelID, (wm_write_data*)sr->data);
		break;
	case WM_SPEAKER_ENABLE: // 0x14
		LOGV(WII_IPC_WIIMOTE, 1, "  WM Speaker Enable 0x%02x: 0x%02x", sr->channel, sr->data[0]);
		//Console::Print("Speaker Enable/Disable 0x%02x: 0x%02x\n", sr->channel, sr->data[0]);
		if(sr->data[0] == 0x02) g_Speaker = 0;
			else if(sr->data[0] == 0x06) g_Speaker = 1;
		break;
	case WM_SPEAKER_MUTE:
		LOGV(WII_IPC_WIIMOTE, 1, "  WM Mute Enable 0x%02x: 0x%02x", sr->channel, sr->data[0]);
		//Console::Print("Speaker Mute/Unmute 0x%02x: 0x%02x\n", sr->channel, sr->data[0]);
		if(sr->data[0] == 0x02) g_SpeakerVoice = 0; // g_SpeakerVoice
			else if(sr->data[0] == 0x06) g_SpeakerVoice = 1;
		break;
	default:
		PanicAlert("HidOutputReport: Unknown channel 0x%02x", sr->channel);
		return;
	}
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
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


// ===================================================
/* LED (blue lights) report. */
// ----------------
void WmLeds(u16 _channelID, wm_leds* leds) {
	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
	LOG(WII_IPC_WIIMOTE, " Set LEDs");
	LOG(WII_IPC_WIIMOTE, "   Leds: %x", leds->leds);
	LOG(WII_IPC_WIIMOTE, "   Rumble: %x", leds->rumble);

	g_Leds = leds->leds;

	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
}


// ===================================================
/* This will generate the 0x22 acknowledgment after all Input reports. It will
   have the form a1 22 00 00 _reportID 00. The first two bytes are the core buttons data,
   they are 00 00 when nothing is pressed. The last byte is the success code 00. */
// ----------------
void WmSendAck(u16 _channelID, u8 _reportID, u32 address)
{
	u8 DataFrame[1024];
	u32 Offset = 0;

	// Header
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
	LOGV(WII_IPC_WIIMOTE, 2, "      Report ID: %02x", _reportID);
	std::string Temp = ArrayToString(DataFrame, Offset, 0);
	//LOGV(WII_IPC_WIIMOTE, 2, "      Data: %s", Temp.c_str());
	Console::Print("%s: WMSendAck: %s\n", Tm(true).c_str(), Temp.c_str());

	/* Debug. Write the report for extension registry writes.
	if((_reportID == 0x16 || _reportID == 0x17)  &&  ((address >> 16) & 0xfe) == 0xa4)
	{
		Console::Print("\nWMSendAck  Report ID: %02x  Encryption: %02x\n", _reportID, g_RegExt[0xf0]);		
		Console::Print("Data: %s\n", Temp.c_str());
	}*/

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
	std::string Temp;
	LOG(WII_IPC_WIIMOTE, "Read data");
	LOG(WII_IPC_WIIMOTE, "    Address space: %x", rd->space);
	LOG(WII_IPC_WIIMOTE, "    Address: 0x%06x", address);
	LOG(WII_IPC_WIIMOTE, "    Size: 0x%04x", size);
	LOG(WII_IPC_WIIMOTE, "    Rumble: %x", rd->rumble);
	
	//u32 _address = address;
	std::string Tmp; // Debugging

	/* Now we determine what address space we are reading from. Space 0 is Eeprom and
	   space 1 and 2 is the registers. */
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
		case 0xA2:
			block = g_RegSpeaker;
			blockSize = WIIMOTE_REG_SPEAKER_SIZE;
			LOGV(WII_IPC_WIIMOTE, 0, "    Case 0xa2: g_RegSpeaker");
			//Tmp = ArrayToString(g_RegSpeaker, size, (address & 0xffff));
			//LOGV(WII_IPC_WIIMOTE, 0, "    Data: %s", Temp.c_str());
			//Console::Print("Read RegSpkr:   Size %i  Address %08x  Offset %08x\nData %s\n",
			//	size, address, (address & 0xffff), Tmp.c_str());
			break;
		case 0xA4:
			block = g_RegExt;
			blockSize = WIIMOTE_REG_EXT_SIZE;
			LOGV(WII_IPC_WIIMOTE, 0, "    Case 0xa4: Read ExtReg ****************************");
			//Tmp = ArrayToString(g_RegExt, size, (address & 0xffff));
			//LOGV(WII_IPC_WIIMOTE, 0, "    Data: %s", Temp.c_str());		
			//Console::Print("Read RegExt: Size %i Address %08x  Offset %08x\nData %s\n",
			//		size, address, (address & 0xffff), Tmp.c_str());
			break;
		case 0xB0:
			block = g_RegIr;
			blockSize = WIIMOTE_REG_IR_SIZE;
			LOGV(WII_IPC_WIIMOTE, 0, "    Case: 0xb0 g_RegIr");
			//Tmp = ArrayToString(g_RegIr, size, (address & 0xffff));
			//LOGV(WII_IPC_WIIMOTE, 0, "    Data: %s", Temp.c_str());
			//Console::Print("Read RegIR: Size %i Address %08x  Offset %08x\nData %s\n",
			//		size, address, (address & 0xffff), Tmp.c_str());
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
			/* Debugging
			Console::Print("\n\nWmReadData  Address: %08x Offset: %08x Size: %i byte\n",
				address, address & 0xffff, (u8)size);			
			// Debugging 
			u32 offset = address & 0xffff;
			std::string Temp = ArrayToString(g_RegExt, size, offset);
			Console::Print("Unencrypted data:\n%s\n", Temp.c_str());*/

			// Check if encrypted reads is on
			if(g_RegExt[0xf0] == 0xaa)
			{
				/* Copy the registry to a temporary space. We don't want to change the unencrypted
				   data in the registry */
				memcpy(g_RegExtTmp, g_RegExt, sizeof(g_RegExt));

				// Encrypt g_RegExtTmp at that location
				wiimote_encrypt(&g_ExtKey, &g_RegExtTmp[address & 0xffff], (address & 0xffff), (u8)size);

				/* Debugging: Show the encrypted data
				std::string Temp = ArrayToString(g_RegExtTmp, size, offset);
				Console::Print("Encrypted data:\n%s\n", Temp.c_str());*/

				// Update the block that SendReadDataReply will eventually send to the Wii
				block = g_RegExtTmp;
			}
		}
		//---------


		address &= 0xFFFF;
		if(address + size > blockSize) {
			PanicAlert("WmReadData: address + size out of bounds!");
			return;
		}

		// Let this function process the message and send it to the Wii
		SendReadDataReply(_channelID, block+address, address, (u8)size);


	} 
	else
	{
		PanicAlert("WmReadData: unimplemented parameters (size: %i, addr: 0x%x!", size, rd->space);
	}

	// Acknowledge the 0x17 read, we will do this from InterruptChannel() 
	//WmSendAck(_channelID, WM_READ_DATA, _address);

	LOGV(WII_IPC_WIIMOTE, 0, "===========================================================");
}



// ===================================================
/* Write data to Wiimote and Extensions registers. */
// ----------------
void WmWriteData(u16 _channelID, wm_write_data* wd) 
{
	LOGV(WII_IPC_WIIMOTE, 0, "========================================================");
	u32 address = convert24bit(wd->address);	
	LOG(WII_IPC_WIIMOTE, "Write data");
	LOG(WII_IPC_WIIMOTE, "    Address space: %x", wd->space);
	LOG(WII_IPC_WIIMOTE, "    Address: 0x%06x", address);
	LOG(WII_IPC_WIIMOTE, "    Size: 0x%02x", wd->size);
	LOG(WII_IPC_WIIMOTE, "    Rumble: %x", wd->rumble);
	//std::string Temp = ArrayToString(wd->data, wd->size);
	//LOGV(WII_IPC_WIIMOTE, 0, "    Data: %s", Temp.c_str());

	// Write to EEPROM
	if(wd->size <= 16 && wd->space == WM_SPACE_EEPROM)
	{
		if(address + wd->size > WIIMOTE_EEPROM_SIZE) {
			PanicAlert("WmWriteData: address + size out of bounds!");
			return;
		}
		memcpy(g_Eeprom + address, wd->data, wd->size);
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
				LOGV(WII_IPC_WIIMOTE, 0, "    Case 0xa2: RegSpeaker");
				//Console::Print("Write RegSpeaker: Size: %i, Address: %08x,  Offset: %08x\n",
				//	wd->size, address, (address & 0xffff));
				//Console::Print("Data: %s\n", Temp.c_str());
				break;
			case 0xA4:
				block = g_RegExt; // Extension Controller register
				blockSize = WIIMOTE_REG_EXT_SIZE;
				//LOGV(WII_IPC_WIIMOTE, 0, "  *******************************************************");
				LOGV(WII_IPC_WIIMOTE, 0, "    Case 0xa4: ExtReg");
				//LOGV(WII_IPC_WIIMOTE, 0, "  *******************************************************");
				/*Console::Print("Write RegExt  Size: %i  Address: %08x  Offset: %08x \n",
					wd->size, address, (address & 0xffff));
				Console::Print("Data: %s\n", Temp.c_str());*/
				break;
			case 0xB0:
				block = g_RegIr;
				blockSize = WIIMOTE_REG_IR_SIZE;
				LOGV(WII_IPC_WIIMOTE, 0, "    Case 0xb0: RegIr");
				/*Console::Print("Write RegIR   Size: %i  Address: %08x  Offset: %08x \n",
					wd->size, address, (address & 0xffff));
				Console::Print("Data: %s\n", Temp.c_str());*/
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
			/* Debugging. Write the data. 
			Console::Print("Data: %s\n", Temp.c_str());
			Console::Print("Current address: %08x\n", address); */

			/* Run the key generation on all writes in the key area, it doesn't matter 
			   that we send it parts of a key, only the last full key will have an
			   effect */
			if(address >= 0x40 && address <= 0x4c)
				wiimote_gen_key(&g_ExtKey, &g_RegExt[0x40]);
		}
		// -------------


	} else {
		PanicAlert("WmWriteData: unimplemented parameters!");
	}

	/* Just added for home brew... Isn't it enough that we call this from
	InterruptChannel()? Or is there a separate route here that don't pass though
	InterruptChannel()? */
	//WmSendAck(_channelID, WM_WRITE_DATA, _address);
	LOGV(WII_IPC_WIIMOTE, 0, "==========================================================");
}


// ===================================================
/* Here we produce the actual 0x21 Input report that we send to the Wii. The message
   is divided into 16 bytes pieces and sent piece by piece. There will be five formatting
   bytes at the begging of all reports. A common format is 00 00 f0 00 20, the 00 00
   means that no buttons are pressed, the f means 16 bytes in the message, the 0
   means no error, the 00 20 means that the message is at the 00 20 offest in the
   registry that was read. */
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
// ================


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
	LOGV(WII_IPC_WIIMOTE, 0, "    Rumble: %x", rs->rumble);
	LOGV(WII_IPC_WIIMOTE, 0, "    Channel: %04x", _channelID);

	//SendStatusReport();
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_STATUS_REPORT);

	wm_status_report* pStatus = (wm_status_report*)(DataFrame + Offset);
	Offset += sizeof(wm_status_report);
	memset(pStatus, 0, sizeof(wm_status_report)); // fill the status report with zeroes

	// Status values
	pStatus->battery_low = 0; // battery is okay
	pStatus->leds = g_Leds; // leds are 4 bit
	pStatus->ir = g_IR; // 1 bit
	pStatus->speaker = g_Speaker; // 1 bit
	/* Battery levels in voltage
		  0x00 - 0x32: level 1
		  0x33 - 0x43: level 2
		  0x33 - 0x54: level 3
		  0x55 - 0xff: level 4 */
	pStatus->battery = 0x5f; // fully charged

	// Read config value for this one
	if(g_Config.bNunchuckConnected || g_Config.bClassicControllerConnected)
		pStatus->extension = 1;
	else
		pStatus->extension = 0;

	LOGV(WII_IPC_WIIMOTE, 0, "    Extension: %x", pStatus->extension);
	LOGV(WII_IPC_WIIMOTE, 0, "    SendStatusReport()");
	LOGV(WII_IPC_WIIMOTE, 0, "        Flags: 0x%02x", pStatus->padding1[2]);
	LOGV(WII_IPC_WIIMOTE, 0, "        Battery: %d", pStatus->battery);

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);
	LOGV(WII_IPC_WIIMOTE, 0, "=================================================");
}

} // WiiMoteEmu
