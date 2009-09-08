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


#include <iostream> // System
#include <queue>

#include "wiiuse.h"

#include "Common.h"
#include "Thread.h"
#include "StringUtil.h"
#include "Timer.h"
#include "pluginspecs_wiimote.h"

#include "wiimote_hid.h"
#include "main.h"
#include "Config.h"
#include "EmuMain.h"
#include "EmuDefinitions.h"
#define EXCLUDE_H // Avoid certain declarations in wiimote_real.h
#include "wiimote_real.h"

extern SWiimoteInitialize g_WiimoteInitialize;

namespace WiiMoteReal
{

// Forwarding

class CWiiMote;

THREAD_RETURN ReadWiimote_ThreadFunc(void* arg);

// Variable declarations

wiimote_t**			g_WiiMotesFromWiiUse = NULL;
Common::Thread*		g_pReadThread = NULL;
int					g_NumberOfWiiMotes;
CWiiMote*			g_WiiMotes[MAX_WIIMOTES];	
bool				g_Shutdown = false;
bool				g_ThreadGoing = false;
bool				g_LocalThread = true;
bool				g_IRSensing = false;
bool				g_MotionSensing = false;
u64					g_UpdateTime = 0;
int					g_UpdateCounter = 0;
bool				g_RunTemporary = false;
int					g_RunTemporaryCountdown = 0;
u8					g_EventBuffer[32];

// Probably this class should be in its own file

class CWiiMote
{
public:

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


// Queue raw HID data from the core to the wiimote
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
		//INFO_LOG(CONSOLE, "Wiimote Write:\n%s\n", Temp.c_str());
    }
    m_pCriticalSection->Leave();
}


/* Read and write data to the Wiimote */
void ReadData() 
{
    m_pCriticalSection->Enter();

	// Send data to the Wiimote
    if (!m_EventWriteQueue.empty())
    {
		//INFO_LOG(CONSOLE, "Writing data to the Wiimote\n");
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
	}
};


// Send queued data to the core
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


// Clear events
void ClearEvents()
{
	while (!m_EventReadQueue.empty())
		m_EventReadQueue.pop();
	while (!m_EventWriteQueue.empty())
		m_EventWriteQueue.pop();	
}

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
    bool m_LastReportValid;
    SEvent m_LastReport;
	wiimote_t* m_pWiiMote; // This is g_WiiMotesFromWiiUse[]
    Common::CriticalSection* m_pCriticalSection;

// Send queued data to the core
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
	/* This Offset value is not exactly correct like it is for the emulated
	   Wiimote reports. It's often to big, but I guess that's okay. The game
	   will know how big the actual data is. */
    Offset += MAX_PAYLOAD;

	// Send it
	g_WiimoteInitialize.pWiimoteInput(m_channelID, Buffer, Offset);

	// Debugging
	// ReadDebugging(false, Buffer, Offset);  
}
};



// Function Definitions 

void SendAcc(u8 _ReportID)
{
	byte DataAcc[MAX_PAYLOAD];

	DataAcc[0] = 0x22; // Report 0x12
	DataAcc[1] = 0x00; // Core buttons
	DataAcc[2] = 0x00;
	DataAcc[3] = _ReportID; // Reporting mode

	// TODO: Update for multiple wiimotes?
	wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[0], (byte*)DataAcc, MAX_PAYLOAD);

	std::string Temp = ArrayToString(DataAcc, 28, 0, 30);
	INFO_LOG(CONSOLE, "SendAcc: %s\n", Temp.c_str());

	//22 00 00 _reportID 00
}

// Clear any potential queued events
void ClearEvents()
{
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
		g_WiiMotes[i]->ClearEvents();
}

