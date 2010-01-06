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
#ifdef WIN32
#include <bthdef.h>
#include <BluetoothAPIs.h>

#pragma comment(lib, "Bthprops.lib")
#endif
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
bool				g_WiimoteInUse[MAX_WIIMOTES];

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
		memcpy(WriteEvent.m_PayLoad, _pData, _Size);
		WriteEvent._Size = _Size;
        m_EventWriteQueue.push(WriteEvent);

		// Debugging
		//std::string Temp = ArrayToString(WriteEvent.m_PayLoad, 28, 0, 30);
		//DEBUG_LOG(WIIMOTE, "Wiimote Write:\n%s", Temp.c_str());
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
		//DEBUG_LOG(WIIMOTE, "Writing data to the Wiimote");
        SEvent& rEvent = m_EventWriteQueue.front();
		wiiuse_io_write(m_pWiiMote, (byte*)rEvent.m_PayLoad, rEvent._Size);
		m_EventWriteQueue.pop();
		
//		InterruptDebugging(false, rEvent.m_PayLoad);
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
			if (pBuffer[1] >= 0x30) 
			{
				// Copy Buffer to LastReport
				memcpy(m_LastReport.m_PayLoad, pBuffer + 1, MAX_PAYLOAD - 1);
				m_LastReportValid = true;
			}
			else
			{
				// Copy Buffer to ImportantEvent
				SEvent ImportantEvent;
				memcpy(ImportantEvent.m_PayLoad, pBuffer + 1, MAX_PAYLOAD - 1);

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
        if (m_LastReportValid)
			SendEvent(m_LastReport);
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
		u32 _Size;
    };
    typedef std::queue<SEvent> CEventQueue;

    u8 m_WiimoteNumber; // Just for debugging
    u16 m_channelID;  
    CEventQueue m_EventReadQueue; // Read from Wiimote
    CEventQueue m_EventWriteQueue; // Write to Wiimote
    SEvent m_LastReport;
	wiimote_t* m_pWiiMote; // This is g_WiiMotesFromWiiUse[]
    Common::CriticalSection* m_pCriticalSection;
	bool m_LastReportValid;

// Send queued data to the core
void SendEvent(SEvent& _rEvent)
{
    // We don't have an answer channel
    if (m_channelID == 0)
		return;

    // Check event buffer
    u8 Buffer[1024];
    u32 Offset = 0;
    hid_packet* pHidHeader = (hid_packet*)(Buffer + Offset);
    pHidHeader->type = HID_TYPE_DATA;
    pHidHeader->param = HID_PARAM_INPUT;

	// Create the buffer
	memcpy(&Buffer[Offset], pHidHeader, sizeof(hid_packet));
	Offset += sizeof(hid_packet);
    memcpy(&Buffer[Offset], _rEvent.m_PayLoad, sizeof(_rEvent.m_PayLoad));
    Offset += sizeof(_rEvent.m_PayLoad); 

	// Send it
	g_WiimoteInitialize.pWiimoteInput(m_WiimoteNumber, m_channelID, Buffer, Offset);

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

	for (int i = 0; i < g_NumberOfWiiMotes; i++)
		wiiuse_io_write(WiiMoteReal::g_WiiMotesFromWiiUse[i], (byte*)DataAcc, MAX_PAYLOAD);

	std::string Temp = ArrayToString(DataAcc, 28, 0, 30);
	DEBUG_LOG(WIIMOTE, "SendAcc: %s", Temp.c_str());

	//22 00 00 _reportID 00
}

// Clear any potential queued events
void ClearEvents()
{
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
		if (g_WiimoteInUse[i])
			g_WiiMotes[i]->ClearEvents();
}

// Flash lights, and if connecting, also rumble
void FlashLights(bool Connect)
{

	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{
		if (Connect)
			wiiuse_rumble(WiiMoteReal::g_WiiMotesFromWiiUse[i], 1);
	}
		SLEEP(200);
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{
		if (Connect) {
			wiiuse_rumble(WiiMoteReal::g_WiiMotesFromWiiUse[i], 0);
			wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[i], WIIMOTE_LED_1 | WIIMOTE_LED_2 | WIIMOTE_LED_3 | WIIMOTE_LED_4);
		}
		else
			wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[i], WIIMOTE_LED_NONE);
	}
}

