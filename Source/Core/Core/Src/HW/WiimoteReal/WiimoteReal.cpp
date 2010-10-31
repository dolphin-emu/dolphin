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

#include <queue>

#include "Common.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "Timer.h"

#include "wiiuse.h"
#include "WiimoteReal.h"

#include "../WiimoteEmu/WiimoteHid.h"

// used for pair up
#ifdef _WIN32
#undef NTDDI_VERSION
#define NTDDI_VERSION	NTDDI_WINXPSP2
#include <bthdef.h>
#include <BluetoothAPIs.h>
#pragma comment(lib, "Bthprops.lib")
#endif

unsigned int	g_wiimote_sources[MAX_WIIMOTES];

namespace WiimoteReal
{

bool	g_real_wiimotes_initialized = false;
wiimote_t**		g_wiimotes_from_wiiuse = NULL;
unsigned int	g_wiimotes_found = 0;
// removed g_wiimotes_lastfound because Refresh() isn't taking advantage of it

volatile bool	g_run_wiimote_thread = false;
Common::Thread	*g_wiimote_threads[MAX_WIIMOTES] = {};
Common::CriticalSection		g_refresh_critsec;


THREAD_RETURN WiimoteThreadFunc(void* arg);
void StartWiimoteThreads();
void StopWiimoteThreads();

Wiimote *g_wiimotes[MAX_WIIMOTES];

Wiimote::Wiimote(wiimote_t* const _wiimote, const unsigned int _index)
	: index(_index)
	, wiimote(_wiimote)
	, m_last_data_report(NULL)
	, m_channel(0)
{
	// disable reporting
	DisableDataReporting();

	// set LEDs
	wiiuse_set_leds(wiimote, WIIMOTE_LED_1 << index);

	// TODO: make Dolphin connect wiimote, maybe
}

Wiimote::~Wiimote()
{
	ClearReadQueue();

	// clear write queue
	Report rpt;
	while (m_write_reports.Pop(rpt))
		delete[] rpt.first;

	// disable reporting / wiiuse might do this on shutdown anyway, o well, don't know for sure
	DisableDataReporting();
}

// silly, copying data n stuff, o well, don't use this too often
void Wiimote::SendPacket(const u8 rpt_id, const void* const data, const unsigned int size)
{
	Report rpt;
	rpt.second = size + 2;
	rpt.first = new u8[rpt.second];
	rpt.first[0] = 0xA1;
	rpt.first[1] = rpt_id;
	memcpy(rpt.first + 2, data, size);
	m_write_reports.Push(rpt);
}

void Wiimote::DisableDataReporting()
{
	wm_report_mode rpt;
	rpt.mode = WM_REPORT_CORE;
	rpt.all_the_time = 0;
	rpt.continuous = 0;
	rpt.rumble = 0;
	SendPacket(WM_REPORT_MODE, &rpt, sizeof(rpt));
}

void Wiimote::ClearReadQueue()
{
	if (m_last_data_report)
	{
		delete[] m_last_data_report;
		m_last_data_report = NULL;
	}

	u8 *rpt;
	while (m_read_reports.Pop(rpt))
		delete[] rpt;
}

void Wiimote::ControlChannel(const u16 channel, const void* const data, const u32 size)
{
	// Check for custom communication
	if (99 == channel)
		Disconnect();
	else
	{
		InterruptChannel(channel, data, size);
		const hid_packet* const hidp = (hid_packet*)data;
		if (hidp->type == HID_TYPE_SET_REPORT)
		{
			u8 handshake_ok = HID_HANDSHAKE_SUCCESS;
			Core::Callback_WiimoteInterruptChannel(index, channel, &handshake_ok, sizeof(handshake_ok));
		}
	}
}

void Wiimote::InterruptChannel(const u16 channel, const void* const data, const u32 size)
{
	if (0 == m_channel)	// first interrupt/control channel sent
	{
		ClearReadQueue();

		// request status
		wm_request_status rpt;
		rpt.rumble = 0;
		SendPacket(WM_REQUEST_STATUS, &rpt, sizeof(rpt));
	}

	m_channel = channel;	// this right?

	Wiimote::Report rpt;
	rpt.first = new u8[size];
	rpt.second = (u8)size;
	memcpy(rpt.first, (u8*)data, size);

	// some hax, since we just send the last data report to Dolphin on each Update() call
	// , make the wiimote only send updated data reports when data changes
	// == less bt traffic, eliminates some unneeded packets
	//if (WM_REPORT_MODE == ((u8*)data)[1])
	//{
	//	// also delete the last data report
	//	if (m_last_data_report)
	//	{
	//		delete[] m_last_data_report;
	//		m_last_data_report = NULL;
	//	}

	//	// nice var names :p, this seems to be this one
	//	((wm_report_mode*)(rpt.first + 2))->all_the_time = false;
	//	//((wm_report_mode*)(data + 2))->continuous = false;
	//}

	m_write_reports.Push(rpt);
}

bool Wiimote::Read()
{
	if (wiiuse_io_read(wiimote))
	{
		if (m_channel)
		{
			// add it to queue
			u8* const rpt = new u8[MAX_PAYLOAD];
			memcpy(rpt, wiimote->event_buf, MAX_PAYLOAD);
			m_read_reports.Push(rpt);
		}

		return true;
	}

	return false;
}

bool Wiimote::Write()
{
	Report rpt;
	if (m_write_reports.Pop(rpt))
	{
		wiiuse_io_write(wiimote, rpt.first, rpt.second);
		delete[] rpt.first;

		return true;
	}

	return false;
}

// returns the next report that should be sent
u8* Wiimote::ProcessReadQueue()
{
	// pop through the queued reports
	u8* rpt = m_last_data_report;
	while (m_read_reports.Pop(rpt))
	{
		// a data report
		if (rpt[1] >= WM_REPORT_CORE)
			m_last_data_report = rpt;
		// some other kind of report
		else
			return rpt;
	}

	// the queue was empty, or there were only data reports
	return rpt;
}

void Wiimote::Update()
{
	// pop through the queued reports
	u8* const rpt = ProcessReadQueue();

	// send the report
	if (rpt && m_channel)
		Core::Callback_WiimoteInterruptChannel(index, m_channel, rpt, MAX_PAYLOAD);

	// delete the data if it isn't also the last data rpt
	if (rpt != m_last_data_report)
		delete[] rpt;
}

void Wiimote::Disconnect()
{
	m_channel = 0;

	// disable reporting
	DisableDataReporting();
}

void LoadSettings()
{
	std::string ini_filename = (std::string(File::GetUserPath(D_CONFIG_IDX)) + WIIMOTE_INI_NAME ".ini" );

	IniFile inifile;
	inifile.Load(ini_filename);

	for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
	{
		std::string secname("Wiimote");
		secname += (char)('1' + i);
		IniFile::Section& sec = *inifile.GetOrCreateSection(secname.c_str());

		sec.Get("Source", &g_wiimote_sources[i], WIIMOTE_SRC_EMU);
	}
}

unsigned int Initialize()
{
	// return if already initialized
	if (g_real_wiimotes_initialized)
		return g_wiimotes_found;

	memset(g_wiimotes, 0, sizeof(g_wiimotes));

	// only call wiiuse_find with the number of slots configured for real wiimotes
	unsigned int wanted_wiimotes = 0;
	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		if (WIIMOTE_SRC_REAL & g_wiimote_sources[i])
			++wanted_wiimotes;

	// don't bother initializing wiiuse if we don't want any real wiimotes
	if (0 == wanted_wiimotes)
	{
		g_wiimotes_found = 0;
		return 0;
	}

	// initialized
	g_real_wiimotes_initialized = true;

#ifdef _WIN32 
	// Alloc memory for wiimote structure only if we're starting fresh
	if(!g_wiimotes_from_wiiuse)
		g_wiimotes_from_wiiuse = wiiuse_init(MAX_WIIMOTES);
	// on windows wiiuse_find() expects as a 3rd parameter the amount of last connected wiimotes instead of the timeout,
	// a timeout parameter is useless on win32 here, since at this points we already have the wiimotes discovered and paired up, just not connected.
	g_wiimotes_found = wiiuse_find(g_wiimotes_from_wiiuse, wanted_wiimotes, 0);
#else
	g_wiimotes_from_wiiuse = wiiuse_init(MAX_WIIMOTES);
	g_wiimotes_found = wiiuse_find(g_wiimotes_from_wiiuse, wanted_wiimotes, 5);
#endif
	DEBUG_LOG(WIIMOTE, "Found %i Real Wiimotes, %i wanted",
		g_wiimotes_found, wanted_wiimotes);

	g_wiimotes_found =
		wiiuse_connect(g_wiimotes_from_wiiuse, g_wiimotes_found);

	DEBUG_LOG(WIIMOTE, "Connected to %i Real Wiimotes", g_wiimotes_found);

	// create real wiimote class instances, assign wiimotes
	for (unsigned int i = 0, w = 0; i<MAX_WIIMOTES && w<g_wiimotes_found; ++i)
	{
		// create/assign wiimote
		if (WIIMOTE_SRC_REAL & g_wiimote_sources[i])
			g_wiimotes[i] = new Wiimote(g_wiimotes_from_wiiuse[w++], i);
	}

	StartWiimoteThreads();
	
	return g_wiimotes_found;
}

void Shutdown(void)
{
	if (false == g_real_wiimotes_initialized)
		return;

	// Uninitialized
	g_real_wiimotes_initialized = false;

	StopWiimoteThreads();

	// delete wiimotes
	for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
		if (g_wiimotes[i])
		{
			delete g_wiimotes[i];
			g_wiimotes[i] = NULL;
		}

	// Clean up wiiuse, win32: we cant just delete the struct on win32, since wiiuse_find() maintains it and
	// adds/removes wimotes directly to/from it to prevent problems, which would occur when using more than 1 wiimote if we create it from scratch everytime
#ifndef _WIN32
	wiiuse_cleanup(g_wiimotes_from_wiiuse, MAX_WIIMOTES);
#endif
}

void Refresh()	// this gets called from the GUI thread
{
#ifdef __linux__
	// make sure real wiimotes have been initialized
	if (!g_real_wiimotes_initialized)
	{
		Initialize();
		return;
	}

	// find the number of slots configured for real wiimotes
	unsigned int wanted_wiimotes = 0;
	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		if (WIIMOTE_SRC_REAL & g_wiimote_sources[i])
			++wanted_wiimotes;

	// don't scan for wiimotes if we don't want any more
	if (wanted_wiimotes <= g_wiimotes_found)
		return;

	// scan for wiimotes
	unsigned int num_wiimotes = wiiuse_find(g_wiimotes_from_wiiuse, wanted_wiimotes, 5);
	
	DEBUG_LOG(WIIMOTE, "Found %i Real Wiimotes, %i wanted", num_wiimotes, wanted_wiimotes);

	int num_new_wiimotes = wiiuse_connect(g_wiimotes_from_wiiuse, num_wiimotes);

	DEBUG_LOG(WIIMOTE, "Connected to %i additional Real Wiimotes", num_new_wiimotes);

	StopWiimoteThreads();

	g_refresh_critsec.Enter();

	// create real wiimote class instances, and assign wiimotes for the new wiimotes
	for (unsigned int i = g_wiimotes_found, w = g_wiimotes_found;
		   	i < MAX_WIIMOTES && w < num_wiimotes; ++i)
	{
		// create/assign wiimote
		if (WIIMOTE_SRC_REAL & g_wiimote_sources[i] && NULL == g_wiimotes[i])
			g_wiimotes[i] = new Wiimote(g_wiimotes_from_wiiuse[w++], i);
	}
	g_wiimotes_found = num_wiimotes;

	g_refresh_critsec.Leave();

	StartWiimoteThreads();

#else	// windows/ OSX
	g_refresh_critsec.Enter();

	// should be fine i think
	Shutdown();
	Initialize();

	g_refresh_critsec.Leave();
#endif
}

void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	g_refresh_critsec.Enter();

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->InterruptChannel(_channelID, _pData, _Size);

