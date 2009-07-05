// Copyright (C) 2003-2009 Dolphin Project.

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



/* Data reports guide. The different structures location in the Input
   reports. The ? in the IR coordinates is the High coordinates that are four
   in one byte.
   
   0x37: For the data reportingmode 0x37 there are five unused IR bytes in the
   end (represented) by "..." below, it seems like they can be set to either
   0xff or 0x00 without affecting the IR emulation. */

/* 0x33 [c.left etc] [c.a etc] acc.x y z ir0.x y ? ir1.x y ? ir2.x y ? ir3.x y
   ?

   0x37 [c.left etc] [c.a etc] acc.x y z ir0.x1 y1 ? x2 y2 ir1.x1 y1 ? x2 y2
   ...  ext.jx jy ax ay az bt


   The Data Report's path from here is
      WII_IPC_HLE_WiiMote.cpp:
	     Callback_WiimoteInput()
	     CWII_IPC_HLE_WiiMote::SendL2capData()
	  WII_IPC_HLE_Device_usb.cpp:
	     CWII_IPC_HLE_Device_usb_oh1_57e_305::SendACLFrame()
	  at that point the message is queued and will be sent by the next
	     CWII_IPC_HLE_Device_usb_oh1_57e_305::Update()
*/

#include "pluginspecs_wiimote.h"

#include <vector>
#include <string>

#include "Common.h" // Common
#include "StringUtil.h" // for ArrayToString

#include "wiimote_hid.h" // Local
#include "main.h"
#include "EmuMain.h"
#include "EmuSubroutines.h"
#include "EmuDefinitions.h"
#include "Encryption.h" // for extension encryption
#include "Config.h" // for g_Config


extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteEmu
{

// Subroutines


// Update the data reporting mode 

void WmDataReporting(u16 _channelID, wm_data_reporting* dr) 
{
	DEBUG_LOG(WII_IPC_WIIMOTE, " Set Data reporting mode");
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Rumble: %x", dr->rumble);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Continuous: %x", dr->continuous);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  All The Time: %x (not only on data change)", dr->all_the_time);
	DEBUG_LOG(WII_IPC_WIIMOTE, "  Mode: 0x%02x", dr->mode);
	INFO_LOG(CONSOLE, "Data reporting:\n");
	INFO_LOG(CONSOLE, "   Continuous: %x\n", dr->continuous);
	INFO_LOG(CONSOLE, "   All The Time: %x (not only on data change)\n", dr->all_the_time);
	INFO_LOG(CONSOLE, "   Mode: 0x%02x\n", dr->mode);
	INFO_LOG(CONSOLE, "   Channel: 0x%04x\n", _channelID);
	
	g_ReportingMode = dr->mode;
	g_ReportingChannel = _channelID;
	switch(dr->mode) // See Wiimote_Update()
	{
	case WM_REPORT_CORE:
	case WM_REPORT_CORE_ACCEL:
	case WM_REPORT_CORE_ACCEL_IR12:
	case WM_REPORT_CORE_ACCEL_EXT16:
	case WM_REPORT_CORE_ACCEL_IR10_EXT6:
		break;
	default:
		PanicAlert("Wiimote: Unsupported reporting mode 0x%x", dr->mode);
	}

	// WmSendAck(_channelID, WM_DATA_REPORTING);

}



/* Case 0x30: Core Buttons */
void SendReportCore(u16 _channelID) 
{
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE);

	wm_report_core* pReport = (wm_report_core*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core);
	memset(pReport, 0, sizeof(wm_report_core));
#if defined(HAVE_WX) && HAVE_WX
	FillReportInfo(pReport->c);
#endif

	INFO_LOG(WII_IPC_WIIMOTE, "  SendReportCore()");

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);

	// Debugging
	ReadDebugging(true, DataFrame, Offset);
}


/* 0x31: Core Buttons and Accelerometer */
void SendReportCoreAccel(u16 _channelID) 
{
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE_ACCEL);

	wm_report_core_accel* pReport = (wm_report_core_accel*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core_accel);
	memset(pReport, 0, sizeof(wm_report_core_accel));
	
#if defined(HAVE_WX) && HAVE_WX
	FillReportInfo(pReport->c);
	FillReportAcc(pReport->a);
#endif

	INFO_LOG(WII_IPC_WIIMOTE,  "  SendReportCoreAccel (0x31)");
	INFO_LOG(WII_IPC_WIIMOTE,  "      Channel: %04x", _channelID);
	INFO_LOG(WII_IPC_WIIMOTE,  "      Offset: %08x", Offset);

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);

	// Debugging
	ReadDebugging(true, DataFrame, Offset);
}