int Initialize()
{
	// Return if already initialized
	if (g_RealWiiMoteInitialized)
		return g_NumberOfWiiMotes;

	// Clear the wiimote classes
    memset(g_WiiMotes, 0, sizeof(CWiiMote*) * MAX_WIIMOTES);
	for (int i = 0; i < MAX_WIIMOTES; i++)
		g_WiimoteInUse[i] = false;

	g_RealWiiMotePresent = false;
	g_RealWiiMoteAllocated = false;

	if (g_Config.bConnectRealWiimote)
	{
		// Call Wiiuse.dll
		g_WiiMotesFromWiiUse = wiiuse_init(MAX_WIIMOTES);
		g_NumberOfWiiMotes = wiiuse_find(g_WiiMotesFromWiiUse, MAX_WIIMOTES, 5);
		DEBUG_LOG(WIIMOTE, "Found No of Wiimotes: %i", g_NumberOfWiiMotes);
		if (g_NumberOfWiiMotes > 0)
			g_RealWiiMotePresent = true;
		else
			return 0;
	}
	else
	{
		g_NumberOfWiiMotes = 0;
		return 0;
	}

	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{
		// Remove the wiiuse_poll() threshold
		wiiuse_set_accel_threshold(g_WiiMotesFromWiiUse[i], 0);
		
		// Set the sensor bar position, this should only affect the internal wiiuse api functions
		wiiuse_set_ir_position(g_WiiMotesFromWiiUse[i], WIIUSE_IR_ABOVE);

		// Set flags
		//wiiuse_set_flags(g_WiiMotesFromWiiUse[i], NULL, WIIUSE_SMOOTHING);
	}

	// psyjoe reports this allows majority of lost packets to be transferred.
	// Will test soon
	//wiiuse_set_timeout(g_WiiMotesFromWiiUse, g_NumberOfWiiMotes, 220, 220);

	// WiiUse initializes the Wiimotes in Windows right from the wiiuse_find function
	// The Functionality should REALLY be changed
	#ifndef _WIN32
		int Connect = wiiuse_connect(g_WiiMotesFromWiiUse, MAX_WIIMOTES);
		DEBUG_LOG(WIIMOTE, "Connected: %i", Connect);
	#endif

	// If we are connecting from the config window without a game running we set the LEDs
	if (g_EmulatorState != PLUGIN_EMUSTATE_PLAY && g_RealWiiMotePresent)
		FlashLights(true);

	// Create a new thread and start listening for Wiimote data
	if (g_NumberOfWiiMotes > 0)
		g_pReadThread = new Common::Thread(ReadWiimote_ThreadFunc, NULL);

	/* Allocate memory and copy the Wiimote eeprom accelerometer neutral values
	   to g_Eeprom. Unlike with and extension we have to do this here, because
	   this data is only read once when the Wiimote is connected. Also, we
	   can't change the neutral values the wiimote will report, I think, unless
	   we update its eeprom? In any case it's probably better to let the
	   current calibration be where it is and adjust the global values after
	   that to avoid overwriting critical data on any Wiimote. */
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{
		byte *data = (byte*)malloc(sizeof(byte) * sizeof(WiiMoteEmu::EepromData_0));
		wiiuse_read_data(g_WiiMotesFromWiiUse[i], data, 0, sizeof(WiiMoteEmu::EepromData_0));
	}

	// Don't run the Wiimote thread if no wiimotes were found
	if (g_NumberOfWiiMotes > 0)
		g_Shutdown = false;

	// Initialized, even if we didn't find a Wiimote
	g_RealWiiMoteInitialized = true;
	
	return g_NumberOfWiiMotes;
}