	g_refresh_critsec.Leave();
}

void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	g_refresh_critsec.Enter();

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->ControlChannel(_channelID, _pData, _Size);

	g_refresh_critsec.Leave();
}


// Read the Wiimote once
void Update(int _WiimoteNumber)
{
	g_refresh_critsec.Enter();

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->Update();

	g_refresh_critsec.Leave();
}

void StateChange(PLUGIN_EMUSTATE newState)
{
	//g_refresh_critsec.Enter();	// enter

	// TODO: disable/enable auto reporting, maybe

	//g_refresh_critsec.Leave();	// leave
}

void StartWiimoteThreads()
{
	g_run_wiimote_thread = true;
	for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
		if (g_wiimotes[i])
			g_wiimote_threads[i] = new Common::Thread(WiimoteThreadFunc, g_wiimotes[i]);
}

void StopWiimoteThreads()
{
	g_run_wiimote_thread = false;
	for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
		if (g_wiimote_threads[i])
		{
			g_wiimote_threads[i]->WaitForDeath();
			delete g_wiimote_threads[i];
			g_wiimote_threads[i] = NULL;
		}
}

THREAD_RETURN WiimoteThreadFunc(void* arg)
{
	Wiimote* const wiimote = (Wiimote*)arg;

	{
	char thname[] = "Wiimote # Thread";
	thname[8] = (char)('1' + wiimote->index);
	Common::SetCurrentThreadName(thname);
	}

	// rumble briefly
	wiiuse_rumble(wiimote->wiimote, 1);
	SLEEP(200);
	wiiuse_rumble(wiimote->wiimote, 0);

	// main loop
	while (g_run_wiimote_thread)
	{
		// hopefully this is alright
		while (wiimote->Write()) {}

		// sleep if there was nothing to read
		if (false == wiimote->Read())
			Common::SleepCurrentThread(1);
	}

	return 0;
}

