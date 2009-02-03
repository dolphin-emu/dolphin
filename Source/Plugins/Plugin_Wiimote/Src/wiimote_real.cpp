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


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯
#include <iostream> // System
#include "pluginspecs_wiimote.h"

#include "wiiuse.h"
#include <queue>

#include "Common.h"
#include "Thread.h"
#include "StringUtil.h"
#include "ConsoleWindow.h"
#include "Timer.h"

#include "wiimote_hid.h"
#include "main.h"
#include "Config.h"
#include "EmuMain.h"
#include "EmuDefinitions.h"
#define EXCLUDE_H // Avoid certain declarations in wiimote_real.h
#include "wiimote_real.h"
#if defined(HAVE_WX) && HAVE_WX
	#include "ConfigDlg.h"
#endif

extern SWiimoteInitialize g_WiimoteInitialize;
////////////////////////////////////////


namespace WiiMoteReal
{

//******************************************************************************
// Forwarding
//******************************************************************************
	
class CWiiMote;

#ifdef _WIN32
	DWORD WINAPI ReadWiimote_ThreadFunc(void* arg);
#else
	void* ReadWiimote_ThreadFunc(void* arg);
#endif
//******************************************************************************
// Variable declarations
//******************************************************************************
		
wiimote_t**			g_WiiMotesFromWiiUse = NULL;
Common::Thread*		g_pReadThread = NULL;
int					g_NumberOfWiiMotes;
CWiiMote*			g_WiiMotes[MAX_WIIMOTES];	
bool				g_Shutdown = false;
bool				g_LocalThread = true;
bool				g_IRSensing = false;
bool				g_MotionSensing = false;
u64					g_UpdateTime = 0;
int					g_UpdateCounter = 0;
bool				g_RunTemporary = false;
u8					g_EventBuffer[32];

//******************************************************************************
// Probably this class should be in its own file
//******************************************************************************

class CWiiMote
{
public:

//////////////////////////////////////////
// On create and on uncreate
// ---------------
CWiiMote(u8 _WiimoteNumber, wiimote_t* _pWiimote) 
    : m_WiimoteNumber(_WiimoteNumber)
    , m_channelID(0)
    , m_pWiiMote(_pWiimote)
    , m_pCriticalSection(NULL)
    , m_LastReportValid(false)
{
    m_pCriticalSection = new Common::CriticalSection();

    //wiiuse_set_leds(m_pWiiMote, WIIMOTE_LED_4);

	#ifdef _WIN32
		// F|RES: i dunno if we really need this
		CancelIo(m_pWiiMote->dev_handle);
	#endif
}

virtual ~CWiiMote() 
{
    delete m_pCriticalSection;
};
//////////////////////


//////////////////////////////////////////
// Queue raw HID data from the core to the wiimote
// ---------------
void SendData(u16 _channelID, const u8* _pData, u32 _Size)
{
    m_channelID = _channelID;

    m_pCriticalSection->Enter();
    {
        SEvent WriteEvent;
        memcpy(WriteEvent.m_PayLoad, _pData + 1, _Size - 1);
        m_EventWriteQueue.push(WriteEvent);

		// Debugging
		//std::string Temp = ArrayToString(WriteEvent.m_PayLoad, 28, 0, 30);
		//Console::Print("Wiimote Write:\n%s\n", Temp.c_str());
    }
    m_pCriticalSection->Leave();
}
/////////////////////


//////////////////////////////////////////////////
/* Read and write data to the Wiimote */
// ---------------
void ReadData() 
{
    m_pCriticalSection->Enter();

	// Send data to the Wiimote
    if (!m_EventWriteQueue.empty())
    {
		//Console::Print("Writing data to the Wiimote\n");
        SEvent& rEvent = m_EventWriteQueue.front();
        wiiuse_io_write(m_pWiiMote, (byte*)rEvent.m_PayLoad, MAX_PAYLOAD);
        m_EventWriteQueue.pop();

#ifdef _WIN32
		// Debugging. Move the data one step to the right first.
		memcpy(rEvent.m_PayLoad + 1, rEvent.m_PayLoad, sizeof(rEvent.m_PayLoad) - 1);
		rEvent.m_PayLoad[0] = 0xa2;
		InterruptDebugging(false, rEvent.m_PayLoad);
#endif
    }

    m_pCriticalSection->Leave();

	// Read data from wiimote (but don't send it to the core, just filter and queue)
	if (wiiuse_io_read(m_pWiiMote))
	{
		const byte* pBuffer = m_pWiiMote->event_buf;

		// Check if we have a channel (connection) if so save the data...
		if (m_channelID > 0)
		{
			m_pCriticalSection->Enter();

			// Filter out data reports
			if (pBuffer[0] >= 0x30) 
			{
				// Copy Buffer to LastReport
				memcpy(m_LastReport.m_PayLoad, pBuffer, MAX_PAYLOAD);
				m_LastReportValid = true;

				/* Check if the data reporting mode is okay. This should not cause any harm outside the dual mode
				   (being able to switch between the real and emulated wiimote) because WiiMoteEmu::g_ReportingMode
				   should always have the right reporting mode. */
				if (g_EmulatorRunning && pBuffer[0] != WiiMoteEmu::g_ReportingMode)
					SetDataReportingMode();
			}
			else
			{
				// Copy Buffer to ImportantEvent
				SEvent ImportantEvent;
				memcpy(ImportantEvent.m_PayLoad, pBuffer, MAX_PAYLOAD);

				// Put it in the read queue right away
				m_EventReadQueue.push(ImportantEvent);
			}
			m_pCriticalSection->Leave();
		}
#ifdef _WIN32
		/* Debugging
		//if(GetAsyncKeyState('V'))
		{
			std::string Temp = ArrayToString(pBuffer, 20, 0, 30);
			Console::Print("Data: %s\n", Temp.c_str());
		} */
#endif
	}
};
/////////////////////


//////////////////////////////////////////
// Send queued data to the core
// ---------------
void Update() 
{
	// Thread function
    m_pCriticalSection->Enter();

    if (m_EventReadQueue.empty())
    {
		// Send the data report
        if (m_LastReportValid) SendEvent(m_LastReport);
    }
    else
    {
		// Send a 0x20, 0x21 or 0x22 report
        SendEvent(m_EventReadQueue.front());
        m_EventReadQueue.pop();
    }

    m_pCriticalSection->Leave();
};
/////////////////////


//////////////////////////////////////////
// Clear events
// ---------------
void ClearEvents()
{
	while (!m_EventReadQueue.empty())
		m_EventReadQueue.pop();
	while (!m_EventWriteQueue.empty())
		m_EventWriteQueue.pop();	
}
/////////////////////

private:

