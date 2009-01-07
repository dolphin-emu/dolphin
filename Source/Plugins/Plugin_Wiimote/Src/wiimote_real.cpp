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

#include "wiiuse.h"
#include <queue>

#include "Common.h"
#include "Thread.h"

#include "wiimote_hid.h"
#include "EmuMain.h"


extern SWiimoteInitialize g_WiimoteInitialize;
//extern void __Log(int log, const char *format, ...);
//extern void __Log(int log, int v, const char *format, ...);


namespace WiiMoteReal
{
#define MAX_WIIMOTES 1

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
    int				g_NumberOfWiiMotes;
    CWiiMote*			g_WiiMotes[MAX_WIIMOTES];	
    bool			g_Shutdown = false;

//******************************************************************************
// Probably this class should be in its own file
//******************************************************************************

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

            wiiuse_set_leds(m_pWiiMote, WIIMOTE_LED_4);

#ifdef _WIN32
            // F|RES: i dunno if we really need this
            CancelIo(m_pWiiMote->dev_handle);
#endif
        }

        virtual ~CWiiMote() 
        {
            delete m_pCriticalSection;
        };

        // send raw HID data from the core to wiimote
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

        // read data from wiimote (but don't send it to the core, just filter
        // and queue)
        void ReadData() 
        {
            m_pCriticalSection->Enter();

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

                    // check if we have a channel (connection) if so save the
                    // data...
                    if (m_channelID > 0)
                        {
                            m_pCriticalSection->Enter();

                            // filter out reports
                            if (pBuffer[0] >= 0x30) 
                                {
                                    memcpy(m_LastReport.m_PayLoad, pBuffer,
                                           MAX_PAYLOAD);
                                    m_LastReportValid = true;
                                }
                            else
                                {
                                    SEvent ImportantEvent;
                                    memcpy(ImportantEvent.m_PayLoad, pBuffer,
                                           MAX_PAYLOAD);
                                    m_EventReadQueue.push(ImportantEvent);
                                }

                            m_pCriticalSection->Leave();
                        }
                }
        };

        // send queued data to the core
        void Update() 
        {
            m_pCriticalSection->Enter();

            if (m_EventReadQueue.empty())
                {
                    if (m_LastReportValid)
                        SendEvent(m_LastReport);
                }
            else
                {
                    SendEvent(m_EventReadQueue.front());
                    m_EventReadQueue.pop();
                }

            m_pCriticalSection->Leave();
        };

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

        u8 m_WiimoteNumber; // just for debugging
        u16 m_channelID;
        wiimote_t* m_pWiiMote;

        Common::CriticalSection* m_pCriticalSection;
        CEventQueue m_EventReadQueue;
        CEventQueue m_EventWriteQueue;
        bool m_LastReportValid;
        SEvent m_LastReport;

        void SendEvent(SEvent& _rEvent)
        {
            // we don't have an answer channel
            if (m_channelID == 0)
                return;

            // check event buffer;
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
    };

//******************************************************************************
// Function Definitions 
//******************************************************************************

    int Initialize()
    {
        memset(g_WiiMotes, 0, sizeof(CWiiMote*) * MAX_WIIMOTES);

		// Call Wiiuse.dll
        g_WiiMotesFromWiiUse = wiiuse_init(MAX_WIIMOTES);
        g_NumberOfWiiMotes = wiiuse_find(g_WiiMotesFromWiiUse, MAX_WIIMOTES, 5);

        for (int i=0; i<g_NumberOfWiiMotes; i++)
            {
                g_WiiMotes[i] = new CWiiMote(i+1, g_WiiMotesFromWiiUse[i]); 
            }

        if (g_NumberOfWiiMotes > 0)
            g_pReadThread = new Common::Thread(ReadWiimote_ThreadFunc, NULL);

        return g_NumberOfWiiMotes;
    }

    void DoState(void* ptr, int mode)
    {}

    void Shutdown(void)
    {
        g_Shutdown = true;

        // stop the thread
        if (g_pReadThread != NULL)
            {
                g_pReadThread->WaitForDeath();
                delete g_pReadThread;
                g_pReadThread = NULL;
            }

        // delete the wiimotes
        for (int i=0; i<g_NumberOfWiiMotes; i++)
            {
                delete g_WiiMotes[i];
                g_WiiMotes[i] = NULL;
            }

        // clean up wiiuse
        wiiuse_cleanup(g_WiiMotesFromWiiUse, g_NumberOfWiiMotes);
    }

    void InterruptChannel(u16 _channelID, const void* _pData, u32 _Size)
    {
        g_WiiMotes[0]->SendData(_channelID, (const u8*)_pData, _Size);
    }

    void ControlChannel(u16 _channelID, const void* _pData, u32 _Size)
    {
        g_WiiMotes[0]->SendData(_channelID, (const u8*)_pData, _Size);
    }
	
    void Update()
    {
        for (int i=0; i<g_NumberOfWiiMotes; i++)
            {
                g_WiiMotes[i]->Update();
            }
    }

#ifdef _WIN32
    DWORD WINAPI ReadWiimote_ThreadFunc(void* arg)
#else
    void *ReadWiimote_ThreadFunc(void* arg)
#endif
    {
        while (!g_Shutdown)
            {
                for (int i=0; i<g_NumberOfWiiMotes; i++)
                    {
                        g_WiiMotes[i]->ReadData();
                    }
            }
        return 0;
    }

}; // end of namespace