/* Case 0x33: Core Buttons and Accelerometer with 12 IR bytes   */
void SendReportCoreAccelIr12(u16 _channelID) {
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE_ACCEL_IR12);

	wm_report_core_accel_ir12* pReport = (wm_report_core_accel_ir12*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core_accel_ir12);
	memset(pReport, 0, sizeof(wm_report_core_accel_ir12));
	
#if defined(HAVE_WX) && HAVE_WX
	FillReportInfo(pReport->c);
	FillReportAcc(pReport->a);

	// We settle with emulating two objects, not all four. We leave object 2 and 3 with 0xff.
	FillReportIR(pReport->ir[0], pReport->ir[1]);
#endif
	memset(&pReport->ir[2], 0xff, sizeof(wm_ir_extended));
	memset(&pReport->ir[3], 0xff, sizeof(wm_ir_extended));

	INFO_LOG(WII_IPC_WIIMOTE,  "  SendReportCoreAccelIr12()");
	INFO_LOG(WII_IPC_WIIMOTE,  "    Offset: %08x", Offset);

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);

	// Debugging
	ReadDebugging(true, DataFrame, Offset);
}


/* Case 0x35: Core Buttons and Accelerometer with 16 Extension Bytes  */
void SendReportCoreAccelExt16(u16 _channelID) 
{
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE_ACCEL_EXT16);

	wm_report_core_accel_ext16* pReport = (wm_report_core_accel_ext16*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core_accel_ext16);
	memset(pReport, 0, sizeof(wm_report_core_accel_ext16));

	// Make a classic extension struct
	wm_classic_extension _ext;
	memset(&_ext, 0, sizeof(wm_classic_extension));

#if defined(HAVE_WX) && HAVE_WX
	FillReportInfo(pReport->c);
	FillReportAcc(pReport->a);
#endif

	if(g_Config.iExtensionConnected == EXT_NUNCHUCK)
	{
#if defined(HAVE_WX) && HAVE_WX
		FillReportExtension(pReport->ext);
#endif
	}
	else if(g_Config.iExtensionConnected == EXT_CLASSIC_CONTROLLER)
	{
#if defined(HAVE_WX) && HAVE_WX
		FillReportClassicExtension(_ext);
#endif
//TODO		// Copy _ext to pReport->ext
		memcpy(&pReport->ext, &_ext, sizeof(_ext));
	}

	INFO_LOG(WII_IPC_WIIMOTE,  "  SendReportCoreAccelExt16 (0x35)");
	INFO_LOG(WII_IPC_WIIMOTE,  "      Channel: %04x", _channelID);
	INFO_LOG(WII_IPC_WIIMOTE,  "      Offset: %08x", Offset);

	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);

	// Debugging
	ReadDebugging(true, DataFrame, Offset);
}


/* Case 0x37: Core Buttons and Accelerometer with 10 IR bytes and 6 Extension Bytes */
void SendReportCoreAccelIr10Ext(u16 _channelID) 
{
	u8 DataFrame[1024];
	u32 Offset = WriteWmReport(DataFrame, WM_REPORT_CORE_ACCEL_IR10_EXT6);

	wm_report_core_accel_ir10_ext6* pReport = (wm_report_core_accel_ir10_ext6*)(DataFrame + Offset);
	Offset += sizeof(wm_report_core_accel_ir10_ext6);
	memset(pReport, 0, sizeof(wm_report_core_accel_ir10_ext6));

	// Make a classic extension struct
	wm_classic_extension _ext;
	memset(&_ext, 0, sizeof(wm_classic_extension));

#if defined(HAVE_WX) && HAVE_WX
	FillReportInfo(pReport->c);
	FillReportAcc(pReport->a);
	FillReportIRBasic(pReport->ir[0], pReport->ir[1]);
#endif
//TODO
	if(g_Config.iExtensionConnected == EXT_NUNCHUCK)
	{
#if defined(HAVE_WX) && HAVE_WX
		FillReportExtension(pReport->ext);
#endif
	}
	else if(g_Config.iExtensionConnected == EXT_CLASSIC_CONTROLLER)
	{
#if defined(HAVE_WX) && HAVE_WX
		FillReportClassicExtension(_ext);
#endif
		// Copy _ext to pReport->ext
		memcpy(&pReport->ext, &_ext, sizeof(_ext));
	}

	INFO_LOG(WII_IPC_WIIMOTE,  "  SendReportCoreAccelIr10Ext()");
	
	g_WiimoteInitialize.pWiimoteInput(_channelID, DataFrame, Offset);

	// Debugging
	ReadDebugging(true, DataFrame, Offset);
}


} // end of namespace