// Flash lights, and if connecting, also rumble
void FlashLights(bool Connect)
{

	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{
		if(Connect)	wiiuse_rumble(WiiMoteReal::g_WiiMotesFromWiiUse[i], 1);
		wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[i], WIIMOTE_LED_1 | WIIMOTE_LED_2 | WIIMOTE_LED_3 | WIIMOTE_LED_4);
	}
	SLEEP(100);
	
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{
		if(Connect)
		{
			wiiuse_rumble(WiiMoteReal::g_WiiMotesFromWiiUse[i], 0);

			// End with light 1 or 4
			wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[i], WIIMOTE_LED_1);
		}
		else wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[i], WIIMOTE_LED_4);
	}
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
	INFO_LOG(CONSOLE, "Found No of Wiimotes: %i\n", g_NumberOfWiiMotes);

	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{
		// Remove the wiiuse_poll() threshold
		wiiuse_set_accel_threshold(g_WiiMotesFromWiiUse[i], 0);
		
		// Set the sensor bar position, this should only affect the internal wiiuse api functions
		wiiuse_set_ir_position(g_WiiMotesFromWiiUse[i], WIIUSE_IR_ABOVE);

		// Set flags
		//wiiuse_set_flags(g_WiiMotesFromWiiUse[i], NULL, WIIUSE_SMOOTHING);
	}
	// WiiUse initializes the Wiimotes in Windows right from the wiiuse_find function
	// The Functionality should REALLY be changed
	#ifndef _WIN32
		int Connect = wiiuse_connect(g_WiiMotesFromWiiUse, MAX_WIIMOTES);
		INFO_LOG(CONSOLE, "Connected: %i\n", Connect);
	#endif

	// If we are connecting from the config window without a game running we flash the lights
	if (!g_EmulatorRunning && g_RealWiiMotePresent) FlashLights(true);

	// Create Wiimote classes
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
		g_WiiMotes[i] = new CWiiMote(i + 1, g_WiiMotesFromWiiUse[i]);

	// Create a new thread and start listening for Wiimote data
	if (g_NumberOfWiiMotes > 0)
		g_pReadThread = new Common::Thread(ReadWiimote_ThreadFunc, NULL);

	// If we are not using the emulated wiimote we can run the thread temporary until the data has beeen copied
	if(g_Config.bUseRealWiimote) g_RunTemporary = true;

	/* Allocate memory and copy the Wiimote eeprom accelerometer neutral values
	   to g_Eeprom. Unlike with and extension we have to do this here, because
	   this data is only read once when the Wiimote is connected. Also, we
	   can't change the neutral values the wiimote will report, I think, unless
	   we update its eeprom? In any case it's probably better to let the
	   current calibration be where it is and adjust the global values after
	   that to avoid overwriting critical data on any Wiimote. */
	// TODO: Update for multiple wiimotes?
	byte *data = (byte*)malloc(sizeof(byte) * sizeof(WiiMoteEmu::EepromData_0));
	wiiuse_read_data(g_WiiMotesFromWiiUse[0], data, 0, sizeof(WiiMoteEmu::EepromData_0));

	// Don't run the Wiimote thread if no wiimotes were found
	if (g_NumberOfWiiMotes > 0) g_Shutdown = false;

	// Initialized, even if we didn't find a Wiimote
	g_RealWiiMoteInitialized = true;
	
	return g_NumberOfWiiMotes;
}

void DoState(PointerWrap &p) 
{
	//TODO: Implement
}

void Shutdown(void)
{
	// Stop the loop in the thread
	g_Shutdown = true;

	// Stop the thread
	if (g_pReadThread != NULL)
	{
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
	if (!g_EmulatorRunning && g_RealWiiMotePresent)
		FlashLights(false);

	// Clean up wiiuse
	wiiuse_cleanup(g_WiiMotesFromWiiUse, g_NumberOfWiiMotes);

	// Uninitialized
	g_RealWiiMoteInitialized = false;
	g_RealWiiMotePresent = false;
}

void InterruptChannel(u16 _channelID, const void* _pData, u32 _Size)
{
	//INFO_LOG(CONSOLE, "Real InterruptChannel\n");
	// TODO: Update for multiple Wiimotes
	g_WiiMotes[0]->SendData(_channelID, (const u8*)_pData, _Size);
}

void ControlChannel(u16 _channelID, const void* _pData, u32 _Size)
{
	//INFO_LOG(CONSOLE, "Real ControlChannel\n");
    g_WiiMotes[0]->SendData(_channelID, (const u8*)_pData, _Size);
}


// Read the Wiimote once
void Update()
{
	//INFO_LOG(CONSOLE, "Real Update\n");
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{
		g_WiiMotes[i]->Update();
	}
}

/* Continuously read the Wiimote status. However, the actual sending of data
   occurs in Update(). If we are not currently using the real Wiimote we allow
   the separate ReadWiimote() function to run. Wo don't use them at the same
   time to avoid a potential collision. */
THREAD_RETURN ReadWiimote_ThreadFunc(void* arg)
{
	while (!g_Shutdown)
	{
		// We need g_ThreadGoing to do a manual WaitForSingleObject() from the configuration window
		g_ThreadGoing = true;
		if(g_Config.bUseRealWiimote && !g_RunTemporary)
			for (int i = 0; i < g_NumberOfWiiMotes; i++) g_WiiMotes[i]->ReadData();
		else
			ReadWiimote();
		g_ThreadGoing = false;
	}
	return 0;
}

}; // end of namespace

