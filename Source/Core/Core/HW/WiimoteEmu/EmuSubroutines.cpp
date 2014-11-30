// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteEmu
{

void Spy(Wiimote* wm_, const void* data_, size_t size_)
{
// Enable for logging and debugging purposes.
#if 0
	// enable log
	bool logCom = true;
	bool logData = true;
	bool logMP = false;
	bool logAudio = false;

	std::string Name, TmpData;
	static int c;
	int size;
	u16 SampleValue;
	bool AudioData = false;
	bool DataReport = false;
	static std::queue<u32> dataRep;
	static u8 dataReply[3] = {0};
	bool emu = wm_;
	static Wiimote* wm = 0;

	// a container for f.e. the extension encryption key
	if (!wm_ && !wm)
		wm = new Wiimote(0);
	else
		wm = wm_;

	// ignore emulated Wiimote data
	if (emu) return;

	const hid_packet* const hidp = (hid_packet*)data_;
	const wm_report* const sr = (wm_report*)hidp->data;

	// use a non-pointer array because that makes read syntax shorter
	u8 data[32] = {};
	memcpy(data, data_, size_);

	switch (data[1])
	{
	case WM_RUMBLE:
		size = 1;
		if (logCom) Name.append("WM_RUMBLE");
		break;

	case WM_LEDS:
		size = sizeof(wm_leds);
		if (logCom) Name.append("WM_LEDS");
		break;

	case WM_REPORT_MODE:
		size = sizeof(wm_report_mode);
		if (logCom) Name.append("WM_REPORT_MODE");
		ERROR_LOG(CONSOLE, "WM_REPORT_MODE: 0x%02x", data[3]);
		break;

	case WM_IR_PIXEL_CLOCK:
		if (logCom) Name.append("WM_IR_PIXEL_CLOCK");
		break;

	case WM_SPEAKER_ENABLE:
		if (logCom) Name.append("WM_SPEAKER_ENABLE");
		NOTICE_LOG(CONSOLE, "Speaker on: %d", sr->enable);
		break;

	case WM_REQUEST_STATUS:
		size = sizeof(wm_request_status);
		if (logCom) Name.append("WM_REQUEST_STATUS");
		NOTICE_LOG(CONSOLE, "WM_REQUEST_STATUS: %s", ArrayToString(data, size+2, 0).c_str());
		break;

	case WM_WRITE_DATA:
	{
		if (logCom) Name.append("W 0x16");
		size = sizeof(wm_write_data);

		wm_write_data* wd = (wm_write_data*)sr->data;
		u32 address = Common::swap24(wd->address);
		address &= ~0x010000;

		switch (data[2] >> 0x01)
		{
		case WM_SPACE_EEPROM:
			if (logCom) Name.append(" REG_EEPROM"); break;
		case WM_SPACE_REGS1:
		case WM_SPACE_REGS2:
		{
			const u8 region_offset = (u8)address;
			void *region_ptr = nullptr;
			int region_size = 0;

			switch (data[3])
			{
			case 0xa2:
				if (logCom)
				{
					Name.append(" REG_SPEAKER");
					if (data[6] == 7)
					{
						//INFO_LOG(CONSOLE, "Sound configuration:");
						if (data[8] == 0x00)
						{
							//memcpy(&SampleValue, &data[9], 2);
							//NOTICE_LOG(CONSOLE, "    Data format: 4-bit ADPCM (%i Hz)", 6000000 / SampleValue);
							//NOTICE_LOG(CONSOLE, "    Volume: %02i%%", (data[11] / 0x40) * 100);
						}
						else if (data[8] == 0x40)
						{
							//memcpy(&SampleValue, &data[9], 2);
							//NOTICE_LOG(CONSOLE, "    Data format: 8-bit PCM (%i Hz)", 12000000 / SampleValue);
							//NOTICE_LOG(CONSOLE, "    Volume: %02i%%", (data[11] / 0xff) * 100);
						}
					}
				}
				break;
			case 0xa4:
				if (logCom) Name.append(" REG_EXT");
				// Update the encryption mode
				if (data[5] == 0xf0)
				{
					if (!emu)
						wm->m_reg_ext.encryption = wd->data[0];
					//NOTICE_LOG(CONSOLE, "Extension encryption: %u", wm->m_reg_ext.encryption);
				}
				region_ptr = &wm->m_reg_ext;
				break;
			case 0xa6 :
				if (logCom) Name.append(" REG_M+");
				// update the encryption mode
				if (data[5] == 0xf0)
				{
					if (!emu)
						wm->m_reg_motion_plus.activated = wd->data[0];
					//NOTICE_LOG(CONSOLE, "Extension enryption: %u", wm->m_reg_ext.encryption);
				}
				region_ptr = &wm->m_reg_motion_plus;
				break;
			case 0xb0:
				 if (logCom) Name.append(" REG_IR"); break;
			}

			// save register
			if (!emu && region_ptr)
				memcpy((u8*)region_ptr + region_offset, wd->data, wd->size);
			// save key
			if (region_offset >= 0x40 && region_offset <= 0x4c)
			{
				if (!emu)
					WiimoteGenerateKey(&wm->m_ext_key, wm->m_reg_ext.encryption_key);
				INFO_LOG(CONSOLE, "Writing key: %s", ArrayToString((u8*)&wm->m_ext_key, sizeof(wm->m_ext_key), 0, 30).c_str());
			}
			if (data[3] == 0xa4 || data[3] == 0xa6)
			{
				//DEBUG_LOG(CONSOLE, "M+: %s", ArrayToString((u8*)&wm->m_reg_motion_plus, sizeof(wm->m_reg_motion_plus), 0, 30).c_str());
				//DEBUG_LOG(CONSOLE, "M+: %s", ArrayToString((u8*)&wm->m_reg_motion_plus.ext_identifier, sizeof(wm->m_reg_motion_plus.ext_identifier), 0, 30).c_str());
				NOTICE_LOG(CONSOLE, "W[0x%02x 0x%02x|%d]: %s", data[3], region_offset,  wd->size, ArrayToString(wd->data, wd->size, 0).c_str());
			}
			break;
		}
		}

		break;
	}

	case WM_READ_DATA:
	{
		if (logCom) Name.append("R");
		size = sizeof(wm_read_data);

		wm_read_data* rd = (wm_read_data*)sr->data;
		u32 address = Common::swap24(rd->address);
		u8 addressLO = address & 0xFFFF;
		address &= 0xFEFFFF;
		u16 size = Common::swap16(rd->size);
		u8 *const block = new u8[size];
		void *region_ptr = nullptr;

		dataRep.push(((data[2]>>1)<<16) + ((data[3])<<8) + addressLO);

		switch (data[2]>>1)
		{
		case WM_SPACE_EEPROM:
			if (logCom) Name.append(" REG_EEPROM");
			if (!emu)
				memcpy(block, wm->m_eeprom + address, size);
			break;
		case WM_SPACE_REGS1:
		case WM_SPACE_REGS2:
			// ignore second byte for extension area
			if (address>>16 == 0xA4) address &= 0xFF00FF;
			const u8 region_offset = (u8)address;
			switch (data[3])
			{
			case 0xa2:
				if (logCom) Name.append(" REG_SPEAKER");
				region_ptr = &wm->m_reg_speaker;
				break;
			case 0xa4:
				 if (logCom) Name.append(" REG_EXT");
				 region_ptr = &wm->m_reg_motion_plus;
				 break;
			case 0xa6:
				 if (logCom) Name.append(" REG_M+");
				 region_ptr = &wm->m_reg_motion_plus;
				break;
			case 0xb0:
				if (logCom) Name.append(" REG_IR");
				region_ptr = &wm->m_reg_ir;
				break;
			}
			//if (!emu && region_ptr)
				//memcpy(block, (u8*)region_ptr + region_offset, size);
			//WARN_LOG(CONSOLE, "READING[0x%02x 0x%02x|%d]: %s", data[3], region_offset, size, ArrayToString(block, size, 0, 30).c_str());
			break;
		}

		break;
	}

	case WM_WRITE_SPEAKER_DATA:
		if (logCom) Name.append("WM_SPEAKER_DATA");
		size = 21;
		break;

	case WM_SPEAKER_MUTE:
		if (logCom) Name.append("WM_SPEAKER");
		size = 1;
		NOTICE_LOG(CONSOLE, "Speaker mute: %d", sr->enable);
		break;

	case WM_IR_LOGIC:
		if (logCom) Name.append("WM_IR");
		size = 1;
		break;

	case WM_STATUS_REPORT:
		size = sizeof(wm_status_report);
		Name = "WM_STATUS_REPORT";
		//INFO_LOG(CONSOLE, "WM_STATUS_REPORT: %s", ArrayToString(data, size+2, 0).c_str());
		{
			wm_status_report* pStatus = (wm_status_report*)(data + 2);
			ERROR_LOG(CONSOLE, ""
				"Statusreport extension: %i",
				//"Speaker enabled: %i"
				//"IR camera enabled: %i"
				//"LED 1: %i\n"
				//"LED 2: %i\n"
				//"LED 3: %i\n"
				//"LED 4: %i\n"
				//"Battery low: %i\n"
				//"Battery level: %i",
				pStatus->extension
				//pStatus->speaker,
				//pStatus->ir,
				//(pStatus->leds >> 0),
				//(pStatus->leds >> 1),
				//(pStatus->leds >> 2),
				//(pStatus->leds >> 3),
				//pStatus->battery_low,
				//pStatus->battery
				);
		}
		break;

	case WM_READ_DATA_REPLY:
	{
		size = sizeof(wm_read_data_reply);
		Name = "R_REPLY";

		u8 data2[32];
		memset(data2, 0, sizeof(data2));
		memcpy(data2, data, size_);
		wm_read_data_reply* const rdr = (wm_read_data_reply*)(data2 + 2);

		bool decrypted = false;
		if (!dataRep.empty())
		{
			dataReply[0] = (dataRep.front()>>16)&0x00FF;
			dataReply[1] = (dataRep.front()>>8)&0x00FF;
			dataReply[2] = dataRep.front()&0x00FF;
			dataRep.pop();
		}

		switch (dataReply[0])
		{
		case WM_SPACE_EEPROM:
			if (logCom)
				Name.append(" REG_EEPROM");
			// Wiimote calibration
			if (data[4] == 0xf0 && data[5] == 0x00 && data[6] == 0x10)
			{
				if (data[6] == 0x10)
				{
					accel_cal* calib = (accel_cal*)&rdr->data[6];
					ERROR_LOG(CONSOLE, "Wiimote calibration:");
					//SERROR_LOG(CONSOLE, "%s", ArrayToString(rdr->data, rdr->size).c_str());
					ERROR_LOG(CONSOLE, "Cal_zero.x: %i", calib->zero_g.x);
					ERROR_LOG(CONSOLE, "Cal_zero.y: %i", calib->zero_g.y);
					ERROR_LOG(CONSOLE, "Cal_zero.z: %i", calib->zero_g.z);
					ERROR_LOG(CONSOLE, "Cal_g.x: %i", calib->one_g.x);
					ERROR_LOG(CONSOLE, "Cal_g.y: %i", calib->one_g.y);
					ERROR_LOG(CONSOLE, "Cal_g.z: %i", calib->one_g.z);
					// save
					if (!emu)
						memcpy(wm->m_eeprom + 0x16, rdr->data + 6, rdr->size);
				}
			}
			break;

		case WM_SPACE_REGS1:
		case WM_SPACE_REGS2:
			switch (dataReply[1])
			{
			case 0xa2: if (logCom) Name.append(" REG_SPEAKER"); break;
			case 0xa4: if (logCom) Name.append(" REG_EXT"); break;
			case 0xa6: if (logCom) Name.append(" REG_M+"); break;
			case 0xb0: if (logCom) Name.append(" REG_IR"); break;
			}
		}

		// save key
		if (!emu && rdr->address>>8 == 0x40)
		{
			memcpy(((u8*)&wm->m_reg_ext.encryption_key), rdr->data, rdr->size+1);
			WiimoteGenerateKey(&wm->m_ext_key, wm->m_reg_ext.encryption_key);
			NOTICE_LOG(CONSOLE, "Reading key: %s", ArrayToString(((u8*)&wm->m_ext_key), sizeof(wm->m_ext_key), 0, 30).c_str());
		}

		// select decryption
		//if(((!wm->GetMotionPlusActive() && ((u8*)&wm->m_reg_ext)[0xf0] == 0xaa) || (wm->GetMotionPlusActive() && ((u8*)&wm->m_reg_motion_plus)[0xf0] == 0xaa)) && rdr->address>>8 < 0xf0) {

		//if(((((u8*)&wm->m_reg_ext)[0xf0] == 0xaa) || ((u8*)&wm->m_reg_motion_plus)[0xf0] == 0xaa) && rdr->address>>8 < 0xf0) {

		//if(!wm->GetMotionPlusActive() && ((u8*)&wm->m_reg_ext)[0xf0] == 0xaa && rdr->address>>8 < 0xf0) {

		//if(!wm->GetMotionPlusActive() && ((u8*)&wm->m_reg_ext)[0xf0] == 0xaa) {

			// SWARN_LOG(CONSOLE, "key %s", ArrayToString(((u8*)&wm->m_ext_key), sizeof(wm->m_ext_key), 0, 30).c_str());
			// SWARN_LOG(CONSOLE, "decrypt %s", ArrayToString(rdr->data, rdr->size+1, 0, 30).c_str());
			// WiimoteDecrypt(&wm->m_ext_key, rdr->data, dataReply[2]&0xffff, rdr->size+1);
			// SWARN_LOG(CONSOLE, "decrypt %s", ArrayToString(rdr->data, rdr->size+1, 0, 30).c_str());
			// decrypted = true;
		//}

		// save data
		if (!emu && !rdr->error)
		{
			//if (dataReply[1] == 0xa4 && wm->GetMotionPlusActive())
			//    memcpy(&((u8*)&wm->m_reg_motion_plus)[rdr->address>>8], rdr->data, rdr->size+1);
			//if (dataReply[1] == 0xa4 && !wm->GetMotionPlusActive())
			//if (dataReply[1] == 0xa4)
			//    memcpy(&((u8*)&wm->m_reg_ext)[rdr->address>>8], rdr->data, rdr->size+1);
			//if (!wm->GetMotionPlusActive() && wm->GetMotionPlusAttached())
			//if (dataReply[1] == 0xa6)
			//    memcpy(&((u8*)&wm->m_reg_motion_plus)[rdr->address>>8], rdr->data, rdr->size+1);
			//INFO_LOG(CONSOLE, "Saving[0x%2x:0x%2x]: %s", dataReply[1], rdr->address>>8, ArrayToString(rdr->data, rdr->size+1).c_str());
		}

		if (!rdr->error && rdr->address>>8 >= 0xf0 && rdr->address>>8 <= 0xff)
		{
			//INFO_LOG(CONSOLE, "Extension ID: %s", ArrayToString(rdr->data, rdr->size+1).c_str());
		}

		// Nunchuk calibration
		if (data[4] == 0xf0 && data[5] == 0x00 && (data[6] == 0x20 || data[6] == 0x30))
		{
			// log
			//TmpData = StringFromFormat("Read[%s] (enc): %s", (Emu ? "Emu" : "Real"), ArrayToString(data, size + 2).c_str());

			// decrypt
			//if(((u8*)&wm->m_reg_ext)[0xf0] == 0xaa) {
			//    WiimoteDecrypt(&wm->m_ext_key, &data[0x07], 0x00, (data[4] >> 0x04) + 1);

			//if (wm->m_extension->name == "NUNCHUK")
			//{
				// INFO_LOG(CONSOLE, "\nGame got the Nunchuk calibration:\n");
				// INFO_LOG(CONSOLE, "Cal_zero.x: %i\n", data[7 + 0]);
				// INFO_LOG(CONSOLE, "Cal_zero.y: %i\n", data[7 + 1]);
				// INFO_LOG(CONSOLE, "Cal_zero.z: %i\n",  data[7 + 2]);
				// INFO_LOG(CONSOLE, "Cal_g.x: %i\n", data[7 + 4]);
				// INFO_LOG(CONSOLE, "Cal_g.y: %i\n",  data[7 + 5]);
				// INFO_LOG(CONSOLE, "Cal_g.z: %i\n",  data[7 + 6]);
				// INFO_LOG(CONSOLE, "Js.Max.x: %i\n",  data[7 + 8]);
				// INFO_LOG(CONSOLE, "Js.Min.x: %i\n",  data[7 + 9]);
				// INFO_LOG(CONSOLE, "Js.Center.x: %i\n", data[7 + 10]);
				// INFO_LOG(CONSOLE, "Js.Max.y: %i\n",  data[7 + 11]);
				// INFO_LOG(CONSOLE, "Js.Min.y: %i\n",  data[7 + 12]);
				// INFO_LOG(CONSOLE, "JS.Center.y: %i\n\n", data[7 + 13]);
			//}
			//else // g_Config.bClassicControllerConnected
			//{
				// INFO_LOG(CONSOLE, "\nGame got the Classic Controller calibration:\n");
				// INFO_LOG(CONSOLE, "Lx.Max: %i\n", data[7 + 0]);
				// INFO_LOG(CONSOLE, "Lx.Min: %i\n", data[7 + 1]);
				// INFO_LOG(CONSOLE, "Lx.Center: %i\n",  data[7 + 2]);
				// INFO_LOG(CONSOLE, "Ly.Max: %i\n", data[7 + 3]);
				// INFO_LOG(CONSOLE, "Ly.Min: %i\n",  data[7 + 4]);
				// INFO_LOG(CONSOLE, "Ly.Center: %i\n",  data[7 + 5]);
				// INFO_LOG(CONSOLE, "Rx.Max.x: %i\n",  data[7 + 6]);
				// INFO_LOG(CONSOLE, "Rx.Min.x: %i\n",  data[7 + 7]);
				// INFO_LOG(CONSOLE, "Rx.Center.x: %i\n", data[7 + 8]);
				// INFO_LOG(CONSOLE, "Ry.Max.y: %i\n",  data[7 + 9]);
				// INFO_LOG(CONSOLE, "Ry.Min: %i\n",  data[7 + 10]);
				// INFO_LOG(CONSOLE, "Ry.Center: %i\n\n", data[7 + 11]);
				// INFO_LOG(CONSOLE, "Lt.Neutral: %i\n",  data[7 + 12]);
				// INFO_LOG(CONSOLE, "Rt.Neutral %i\n\n", data[7 + 13]);
			//}

			// save values
			if (!emu)
			{
				// Save to registry
				if (data[7 + 0] != 0xff)
				{
					//memcpy((u8*)&wm->m_reg_ext.calibration, &data[7], 0x10);
					//memcpy((u8*)&wm->m_reg_ext.unknown3, &data[7], 0x10);
				}
				// Save the default values that should work with Wireless Nunchuks
				else
				{
					//WiimoteEmu::SetDefaultExtensionRegistry();
				}
				//WiimoteEmu::UpdateEeprom();
			}
			// third party Nunchuk
			else if (data[7] == 0xff)
			{
				//memcpy(wm->m_reg_ext + 0x20, WiimoteEmu::wireless_nunchuk_calibration, sizeof(WiimoteEmu::wireless_nunchuk_calibration));
				//memcpy(wm->m_reg_ext + 0x30, WiimoteEmu::wireless_nunchuk_calibration, sizeof(WiimoteEmu::wireless_nunchuk_calibration));
			}

			// print encrypted data
			//INFO_LOG(CONSOLE, "WM_READ_DATA_REPLY: Extension calibration: %s", TmpData.c_str());
		}

		if (dataReply[1] == 0xa4 || dataReply[1] == 0xa6)
		{
			if (rdr->error == 7 || rdr->error == 8)
			{
				WARN_LOG(CONSOLE, "R%s[0x%02x 0x%02x]: e-%d", decrypted?"*":"", dataReply[1], rdr->address>>8, rdr->error);
			}
			else
			{
				WARN_LOG(CONSOLE, "R%s[0x%02x 0x%02x|%d]: %s", decrypted?"*":"", dataReply[1], rdr->address>>8, rdr->size+1, ArrayToString(rdr->data, rdr->size+1, 0).c_str());
			}
		}

		break;
	}

	case WM_ACK_DATA:
		size = sizeof(wm_acknowledge);
		Name = "WM_ACK_DATA";
		//INFO_LOG(CONSOLE, "ACK 0x%02x", data[4]);
		break;

	case WM_REPORT_CORE:
		size = sizeof(wm_report_core);
		DataReport = true;
		break;
	case WM_REPORT_CORE_ACCEL:
		size = sizeof(wm_report_core_accel);
		DataReport = true;
		break;
	case WM_REPORT_CORE_EXT8:
		size = sizeof(wm_report_core_accel_ir12);
		DataReport = true;
		break;
	case WM_REPORT_CORE_ACCEL_IR12:
		size = sizeof(wm_report_core_accel_ir12);
		DataReport = true;
		break;
	case WM_REPORT_CORE_EXT19:
		size = sizeof(wm_report_core_accel_ext16);
		DataReport = true;
		break;
	case WM_REPORT_CORE_ACCEL_EXT16:
		size = sizeof(wm_report_core_accel_ext16);
		DataReport = true;
		break;
	case WM_REPORT_CORE_IR10_EXT9:
		size = sizeof(wm_report_core_accel_ir10_ext6);
		DataReport = true;
		break;
	case WM_REPORT_CORE_ACCEL_IR10_EXT6:
		size = sizeof(wm_report_core_accel_ir10_ext6);
		DataReport = true;
		break;

	default:
		size = 15;
		NOTICE_LOG(CONSOLE, "Debugging[%s]: Unknown channel 0x%02x", (emu ? "E" : "R"), data[1]);
		break;
	}

	//if (DataReport && wm->GetMotionPlusActive())
	//{
	//	if (data[1] == WM_REPORT_CORE_ACCEL_IR10_EXT6)
	//		static bool extension = false;
	//	if (extension != (bool)(data[17+4]&1))
	//		ERROR_LOG(CONSOLE, "Datareport extension %d", data[17+4]&1);
	//		extension = data[17+4]&1;
	//}

	if (!DataReport && logCom && size <= 30)
	{
		ERROR_LOG_S(CONSOLE, "Com[%s] %s: %s", (emu ? "E" : "R"), Name.c_str(), ArrayToString(data, size + 2, 0).c_str());
	}

	if (logAudio && AudioData)
	{
		//DEBUG_LOG(CONSOLE, "%s: %s\n", Name.c_str(), ArrayToString(data, std::min(10,size), 0, 30).c_str());
	}

	if (DataReport && (logData || logMP))
	{
		// decrypt extension data
		//if (data[1] == 0x37 && !wm->GetMotionPlusActive())
		//if (data[1] == 0x37)
		//	WiimoteDecrypt(&wm->m_ext_key, &data[17], 0x00, 0x06);
		//if (data[1] == 0x35)
		//	WiimoteDecrypt(&wm->m_ext_key, &data[7], 0x00, 0x06);

		//if (data[1] == 0x35 || data[1] == 0x37)
		//{
		//	if (!g_DebugMP && mp->is_mp_data) return;
		//	if (!g_DebugData && !mp->is_mp_data) return;
		//}

		std::string SCore = "", SAcc = "", SIR = "", SExt = "", SExtID = "";

		wm_buttons* core = (wm_buttons*)sr->data;
		accel_cal* calib = (accel_cal*)&wm->m_eeprom[0x16];
		wm_accel* accel = (wm_accel*)&data[4];

		//SCore = StringFromFormat(
		//	"%d %d %d %d ",
		//	core->xL,
		//	core->yL,
		//	core->zL,
		//	core->unknown);

		SAcc = StringFromFormat(
			//"%3d %3d %3d"
			//" | %3d %3d %3d"
			//" | %3d %3d %3d"
			"| %5.2f %5.2f %5.2f"
			//, calib->zero_g.x, calib->zero_g.y, calib->zero_g.z
			//, (calib->zero_g.x<<2) + calib->zero_g.xL, (calib->zero_g.y<<2) + calib->zero_g.yL, (calib->zero_g.z<<2) + calib->zero_g.zL
			//, calib->one_g.x, calib->one_g.y, calib->one_g.z
			//, (calib->one_g.x<<2) + calib->one_g.xL, (calib->one_g.y<<2) + calib->one_g.yL, (calib->one_g.z<<2) + calib->one_g.zL
			//, accel->x, accel->y, accel->z
			//, (accel->x<<2) + core->xL, (accel->y<<2) + core->yL, (accel->z<<2) + core->zL
			, (accel->x - calib->zero_g.x) / float(calib->one_g.x-calib->zero_g.x), (accel->y - calib->zero_g.y) / float(calib->one_g.y-calib->zero_g.y), (accel->z - calib->zero_g.z) / float(calib->one_g.z-calib->zero_g.z));

		NOTICE_LOG(CONSOLE, "%d", size);

		if (data[1] == WM_REPORT_CORE_ACCEL_IR12)
		{
			wm_ir_extended *ir = (wm_ir_extended*)&data[7];

			SIR = StringFromFormat(
				"%4u %4u | %u"
				, ir->x | ir->xhi << 8
				, ir->y | ir->yhi << 8
				, ir->size);
		}

		if (data[1] == WM_REPORT_CORE_ACCEL_EXT16)
		{
			wm_nc *nc = (wm_nc*)&data[7];

			SExt = StringFromFormat(
				"%02x %02x | %02x %02x %02x | %02x"
				, nc->jx, nc->jy
				, nc->ax, nc->ay, nc->az
				, nc->bt);
		}

		if (data[1] == WM_REPORT_CORE_ACCEL_IR10_EXT6)
		{
			wm_ir_basic *ir = (wm_ir_basic*)&data[7];

			SIR = StringFromFormat(
				"%4u %4u %4u %4u"
				, ir->x1 | ir->x1hi << 8
				, ir->y1 | ir->y1hi << 8
				, ir->x2 | ir->x2hi << 8
				, ir->y2 | ir->y1hi << 8);

			/*
			wm_motionplus *mp = (wm_motionplus*)&data[17];
			wm_nc_mp  *nc_mp = (wm_nc_mp *)&data[17];

			if (mp->is_mp_data)
			{
				SExt = StringFromFormat(""
					//"%02x %02x %02x %02x %02x %02x"
					//"| %04x %04x %04x
					" %5.2f %5.2f %5.2f"
					" %s%s%s"
					//, mp->roll1, mp->roll2
					//, mp->pitch1, mp->pitch2
					//, mp->yaw1, mp->yaw2
					//, mp->pitch2<<8 | mp->pitch1
					//, mp->roll2<<8 | mp->roll1
					//, mp->yaw2<<8 | mp->yaw1
					//, mp->pitch2<<8 | mp->pitch1
					//, mp->roll2<<8 | mp->roll1
					//, mp->yaw2<<8 | mp->yaw1
					, float((mp->pitch2<<8 | mp->pitch1) - 0x1f7f) / float(0x1fff)
					, float((mp->roll2<<8 | mp->roll1) - 0x1f7f) / float(0x1fff)
					, float((mp->yaw2<<8 | mp->yaw1) - 0x1f7f) / float(0x1fff)
					, mp->pitch_slow?"*":" ", mp->roll_slow?"*":" ", mp->yaw_slow?"*":" ");
			}
			else
			{
				SExt = StringFromFormat(
					"%02x %02x | %02x %02x | %02x %02x %02x | %02x %02x %02x",
					nc_mp->bz, nc_mp->bc,
					nc_mp->jx, nc_mp->jy,
					nc_mp->ax+nc_mp->axL, nc_mp->ay+nc_mp->ayL, (nc_mp->az<<1)+nc_mp->azL);
			}

			SExtID = StringFromFormat(
				"[%s|%d|%d]"
				, mp->is_mp_data ? "+" : "e"
				, mp->is_mp_data ? mp->extension_connected : nc_mp->extension_connected
				, wm->m_extension->active_extension);
			*/

			//DEBUG_LOG_S(CONSOLE, "M+ %d Extension %d %d %s", mp->is_mp_data, mp->is_mp_data ?
			//		mp->extension_connected : ((wm_nc_mp*)&data[17])->extension_connected, wm->m_extension->active_extension,
			//		ArrayToString(((u8*)&wm->m_reg_motion_plus.ext_identifier), sizeof(wm->m_reg_motion_plus.ext_identifier), 0).c_str());
		}

		// select log data
		WARN_LOG_S(CONSOLE, "Data"
			"[%s]"
			" | id %s"
			" | %s"
			" | c %s"
			" | a %s"
			" | ir %s"
			" | ext %s"
			//" | %s"
			//" | %s"
			//" | %s"
			, (emu ? "E" : "R")
			, SExtID.c_str()
			, ArrayToString(data, 2, 0).c_str()
			, SCore.c_str()
			, SAcc.c_str()
			, SIR.c_str()
			, SExt.c_str()
			//, ArrayToString(&data[4], 3, 0).c_str()
			//, (accel->x - 0x7f) / float(0xff), (accel->y - 0x7f) / float(0xff), (accel->z - 0x7f) / float(0xff)
			//, ArrayToString(&data[17], 6, 0).c_str(),
			);
	}
#endif
}

void Wiimote::ReportMode(const wm_report_mode* const dr)
{
	//INFO_LOG(WIIMOTE, "Set data report mode");
	//DEBUG_LOG(WIIMOTE, "  Rumble: %x", dr->rumble);
	//DEBUG_LOG(WIIMOTE, "  Continuous: %x", dr->continuous);
	//DEBUG_LOG(WIIMOTE, "  All The Time: %x", dr->all_the_time);
	//DEBUG_LOG(WIIMOTE, "  Mode: 0x%02x", dr->mode);

	//m_reporting_auto = dr->all_the_time;
	m_reporting_auto = dr->continuous;	// this right?
	m_reporting_mode = dr->mode;
	//m_reporting_channel = _channelID;	// this is set in every Interrupt/Control Channel now

	// reset IR camera
	//memset(m_reg_ir, 0, sizeof(*m_reg_ir));  //ugly hack

	if (dr->mode > 0x37)
		PanicAlert("Wiimote: Unsupported Reporting mode.");
	else if (dr->mode < WM_REPORT_CORE)
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
	INFO_LOG(WIIMOTE, "HidOutputReport (page: %i, cid: 0x%02x, wm: 0x%02x)", m_index, m_reporting_channel, sr->wm);

	// wiibrew:
	// In every single Output Report, bit 0 (0x01) of the first byte controls the Rumble feature.
	m_rumble_on = sr->rumble;

	switch (sr->wm)
	{
	case WM_RUMBLE : // 0x10
		// this is handled above
		return;	// no ack
		break;

	case WM_LEDS : // 0x11
		//INFO_LOG(WIIMOTE, "Set LEDs: 0x%02x", sr->data[0]);
		m_status.leds = sr->data[0] >> 4;
		break;

	case WM_REPORT_MODE :  // 0x12
		ReportMode((wm_report_mode*)sr->data);
		break;

	case WM_IR_PIXEL_CLOCK : // 0x13
		//INFO_LOG(WIIMOTE, "WM IR Clock: 0x%02x", sr->data[0]);
		//m_ir_clock = sr->enable;
		if (false == sr->ack)
			return;
		break;

	case WM_SPEAKER_ENABLE : // 0x14
		//ERROR_LOG(WIIMOTE, "WM Speaker Enable: %02x", sr->enable);
		//PanicAlert( "WM Speaker Enable: %d", sr->data[0] );
		m_status.speaker = sr->enable;
		if (false == sr->ack)
			return;
		break;

	case WM_REQUEST_STATUS : // 0x15
		if (WIIMOTE_SRC_EMU & g_wiimote_sources[m_index])
			RequestStatus((wm_request_status*)sr->data);
		return;	// sends its own ack
		break;

	case WM_WRITE_DATA : // 0x16
		WriteData((wm_write_data*)sr->data);
		break;

	case WM_READ_DATA : // 0x17
		if (WIIMOTE_SRC_EMU & g_wiimote_sources[m_index])
			ReadData((wm_read_data*)sr->data);
		return;	// sends its own ack
		break;

	case WM_WRITE_SPEAKER_DATA : // 0x18
		//wm_speaker_data *spkz = (wm_speaker_data*)sr->data;
		//ERROR_LOG(WIIMOTE, "WM_WRITE_SPEAKER_DATA len:%x %s", spkz->length,
		//	ArrayToString(spkz->data, spkz->length, 100, false).c_str());
		if (WIIMOTE_SRC_EMU & g_wiimote_sources[m_index])
			Wiimote::SpeakerData((wm_speaker_data*) sr->data);
		return;	// no ack
		break;

	case WM_SPEAKER_MUTE : // 0x19
		//ERROR_LOG(WIIMOTE, "WM Speaker Mute: %02x", sr->enable);
		//PanicAlert( "WM Speaker Mute: %d", sr->data[0] & 0x04 );
		// testing
		//if (sr->data[0] & 0x04)
		//	memset(&m_channel_status, 0, sizeof(m_channel_status));
		m_speaker_mute = sr->enable;
		if (false == sr->ack)
			return;
		break;

	case WM_IR_LOGIC: // 0x1a
		// comment from old plugin:
		// This enables or disables the IR lights, we update the global variable g_IR
		// so that WmRequestStatus() knows about it
		//INFO_LOG(WIIMOTE, "WM IR Enable: 0x%02x", sr->data[0]);
		m_status.ir = sr->enable;
		if (false == sr->ack)
			return;
		break;

	default:
		PanicAlert("HidOutputReport: Unknown channel 0x%02x", sr->wm);
		return; // no ack
		break;
	}

	// send ack
	if (send_ack && WIIMOTE_SRC_EMU & g_wiimote_sources[m_index])
		SendAck(sr->wm);
}

/* This will generate the 0x22 acknowledgement for most Input reports.
   It has the form of "a1 22 00 00 _reportID 00".
   The first two bytes are the core buttons data,
   00 00 means nothing is pressed.
   The last byte is the success code 00. */
void Wiimote::SendAck(u8 _reportID)
{
	u8 data[6];

	data[0] = 0xA1;
	data[1] = WM_ACK_DATA;

	wm_acknowledge* const ack = (wm_acknowledge*)(data + 2);

	ack->buttons = m_status.buttons;
	ack->reportID = _reportID;
	ack->errorID = 0;

	Core::Callback_WiimoteInterruptChannel( m_index, m_reporting_channel, data, sizeof(data));
}

void Wiimote::HandleExtensionSwap()
{
	// handle switch extension
	if (m_extension->active_extension != m_extension->switch_extension)
	{
		// if an extension is currently connected and we want to switch to a different extension
		if ((m_extension->active_extension > 0) && m_extension->switch_extension)
			// detach extension first, wait til next Update() or RequestStatus() call to change to the new extension
			m_extension->active_extension = 0;
		else
			// set the wanted extension
			m_extension->active_extension = m_extension->switch_extension;

		// reset register
		((WiimoteEmu::Attachment*)m_extension->attachments[m_extension->active_extension].get())->Reset();
	}
}

// old comment
/* Here we produce a 0x20 status report to send to the Wii. We currently ignore
   the status request rs and all its eventual instructions it may include (for
   example turn off rumble or something else) and just send the status
   report. */
void Wiimote::RequestStatus(const wm_request_status* const rs)
{
	HandleExtensionSwap();

	// update status struct
	m_status.extension = (m_extension->active_extension || m_motion_plus_active) ? 1 : 0;

	// set up report
	u8 data[8];
	data[0] = 0xA1;
	data[1] = WM_STATUS_REPORT;

	// status values
	*(wm_status_report*)(data + 2) = m_status;

	// hybrid wiimote stuff
	if (WIIMOTE_SRC_REAL & g_wiimote_sources[m_index] && (m_extension->switch_extension <= 0))
	{
		using namespace WiimoteReal;

		std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

		if (g_wiimotes[m_index])
		{
			wm_request_status rpt = {};
			g_wiimotes[m_index]->QueueReport(WM_REQUEST_STATUS, &rpt, sizeof(rpt));
		}

		return;
	}

	// send report
	Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, data, sizeof(data));
}