    struct SEvent 
    {
        SEvent()
        {
            memset(m_PayLoad, 0, MAX_PAYLOAD);
        }
        byte m_PayLoad[MAX_PAYLOAD];
    };
    typedef std::queue<SEvent> CEventQueue;

    u8 m_WiimoteNumber; // Just for debugging
    u16 m_channelID;  
    CEventQueue m_EventReadQueue; // Read from Wiimote
    CEventQueue m_EventWriteQueue; // Write to Wiimote
    Common::CriticalSection* m_pCriticalSection;
    bool m_LastReportValid;
    SEvent m_LastReport;
	wiimote_t* m_pWiiMote; // This is g_WiiMotesFromWiiUse[]

//////////////////////////////////////////
// Send queued data to the core
// ---------------
void SendEvent(SEvent& _rEvent)
{
    // We don't have an answer channel
    if (m_channelID == 0) return;

    // Check event buffer
    u8 Buffer[1024];
    u32 Offset = 0;
    hid_packet* pHidHeader = (hid_packet*)(Buffer + Offset);
    Offset += sizeof(hid_packet);
    pHidHeader->type = HID_TYPE_DATA;
    pHidHeader->param = HID_PARAM_INPUT;

	// Create the buffer
    memcpy(&Buffer[Offset], _rEvent.m_PayLoad, MAX_PAYLOAD);
    Offset += MAX_PAYLOAD;

	// Send it
	g_WiimoteInitialize.pWiimoteInput(m_channelID, Buffer, Offset);

	// Debugging
	ReadDebugging(false, Buffer);  
}
/////////////////////
};



//******************************************************************************
// Function Definitions 
//******************************************************************************

void SendAcc(u8 _ReportID)
{
	byte DataAcc[MAX_PAYLOAD];

	DataAcc[0] = 0x22; // Report 0x12
	DataAcc[1] = 0x00; // Core buttons
	DataAcc[2] = 0x00;
	DataAcc[3] = _ReportID; // Reporting mode

	wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)DataAcc, MAX_PAYLOAD);

	std::string Temp = ArrayToString(DataAcc, 28, 0, 30);
	Console::Print("SendAcc: %s\n", Temp.c_str());

	//22 00 00 _reportID 00
}

// Clear any potential queued events
void ClearEvents()
{
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
		g_WiiMotes[i]->ClearEvents();
}