// Allocate each Real WiiMote found to a WiiMote slot with Source set to "WiiMote Real"
void Allocate()
{
	if (!g_RealWiiMoteInitialized)
		Initialize();

	// Clear the wiimote classes
    memset(g_WiiMotes, 0, sizeof(CWiiMote*) * MAX_WIIMOTES);
	for (int i = 0; i < MAX_WIIMOTES; i++)
		g_WiimoteInUse[i] = false;

	g_Config.bNumberEmuWiimotes = g_Config.bNumberRealWiimotes = 0;

	// Create Wiimote classes, one for each Real WiiMote found
	int current_number = 0;
	for (int i = 0; i < g_NumberOfWiiMotes; i++)
	{

		// Let's find out the slot this Real WiiMote will be using... We need this because the user can set any of the 4 WiiMotes as Real, Emulated or Inactive...
		for (; current_number < MAX_WIIMOTES; current_number++)
		{
			// Just to be sure we won't be overlapping any WiiMote...
			if (g_WiimoteInUse[current_number] == true)
				continue;
			if (WiiMoteEmu::WiiMapping[current_number].Source == 1)
				g_Config.bNumberEmuWiimotes++;
			// Found a WiiMote (slot) that wants to be real :P
			else if (WiiMoteEmu::WiiMapping[current_number].Source == 2) {
				g_Config.bNumberRealWiimotes++;
				break;
			}
		}
		// If we found a valid slot for this WiiMote...
		if (current_number < MAX_WIIMOTES)
		{
			g_WiiMotes[current_number] = new CWiiMote(current_number, g_WiiMotesFromWiiUse[i]);
			g_WiimoteInUse[current_number] = true;
			switch (current_number)
			{
				case 0:
					wiiuse_set_leds(g_WiiMotesFromWiiUse[i], WIIMOTE_LED_1);
					break;
				case 1:
					wiiuse_set_leds(g_WiiMotesFromWiiUse[i], WIIMOTE_LED_2);
					break;
				case 2:
					wiiuse_set_leds(g_WiiMotesFromWiiUse[i], WIIMOTE_LED_3);
					break;
				case 3:
					wiiuse_set_leds(g_WiiMotesFromWiiUse[i], WIIMOTE_LED_4);
					break;
				default:
					PanicAlert("Trying to create real wiimote %i WTF", current_number);
					break;
			}
			DEBUG_LOG(WIIMOTE, "Real WiiMote allocated as WiiMote #%i", current_number);
		}
	}

	// If there are more slots marked as "Real WiiMote" than the number of Real WiiMotes connected, disable them!
	for(current_number++; current_number < MAX_WIIMOTES; current_number++)
	{
		if (WiiMoteEmu::WiiMapping[current_number].Source == 1)
			g_Config.bNumberEmuWiimotes++;
		else if (WiiMoteEmu::WiiMapping[current_number].Source == 2)
			WiiMoteEmu::WiiMapping[current_number].Source = 0;
	}

	DEBUG_LOG(WIIMOTE, "Using %i Real Wiimote(s) and %i Emu Wiimote(s)", g_Config.bNumberRealWiimotes, g_Config.bNumberEmuWiimotes);

	// If we are not using any emulated wiimotes we can run the thread temporary until the data has beeen copied
	if (g_Config.bNumberEmuWiimotes == 0)
		g_RunTemporary = true;
	
	g_RealWiiMoteAllocated = true;

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
	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		delete g_WiiMotes[i];
		g_WiiMotes[i] = NULL;
	}

	// Flash flights
	if (g_EmulatorState != PLUGIN_EMUSTATE_PLAY && g_RealWiiMotePresent)
		FlashLights(false);

	// Clean up wiiuse
	wiiuse_cleanup(g_WiiMotesFromWiiUse, g_NumberOfWiiMotes);

	// Uninitialized
	g_RealWiiMoteInitialized = false;
	g_RealWiiMotePresent = false;
}

void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	//DEBUG_LOG(WIIMOTE, "Real InterruptChannel on WiiMote #%i", _WiimoteNumber);
	g_WiiMotes[_WiimoteNumber]->SendData(_channelID, (const u8*)_pData, _Size);
}

void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	//DEBUG_LOG(WIIMOTE, "Real ControlChannel on WiiMote #%i", _WiimoteNumber);
	g_WiiMotes[_WiimoteNumber]->SendData(_channelID, (const u8*)_pData, _Size);
}


// Read the Wiimote once
void Update(int _WiimoteNumber)
{
	//DEBUG_LOG(WIIMOTE, "Real Update on WiiMote #%i", _WiimoteNumber);
	g_WiiMotes[_WiimoteNumber]->Update();
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
		// There is at least one Real Wiimote in use
		if (g_Config.bNumberRealWiimotes > 0 && !g_RunTemporary)
		{
			for (int i = 0; i < MAX_WIIMOTES; i++)
				if (g_WiimoteInUse[i])
					g_WiiMotes[i]->ReadData();
		}
		else
			ReadWiimote();
		g_ThreadGoing = false;
	}
	return 0;
}


