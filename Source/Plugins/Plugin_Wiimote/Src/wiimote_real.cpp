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

#include "wiimote_hid.h"
#include "main.h"
#include "EmuMain.h"
#define EXCLUDE_H // Avoid certain declarations in main.h
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
// Send raw HID data from the core to wiimote
// ---------------
void SendData(u16 _channelID, const u8* _pData, u32 _Size)
{
    m_channelID = _channelID;

    m_pCriticalSection->Enter();
    {
        SEvent WriteEvent;
        memcpy(WriteEvent.m_PayLoad, _pData+1, _Size-1);
        m_EventWriteQueue.push(WriteEvent);
    }
    m_pCriticalSection->Leave();
}
/////////////////////


//////////////////////////////////////////
// Read data from wiimote (but don't send it to the core, just filter and queue)
// ---------------
void ReadData() 
{
    m_pCriticalSection->Enter();

	// Send data to the Wiimote
    if (!m_EventWriteQueue.empty())
    {
        SEvent& rEvent = m_EventWriteQueue.front();
        wiiuse_io_write(m_pWiiMote, (byte*)rEvent.m_PayLoad, MAX_PAYLOAD);
        m_EventWriteQueue.pop();
    }

    m_pCriticalSection->Leave();

    if (wiiuse_io_read(m_pWiiMote))
    {
        const byte* pBuffer = m_pWiiMote->event_buf;

        // Check if we have a channel (connection) if so save the data...
        if (m_channelID > 0)
        {
            m_pCriticalSection->Enter();

            // Filter out reports
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
                m_EventReadQueue.push(ImportantEvent);
            }
            m_pCriticalSection->Leave();
        }

		//std::string Temp = ArrayToString(pBuffer, sizeof(pBuffer), 0);
		//Console::Print("Data:\n%s\n", Temp.c_str());
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
        if (m_LastReportValid) SendEvent(m_LastReport);
    }
    else
    {
        SendEvent(m_EventReadQueue.front());
        m_EventReadQueue.pop();
    }

    m_pCriticalSection->Leave();
};
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

    Common::CriticalSection* m_pCriticalSection;
    CEventQueue m_EventReadQueue;
    CEventQueue m_EventWriteQueue;
    bool m_LastReportValid;
    SEvent m_LastReport;
	wiimote_t* m_pWiiMote; // This is g_WiiMotesFromWiiUse[]

//////////////////////////////////////////
// Send event
// ---------------
void SendEvent(SEvent& _rEvent)
{
    // We don't have an answer channel
    if (m_channelID == 0) return;

    // Check event buffer;
    u8 Buffer[1024];
    u32 Offset = 0;
    hid_packet* pHidHeader = (hid_packet*)(Buffer + Offset);
    Offset += sizeof(hid_packet);
    pHidHeader->type = HID_TYPE_DATA;
    pHidHeader->param = HID_PARAM_INPUT;

    memcpy(&Buffer[Offset], _rEvent.m_PayLoad, MAX_PAYLOAD);
    Offset += MAX_PAYLOAD;

    g_WiimoteInitialize.pWiimoteInput(m_channelID, Buffer, Offset);
}
/////////////////////
};



//******************************************************************************
// Function Definitions 
//******************************************************************************
int Initialize()
{
	if (g_RealWiiMoteInitialized) return g_NumberOfWiiMotes;

    memset(g_WiiMotes, 0, sizeof(CWiiMote*) * MAX_WIIMOTES);

	// Call Wiiuse.dll
    g_WiiMotesFromWiiUse = wiiuse_init(MAX_WIIMOTES);
    g_NumberOfWiiMotes = wiiuse_find(g_WiiMotesFromWiiUse, MAX_WIIMOTES, 5);

	if (g_NumberOfWiiMotes > 0) g_RealWiiMotePresent = true;

	Console::Print("Found No of Wiimotes: %i\n", g_NumberOfWiiMotes);

	// For the status window
	if (!g_EmulatorRunning)
	{
		// Do I need this?
		//int Connect = wiiuse_connect(g_WiiMotesFromWiiUse, MAX_WIIMOTES);
		//Console::Print("Connected: %i\n", Connect);

		//wiiuse_set_timeout(g_WiiMotesFromWiiUse, MAX_WIIMOTES, 500, 1000);
		//wiiuse_set_flags(g_WiiMotesFromWiiUse[0], WIIUSE_CONTINUOUS, NULL);

		if(frame) frame->StartTimer();
	}
	else
	{
		//wiiuse_disconnect(g_WiiMotesFromWiiUse);
	}

	// Create Wiimote classes
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
		g_WiiMotes[i] = new CWiiMote(i + 1, g_WiiMotesFromWiiUse[i]);

	// Create a nee thread and start listening for Wiimote data
	if (g_NumberOfWiiMotes > 0)
		g_pReadThread = new Common::Thread(ReadWiimote_ThreadFunc, NULL);

	// Initialized
	if (g_NumberOfWiiMotes > 0) { g_RealWiiMoteInitialized = true; g_Shutdown = false; }	

    return g_NumberOfWiiMotes;
}

void DoState(void* ptr, int mode) {}

void Shutdown(void)
{
    g_Shutdown = true;	

    // Stop the thread
    if (g_pReadThread != NULL)
        {
            g_pReadThread->WaitForDeath();
            delete g_pReadThread;
            g_pReadThread = NULL;
        }

    // Delete the wiimotes
    for (int i = 0; i < g_NumberOfWiiMotes; i++)
        {
            delete g_WiiMotes[i];
            g_WiiMotes[i] = NULL;
        }

	#if defined(HAVE_WX) && HAVE_WX
		if(frame) frame->ShutDown = true;
		if(frame) frame->StartTimer();
	#else
		// Clean up wiiuse
		wiiuse_cleanup(g_WiiMotesFromWiiUse, g_NumberOfWiiMotes);

		// Uninitialized
		g_RealWiiMoteInitialized = false;
	#endif

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
// Continuously read the Wiimote status
// ---------------
#ifdef _WIN32
	DWORD WINAPI ReadWiimote_ThreadFunc(void* arg)
#else
	void *ReadWiimote_ThreadFunc(void* arg)
#endif
{
    while (!g_Shutdown)
    {
		if(g_EmulatorRunning)
			for (int i = 0; i < g_NumberOfWiiMotes; i++) g_WiiMotes[i]->ReadData();
		else
			ReadWiimote();
    }
    return 0;
}
////////////////////


}; // end of namespace