/* Write data to Wiimote and Extensions registers. */
void Wiimote::WriteData(const wm_write_data* const wd)
{
	u32 address = Common::swap24(wd->address);

	// ignore the 0x010000 bit
	address &= ~0x010000;

	if (wd->size > 16)
	{
		PanicAlert("WriteData: size is > 16 bytes");
		return;
	}

	switch (wd->space)
	{
	case WM_SPACE_EEPROM :
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
			// i need to improve this greatly
			if (address >= 0x0FCA && address < 0x12C0)
			{
				// writing the whole mii block each write :/
				std::ofstream file;
				OpenFStream(file, File::GetUserPath(D_WIIUSER_IDX) + "mii.bin", std::ios::binary | std::ios::out);
				file.write((char*)m_eeprom + 0x0FCA, 0x02f0);
				file.close();
			}
		}
		break;

	case WM_SPACE_REGS1 :
	case WM_SPACE_REGS2 :
		{
			// Write to Control Register

			// ignore second byte for extension area
			if (0xA4 == (address >> 16))
				address &= 0xFF00FF;

			const u8 region_offset = (u8)address;
			void *region_ptr = nullptr;
			int region_size = 0;

			switch (address >> 16)
			{
			// speaker
			case 0xa2 :
				region_ptr = &m_reg_speaker;
				region_size = WIIMOTE_REG_SPEAKER_SIZE;
				break;

			// extension register
			case 0xa4 :
				region_ptr = m_motion_plus_active ? (void*)&m_reg_motion_plus : (void*)&m_reg_ext;
				region_size = WIIMOTE_REG_EXT_SIZE;
				break;

			// motion plus
			case 0xa6 :
				if (false == m_motion_plus_active)
				{
					region_ptr = &m_reg_motion_plus;
					region_size = WIIMOTE_REG_EXT_SIZE;
				}
				break;

			// ir
			case 0xB0 :
				region_ptr = &m_reg_ir;
				region_size = WIIMOTE_REG_IR_SIZE;

				//if (5 == m_reg_ir->mode)
				//	PanicAlert("IR Full Mode is Unsupported!");
				break;
			}

			if (region_ptr && (region_offset + wd->size <= region_size))
			{
				memcpy((u8*)region_ptr + region_offset, wd->data, wd->size);
			}
			else
				return;	// TODO: generate a writedata error reply

			/* TODO?
			if (region_ptr == &m_reg_speaker)
			{
				ERROR_LOG(WIIMOTE, "Write to speaker register %x %s", address,
					ArrayToString(wd->data, wd->size, 100, false).c_str());
			}
			*/

			if (&m_reg_ext == region_ptr)
			{
				// Run the key generation on all writes in the key area, it doesn't matter
				// that we send it parts of a key, only the last full key will have an effect
				if (address >= 0xa40040 && address <= 0xa4004c)
					WiimoteGenerateKey(&m_ext_key, m_reg_ext.encryption_key);
			}
			else if (&m_reg_motion_plus == region_ptr)
			{
				// activate/deactivate motion plus
				if (0x55 == m_reg_motion_plus.activated)
				{
					// maybe hacky
					m_reg_motion_plus.activated = 0;
					m_motion_plus_active ^= 1;

					RequestStatus();
				}
			}
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
	u32 address = Common::swap24(rd->address);
	u16 size = Common::swap16(rd->size);

	// ignore the 0x010000 bit
	address &= 0xFEFFFF;

	// hybrid wiimote stuff
	// relay the read data request to real-wiimote
	if (WIIMOTE_SRC_REAL & g_wiimote_sources[m_index] && ((0xA4 != (address >> 16)) || (m_extension->switch_extension <= 0)))
	{
		WiimoteReal::InterruptChannel(m_index, m_reporting_channel, ((u8*)rd) - 2, sizeof(wm_read_data) + 2); // hacky

		// don't want emu-wiimote to send reply
		return;
	}

	ReadRequest rr;
	u8 *const block = new u8[size];

	switch (rd->space)
	{
	case WM_SPACE_EEPROM :
		{
			//PanicAlert("ReadData: reading from EEPROM: address: 0x%x size: 0x%x", address, size);
			// Read from EEPROM
			if (address + size >= WIIMOTE_EEPROM_FREE_SIZE)
			{
				if (address + size > WIIMOTE_EEPROM_SIZE)
				{
					PanicAlert("ReadData: address + size out of bounds");
					delete [] block;
					return;
				}
				// generate a read error
				size = 0;
			}

			// read mii data from file
			// i need to improve this greatly
			if (address >= 0x0FCA && address < 0x12C0)
			{
				// reading the whole mii block :/
				std::ifstream file;
				file.open((File::GetUserPath(D_WIIUSER_IDX) + "mii.bin").c_str(), std::ios::binary | std::ios::in);
				file.read((char*)m_eeprom + 0x0FCA, 0x02f0);
				file.close();
			}

			// read memory to be sent to Wii
			memcpy(block, m_eeprom + address, size);
		}
		break;

	case WM_SPACE_REGS1 :
	case WM_SPACE_REGS2 :
		{
			// Read from Control Register

			// ignore second byte for extension area
			if (0xA4 == (address >> 16))
				address &= 0xFF00FF;

			const u8 region_offset = (u8)address;
			void *region_ptr = nullptr;
			int region_size = 0;

			switch (address >> 16)
			{
			// speaker
			case 0xa2:
				region_ptr = &m_reg_speaker;
				region_size = WIIMOTE_REG_SPEAKER_SIZE;
				break;

			// extension
			case 0xa4:
				region_ptr = m_motion_plus_active ? (void*)&m_reg_motion_plus : (void*)&m_reg_ext;
				region_size = WIIMOTE_REG_EXT_SIZE;
				break;

			// motion plus
			case 0xa6:
				// reading from 0xa6 returns error when mplus is activated
				if (false == m_motion_plus_active)
				{
					region_ptr = &m_reg_motion_plus;
					region_size = WIIMOTE_REG_EXT_SIZE;
				}
				break;

			// ir
			case 0xb0:
				region_ptr = &m_reg_ir;
				region_size = WIIMOTE_REG_IR_SIZE;
				break;
			}

			if (region_ptr && (region_offset + size <= region_size))
			{
				memcpy(block, (u8*)region_ptr + region_offset, size);
			}
			else
				size = 0;	// generate read error

			if (&m_reg_ext == region_ptr)
			{
				// Encrypt data read from extension register
				// Check if encrypted reads is on
				if (0xaa == m_reg_ext.encryption)
					WiimoteEncrypt(&m_ext_key, block, address & 0xffff, (u8)size);
			}
		}
		break;

	default :
		PanicAlert("WmReadData: unimplemented parameters (size: %i, address: 0x%x)!", size, rd->space);
		break;
	}

	// want the requested address, not the above modified one
	rr.address = Common::swap24(rd->address);
	rr.size = size;
	//rr.channel = _channelID;
	rr.position = 0;
	rr.data = block;

	// send up to 16 bytes
	SendReadDataReply(rr);

	// if there is more data to be sent, add it to the queue
	if (rr.size)
		m_read_requests.push( rr );
	else
		delete[] rr.data;
}

// old comment
/* Here we produce the actual 0x21 Input report that we send to the Wii. The
   message is divided into 16 bytes pieces and sent piece by piece. There will
   be five formatting bytes at the begging of all reports. A common format is
   00 00 f0 00 20, the 00 00 means that no buttons are pressed, the f means 16
   bytes in the message, the 0 means no error, the 00 20 means that the message
   is at the 00 20 offest in the registry that was read.
*/
void Wiimote::SendReadDataReply(ReadRequest& _request)
{
	u8 data[23];
	data[0] = 0xA1;
	data[1] = WM_READ_DATA_REPLY;

	wm_read_data_reply* const reply = (wm_read_data_reply*)(data + 2);
	reply->buttons = m_status.buttons;
	reply->address = Common::swap16(_request.address);

	// generate a read error
	// Out of bounds. The real Wiimote generate an error for the first
	// request to 0x1770 if we dont't replicate that the game will never
	// read the calibration data at the beginning of Eeprom. I think this
	// error is supposed to occur when we try to read above the freely
	// usable space that ends at 0x16ff.
	if (0 == _request.size)
	{
		reply->size = 0x0f;
		reply->error = 0x08;

		memset(reply->data, 0, sizeof(reply->data));
	}
	else
	{
		// Limit the amt to 16 bytes
		// AyuanX: the MTU is 640B though... what a waste!
		const int amt = std::min( (unsigned int)16, _request.size );

		// no error
		reply->error = 0;

		// 0x1 means two bytes, 0xf means 16 bytes
		reply->size = amt - 1;

		// Clear the mem first
		memset(reply->data, 0, sizeof(reply->data));

		// copy piece of mem
		memcpy(reply->data, _request.data + _request.position, amt);

		// update request struct
		_request.size -= amt;
		_request.position += amt;
		_request.address += amt;
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
	p.Do(m_motion_plus_present);
	p.Do(m_motion_plus_active);
	p.Do(m_reporting_auto);
	p.Do(m_reporting_mode);
	p.Do(m_reporting_channel);
	p.Do(m_shake_step);
	p.Do(m_sensor_bar_on_top);
	p.Do(m_status);
	p.Do(m_adpcm_state);
	p.Do(m_ext_key);
	p.DoArray(m_eeprom, sizeof(m_eeprom));
	p.Do(m_reg_motion_plus);
	p.Do(m_reg_ir);
	p.Do(m_reg_ext);
	p.Do(m_reg_speaker);

	//Do 'm_read_requests' queue
	{
		u32 size = 0;
		if (p.mode == PointerWrap::MODE_READ)
		{
			//clear
			while (!m_read_requests.empty())
				m_read_requests.pop();

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