// Update the data reporting mode if we are switching between the emulated and the real Wiimote
void SetDataReportingMode(u8 ReportingMode)
{
	// Don't allow this to run to often
	if (Common::Timer::GetTimeSinceJan1970() == g_UpdateTime) return;

	// Save the time
	g_UpdateTime = Common::Timer::GetTimeSinceJan1970();

	// Decide if we should we use custom values
	if (ReportingMode == 0) ReportingMode = WiiMoteEmu::g_ReportingMode;

	// Just in case this should happen
	if (ReportingMode == 0) ReportingMode = 0x30;

	// Shortcut
	wiimote_t* wm = WiiMoteReal::g_WiiMotesFromWiiUse[0];

	switch(ReportingMode) // See Wiimote_Update()
	{
	case WM_REPORT_CORE:
		wiiuse_motion_sensing(wm, 0);
		wiiuse_set_ir(wm, 0);
		break;
	case WM_REPORT_CORE_ACCEL:
		wiiuse_motion_sensing(wm, 1);
		wiiuse_set_ir(wm, 0);
		break;
	case WM_REPORT_CORE_ACCEL_IR12:
		wiiuse_motion_sensing(wm, 1);
		wiiuse_set_ir(wm, 1);
		break;
	case WM_REPORT_CORE_ACCEL_EXT16:
		wiiuse_motion_sensing(wm, 1);
		wiiuse_set_ir(wm, 0);
		break;
	case WM_REPORT_CORE_ACCEL_IR10_EXT6:
		wiiuse_motion_sensing(wm, 1);
		wiiuse_set_ir(wm, 1);
		break;
	}

	/* On occasion something in this function caused an instant reboot of Windows XP SP3
	   with Bluesoleil 6. So I'm trying to use the API functions to reach the same goal. */
	/*byte DataReporting[MAX_PAYLOAD];
	byte IR0[MAX_PAYLOAD];
	byte IR1[MAX_PAYLOAD];

	DataReporting[0] = 0x12; // Report 0x12
	DataReporting[1] = 0x06; // Continuous reporting
	DataReporting[2] = ReportingMode; // Reporting mode

	IR0[0] = 0x13; // Report 0x13
	if (IR) IR0[1] = 0x06; else IR0[1] = 0x02;
	IR1[0] = 0x1a; // Report 0x1a
	if (IR) IR1[1] = 0x06; else IR1[1] = 0x02;

	// Calibrate IR
	static const u8 IR_0[] = { 0x16, 0x04, 0xb0, 0x00, 0x30, 0x01,
		0x01 };
	static const u8 IR_1[] = { 0x16, 0x04, 0xb0, 0x00, 0x00, 0x09,
		0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xaa, 0x00, 0x64 };
	static const u8 IR_2[] = { 0x16, 0x04, 0xb0, 0x00, 0x1a, 0x02,
		0x63, 0x03 };
	static const u8 IR_3[] = { 0x16, 0x04, 0xb0, 0x00, 0x33, 0x01,
		0x03 };
	static const u8 IR_4[] = { 0x16, 0x04, 0xb0, 0x00, 0x30, 0x01,
		0x08 };

	wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)DataReporting, MAX_PAYLOAD);
	wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)IR0, MAX_PAYLOAD);
	wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)IR1, MAX_PAYLOAD);

	if (IR)
	{
		wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)IR_0, MAX_PAYLOAD);
		wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)IR_1, MAX_PAYLOAD);
		wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)IR_2, MAX_PAYLOAD);
		wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)IR_3, MAX_PAYLOAD);
		wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)IR_4, MAX_PAYLOAD);
	}*/
}

// Flash lights, and if connecting, also rumble
void FlashLights(bool Connect)
{
	if(Connect) wiiuse_rumble(WiiMoteReal::g_WiiMotesFromWiiUse[0], 1);
	wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_1 | WIIMOTE_LED_2 | WIIMOTE_LED_3 | WIIMOTE_LED_4);
	Sleep(100);
	if(Connect) wiiuse_rumble(WiiMoteReal::g_WiiMotesFromWiiUse[0], 0);

	// End with light 1 or 4
	if(Connect)
		wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_1);
	else
		wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_4);
}