#ifdef _WIN32
int UnPair()
{
	// TODO:
	return 0;
}

// WiiMote Pair-Up, function will return amount of either new paired or unpaired devices
// negative number on failure
int PairUp(bool unpair)
{
	int nPaired = 0;

	BLUETOOTH_DEVICE_SEARCH_PARAMS srch;
	srch.dwSize = sizeof(srch);
	srch.fReturnAuthenticated = true;
	srch.fReturnRemembered = true;
	srch.fReturnConnected = true; // does not filter properly somehow, so we've to do an additional check on fConnected BT Devices
	srch.fReturnUnknown = true;
	srch.fIssueInquiry = true;
	srch.cTimeoutMultiplier = 2;	// == (2 * 1.28) seconds

	BLUETOOTH_FIND_RADIO_PARAMS radioParam;
	radioParam.dwSize = sizeof(radioParam);
	
	HANDLE hRadio;

	// Enumerate BT radios
	HBLUETOOTH_RADIO_FIND hFindRadio = BluetoothFindFirstRadio(&radioParam, &hRadio);

	if (NULL == hFindRadio)
		return -1;

	while (hFindRadio)
	{
		BLUETOOTH_RADIO_INFO radioInfo;
		radioInfo.dwSize = sizeof(radioInfo);

		// TODO: check for SUCCEEDED()
		BluetoothGetRadioInfo(hRadio, &radioInfo);

		srch.hRadio = hRadio;

		BLUETOOTH_DEVICE_INFO btdi;
		btdi.dwSize = sizeof(btdi);

		// Enumerate BT devices
		HBLUETOOTH_DEVICE_FIND hFindDevice = BluetoothFindFirstDevice(&srch, &btdi);
		while (hFindDevice)
		{
			//btdi.szName is sometimes missings it's content - it's a bt feature..
			DEBUG_LOG(WIIMOTE, "authed %i connected %i remembered %i ", btdi.fAuthenticated, btdi.fConnected, btdi.fRemembered);

			// TODO: Probably could just check for "Nintendo RVL"
			if (0 == wcscmp(btdi.szName, L"Nintendo RVL-WBC-01") || 0 == wcscmp(btdi.szName, L"Nintendo RVL-CNT-01"))
			{
				if (unpair)
				{
					if (SUCCEEDED(BluetoothRemoveDevice(&btdi.Address)))
					{
						NOTICE_LOG(WIIMOTE, "Pair-Up: Automatically removed BT Device on shutdown: %08x", GetLastError());
						++nPaired;
					}
				}
				else
				{
					if (false == btdi.fConnected)
					{
						//TODO: improve the read of the BT driver, esp. when batteries of the wiimote are removed while being fConnected
						if (btdi.fRemembered)
						{
							// Make Windows forget old expired pairing
							// we can pretty much ignore the return value here.
							// it either worked (ERROR_SUCCESS), or the device did not exist (ERROR_NOT_FOUND)
							// in both cases, there is nothing left.
							BluetoothRemoveDevice(&btdi.Address);
						}

						// Activate service
						const DWORD hr = BluetoothSetServiceState(hRadio, &btdi, &HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE);
						if (SUCCEEDED(hr))
							++nPaired;
						else
							ERROR_LOG(WIIMOTE, "Pair-Up: BluetoothSetServiceState() returned %08x", hr);
					}
				}
			}

			if (false == BluetoothFindNextDevice(hFindDevice, &btdi))
			{
				BluetoothFindDeviceClose(hFindDevice);
				hFindDevice = NULL;
			}
		}

		if (false == BluetoothFindNextRadio(hFindRadio, &hRadio))
		{
			BluetoothFindRadioClose(hFindRadio);
			hFindRadio = NULL;
		}
	}

	return nPaired;
}
#endif

}; // end of namespace