// WiiMote Pair-Up
#ifdef WIN32
int WiimotePairUp(void)
{
	HANDLE hRadios[256];
	int nRadios;
	int nPaired = 0;

	// Enumerate BT radios
	HBLUETOOTH_RADIO_FIND hFindRadio;
	BLUETOOTH_FIND_RADIO_PARAMS radioParam;

	radioParam.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS);
	nRadios = 0;

	hFindRadio = BluetoothFindFirstRadio(&radioParam, &hRadios[nRadios++]);
	if (hFindRadio)
	{
		while (BluetoothFindNextRadio(&radioParam, &hRadios[nRadios++]));
		BluetoothFindRadioClose(hFindRadio);
	}
	else
	{
		ERROR_LOG(WIIMOTE, "Pair-Up: Error enumerating radios", GetLastError());
		return -1;
	}
	nRadios--;
	// DEBUG_LOG(WIIMOTE, "Pair-Up: Found %d radios\n", nRadios);

	// Pair with Wii device(s)
	int radio = 0;


		for (radio = 0; radio < nRadios; radio++)
		{
			BLUETOOTH_RADIO_INFO radioInfo;
			HBLUETOOTH_DEVICE_FIND hFind;

			BLUETOOTH_DEVICE_INFO btdi;
			BLUETOOTH_DEVICE_SEARCH_PARAMS srch;

			radioInfo.dwSize = sizeof(radioInfo);
			btdi.dwSize = sizeof(btdi);
			srch.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);

			BluetoothGetRadioInfo(hRadios[radio], &radioInfo);

			srch.fReturnAuthenticated = TRUE;
			srch.fReturnRemembered = TRUE;
			srch.fReturnConnected = TRUE; // does not filter properly somehow, so we 've to do an additional check on fConnected BT Devices
			srch.fReturnUnknown = TRUE;
			srch.fIssueInquiry = FALSE;
			srch.cTimeoutMultiplier = 1;
			srch.hRadio = hRadios[radio];

			//DEBUG_LOG(WIIMOTE, _T("Pair-Up: Scanning for BT Device(s)"));

			hFind = BluetoothFindFirstDevice(&srch, &btdi);

			if (hFind == NULL)
			{
				ERROR_LOG(WIIMOTE, "Pair-Up: Error enumerating devices: %08x", GetLastError());  
				return -1;
			}

			//DWORD pcServices = 16;
			//GUID guids[16];
			do
			{
				if ((!wcscmp(btdi.szName, L"Nintendo RVL-WBC-01") || !wcscmp(btdi.szName, L"Nintendo RVL-CNT-01")) && !btdi.fConnected)
				{
					// Wiimote found and not paired up yet
					
					// Wiimote unpaired(no driver installed yet): Name: Nintendo RVL-CNT-01 ClassOfDevice: 9476 fConnected: 0 fRemembered: 0 fAuthenicated: 0
					// Wiimote already paired(driver installed): Name: Nintendo RVL-CNT-01 ClassOfDevice: 9476 fConnected: 32 fRemembered: 16 fAuthenicated: 0
					// Wiimote paired but disc.(driver still active): Name: Nintendo RVL-CNT-01 ClassOfDevice: 9476 fConnected: 0 fRemembered: 16 fAuthenicated: 0
					//TODO: improve the readd of the BT driver, esp. when batteries of the wiimote are removed while beeing fConnected
					if (btdi.fRemembered) 
					{
						// Make Windows forget old expired pairing 
						// we can pretty much ignore the return value here.
						// it either worked (ERROR_SUCCESS), or the device did not exist (ERROR_NOT_FOUND)
						// in both cases, there is nothing left.
						BluetoothRemoveDevice(&btdi.Address);
					}

					// If this is not done, the Wii device will not remember the pairing
					// same thing here, it should return ERROR_SUCCESS, or ERROR_MORE_DATA if the wiimote
					// offers more than 16 services (pcServices).
					//BluetoothEnumerateInstalledServices(hRadios[radio], &btdi, &pcServices, guids);

					// Activate service
					DWORD hr = BluetoothSetServiceState(hRadios[radio], &btdi, &HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE);

					////authentication not directly neeeded, read it w/o getting corrupted driver installation.
					////MAC address is passphrase;
					//WCHAR pass[6];
					//pass[0] = radioInfo.address.rgBytes[0];
					//pass[1] = radioInfo.address.rgBytes[1];
					//pass[2] = radioInfo.address.rgBytes[2];
					//pass[3] = radioInfo.address.rgBytes[3];
					//pass[4] = radioInfo.address.rgBytes[4];
					//pass[5] = radioInfo.address.rgBytes[5];

					//// Pair with Wii device; Pairing before enumerating and setting service state will result mostly in unsuccessfull pairing.
					//if (BluetoothAuthenticateDevice(NULL, hRadios[radio], &btdi, pass, 6) != ERROR_SUCCESS)
					//	error = TRUE;
					if (!hr == ERROR_SUCCESS) 
					{
						nPaired++;
					}
					else 
					{
						ERROR_LOG(WIIMOTE, "Pair-Up: BluetoothSetServiceState() returned %08x", hr);
					}
					
				}
			} while (BluetoothFindNextDevice(hFind, &btdi));
		}

		SLEEP(10);
	

	// Clean up
	for (radio = 0; radio < nRadios; radio++)
	{
		CloseHandle(hRadios[radio]);
	}

	return nPaired;
}
#endif

}; // end of namespace