int Initialize()
{
	// Return if already initialized
	if (g_RealWiiMoteInitialized) return g_NumberOfWiiMotes;

	// Clear the wiimote classes
    memset(g_WiiMotes, 0, sizeof(CWiiMote*) * MAX_WIIMOTES);

	// Call Wiiuse.dll
    g_WiiMotesFromWiiUse = wiiuse_init(MAX_WIIMOTES);
    g_NumberOfWiiMotes = wiiuse_find(g_WiiMotesFromWiiUse, MAX_WIIMOTES, 5);
	if (g_NumberOfWiiMotes > 0) g_RealWiiMotePresent = true;
	Console::Print("Found No of Wiimotes: %i\n", g_NumberOfWiiMotes);

	// Remove the wiiuse_poll() threshold
	wiiuse_set_accel_threshold(g_WiiMotesFromWiiUse[0], 0);

	// Set the sensor bar position, this only affects the internal wiiuse api functions
	wiiuse_set_ir_position(g_WiiMotesFromWiiUse[0], WIIUSE_IR_ABOVE);

	// I don't seem to need wiiuse_connect()
	//int Connect = wiiuse_connect(g_WiiMotesFromWiiUse, MAX_WIIMOTES);
	//Console::Print("Connected: %i\n", Connect);	

	// If we are connecting from the config window without a game running we flash the lights
	if (!g_EmulatorRunning) FlashLights(true);

	// Create Wiimote classes
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
		g_WiiMotes[i] = new CWiiMote(i + 1, g_WiiMotesFromWiiUse[i]);

	// Create a nee thread and start listening for Wiimote data
	if (g_NumberOfWiiMotes > 0)
		g_pReadThread = new Common::Thread(ReadWiimote_ThreadFunc, NULL);

	// If we are not using the emulated wiimote we can run the thread temporary until the data has beeen copied
	if(g_Config.bUseRealWiimote) g_RunTemporary = true;

	/* Allocate memory and copy the Wiimote eeprom accelerometer neutral values to g_Eeprom. We can't
	   change the neutral values the wiimote will report, I think, unless we update its eeprom? In any
	   case it's probably better to let the current calibration be where it is and adjust the global
	   values after that. I don't feel comfortable with overwriting critical data on a lot of Wiimotes. */
	byte *data = (byte*)malloc(sizeof(byte) * sizeof(WiiMoteEmu::EepromData_0));
	wiiuse_read_data(g_WiiMotesFromWiiUse[0], data, 0, sizeof(WiiMoteEmu::EepromData_0));

	// Update the global extension settings
	g_Config.bNunchuckConnected = (g_WiiMotesFromWiiUse[0]->exp.type == EXP_NUNCHUK);
	g_Config.bClassicControllerConnected = (g_WiiMotesFromWiiUse[0]->exp.type == EXP_CLASSIC);

	// Initialized
	if (g_NumberOfWiiMotes > 0) { g_RealWiiMoteInitialized = true; g_Shutdown = false; }	

    return g_NumberOfWiiMotes;
}

void DoState(void* ptr, int mode) {}

void Shutdown(void)
{
	// Stop the loop in the thread
    g_Shutdown = true;

	// Sleep this thread to wait for activity in g_pReadThread to stop entirely
	Sleep(100);

    // Stop the thread
    if (g_pReadThread != NULL)
        {
			// This sometimes hangs for some reason so I'm trying to disable it
			// It seemed to hang in WaitForSingleObject(), but I don't know why
            //g_pReadThread->WaitForDeath();
            delete g_pReadThread;
            g_pReadThread = NULL;
        }

    // Delete the wiimotes
    for (int i = 0; i < g_NumberOfWiiMotes; i++)
        {
            delete g_WiiMotes[i];
            g_WiiMotes[i] = NULL;
        }

	// Flash flights
	if (!g_EmulatorRunning) FlashLights(false);

	// Clean up wiiuse
	wiiuse_cleanup(g_WiiMotesFromWiiUse, g_NumberOfWiiMotes);

	// Uninitialized
	g_RealWiiMoteInitialized = false;

	// Uninitialized
	g_RealWiiMoteInitialized = false;
	g_RealWiiMotePresent = false;
}

void InterruptChannel(u16 _channelID, const void* _pData, u32 _Size)
{
	//Console::Print("Real InterruptChannel\n");
    g_WiiMotes[0]->SendData(_channelID, (const u8*)_pData, _Size);
}

void ControlChannel(u16 _channelID, const void* _pData, u32 _Size)
{
	//Console::Print("Real ControlChannel\n");
    g_WiiMotes[0]->SendData(_channelID, (const u8*)_pData, _Size);
}


//////////////////////////////////
// Read the Wiimote once
// ---------------
void Update()
{
	//Console::Print("Real Update\n");
    for (int i = 0; i < g_NumberOfWiiMotes; i++)
    {
        g_WiiMotes[i]->Update();
    }
}

//////////////////////////////////
/* Continuously read the Wiimote status. However, the actual sending of data occurs in Update(). If we are
   not currently using the real Wiimote we allow the separate ReadWiimote() function to run. Wo don't use
   them at the same time to avoid a potential collision. */
// ---------------
#ifdef _WIN32
	DWORD WINAPI ReadWiimote_ThreadFunc(void* arg)
#else
	void *ReadWiimote_ThreadFunc(void* arg)
#endif
{
    while (!g_Shutdown)
    {
		if(g_Config.bUseRealWiimote && !g_RunTemporary)
			for (int i = 0; i < g_NumberOfWiiMotes; i++) g_WiiMotes[i]->ReadData();
		else
			ReadWiimote();
    }
    return 0;
}
////////////////////


}; // end of namespace

