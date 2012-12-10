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
#include <stdlib.h>

#include "Common.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "Timer.h"

#include "WiimoteReal.h"

#include "../WiimoteEmu/WiimoteHid.h"

unsigned int	g_wiimote_sources[MAX_WIIMOTES];

namespace WiimoteReal
{

bool g_real_wiimotes_initialized = false;
unsigned int g_wiimotes_found = 0;

std::mutex g_refresh_lock;

Wiimote *g_wiimotes[MAX_WIIMOTES];

Wiimote::Wiimote(const unsigned int _index)
	: index(_index)
#ifdef __APPLE__
	, inputlen(0)
#elif defined(__linux__) && HAVE_BLUEZ
	, out_sock(-1), in_sock(-1)
#elif defined(_WIN32)
	, dev_handle(0), stack(MSBT_STACK_UNKNOWN)
#endif
	, leds(0), m_last_data_report(Report((u8 *)NULL, 0))
	, m_channel(0), m_connected(false)
{
#if defined(__linux__) && HAVE_BLUEZ
	bdaddr = (bdaddr_t){{0, 0, 0, 0, 0, 0}};
#endif

	DisableDataReporting();
}

Wiimote::~Wiimote()
{
	RealDisconnect();

	ClearReadQueue();

	// clear write queue
	Report rpt;
	while (m_write_reports.Pop(rpt))
		delete[] rpt.first;
}

// Silly, copying data n stuff, o well, don't use this too often
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
	Report rpt;

	if (m_last_data_report.first)
	{
		delete[] m_last_data_report.first;
		m_last_data_report.first = NULL;
	}

	while (m_read_reports.Pop(rpt))
		delete[] rpt.first;
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

	Report rpt;
	rpt.first = new u8[size];
	rpt.second = (u8)size;
	memcpy(rpt.first, (u8*)data, size);

	// Convert output DATA packets to SET_REPORT packets.
	// Nintendo Wiimotes work without this translation, but 3rd
	// party ones don't.
 	if (rpt.first[0] == 0xa2)
	{
		rpt.first[0] = WM_SET_REPORT | WM_BT_OUTPUT;
 	}

	if (rpt.first[0] == (WM_SET_REPORT | WM_BT_OUTPUT) &&
		rpt.first[1] == 0x18 && rpt.second == 23)
	{
		m_audio_reports.Push(rpt);
 		return;
 	}

	m_write_reports.Push(rpt);
}

bool Wiimote::Read()
{
	Report rpt;
	
	rpt.first = new unsigned char[MAX_PAYLOAD];
	rpt.second = IORead(rpt.first);

	if (rpt.second > 0 && m_channel > 0) {
		// Add it to queue
		m_read_reports.Push(rpt);
		return true;
	}

	delete rpt.first;
	return false;
}

bool Wiimote::Write()
{
	Report rpt;
	bool audio_written = false;
	
	if (m_audio_reports.Pop(rpt))
	{
		IOWrite(rpt.first, rpt.second);
		delete[] rpt.first;
		audio_written = true;
	}

	if (m_write_reports.Pop(rpt))
	{
		IOWrite(rpt.first, rpt.second);
		delete[] rpt.first;
		return true;
	}	

	return audio_written;
}

// Returns the next report that should be sent
Report Wiimote::ProcessReadQueue()
{
	// Pop through the queued reports
	Report rpt = m_last_data_report;
	while (m_read_reports.Pop(rpt))
	{
		if (rpt.first[1] >= WM_REPORT_CORE)
			// A data report
			m_last_data_report = rpt;
		else
			// Some other kind of report
			return rpt;
	}

	// The queue was empty, or there were only data reports
	return rpt;
}

void Wiimote::Update()
{
	// Pop through the queued reports
	Report const rpt = ProcessReadQueue();

	// Send the report
	if (rpt.first != NULL && m_channel > 0)
		Core::Callback_WiimoteInterruptChannel(index, m_channel,
			rpt.first, rpt.second);

	// Delete the data if it isn't also the last data rpt
	if (rpt != m_last_data_report)
		delete[] rpt.first;
}

void Wiimote::Disconnect()
{
	m_channel = 0;

	DisableDataReporting();
}

bool Wiimote::IsConnected()
{
	return m_connected;
}

// Rumble briefly
void Wiimote::Rumble()
{
	if (!IsConnected())
		return;

	unsigned char buffer = 0x01;
	DEBUG_LOG(WIIMOTE, "Starting rumble...");
	SendRequest(WM_CMD_RUMBLE, &buffer, 1);

	SLEEP(200);

	DEBUG_LOG(WIIMOTE, "Stopping rumble...");
	buffer = 0x00;
	SendRequest(WM_CMD_RUMBLE, &buffer, 1);
}

// Set the active LEDs.
// leds is a bitwise or of WIIMOTE_LED_1 through WIIMOTE_LED_4.
void Wiimote::SetLEDs(int new_leds)
{
	unsigned char buffer;

	if (!IsConnected())
		return;

	// Remove the lower 4 bits because they control rumble
	buffer = leds = (new_leds & 0xF0);

	SendRequest(WM_CMD_LED, &buffer, 1);
}

// Send a handshake
bool Wiimote::Handshake()
{
	// Set buffer[0] to 0x04 for continuous reporting
	unsigned char buffer[2] = {0x04, 0x30};

	if (!IsConnected())
		return 0;

	DEBUG_LOG(WIIMOTE, "Sending handshake to wiimote");

	return SendRequest(WM_CMD_REPORT_TYPE, buffer, 2);
}

// Send a packet to the wiimote.
// report_type should be one of WIIMOTE_CMD_LED, WIIMOTE_CMD_RUMBLE, etc.
bool Wiimote::SendRequest(unsigned char report_type, unsigned char* data, int length)
{
	unsigned char buffer[32] = {WM_SET_REPORT | WM_BT_OUTPUT, report_type};

	memcpy(buffer + 2, data, length);

	return (IOWrite(buffer, length + 2) != 0);
}

void Wiimote::ThreadFunc()
{
	char thname[] = "Wiimote # Thread";
	thname[8] = (char)('1' + index);
	Common::SetCurrentThreadName(thname);

	// rumble briefly
	Rumble();

	// main loop
	while (IsConnected())
	{
#ifdef __APPLE__
		while (Write()) {}
		Common::SleepCurrentThread(1);
#else
		bool read = false;
		while (Write() || (read = true, Read()))
		{
			if (m_audio_reports.Size() && !read)
				Read();
			Common::SleepCurrentThread(m_audio_reports.Size() ? 5 : 2);
			read = false;
		}
#endif
	}
}

#ifndef _WIN32
// Connect all discovered wiimotes
// Return the number of wiimotes that successfully connected.
static int ConnectWiimotes(Wiimote** wm)
{
	int connected = 0;

	for (int i = 0; i < MAX_WIIMOTES; ++i)
	{
		if (!wm[i] || wm[i]->IsConnected())
			continue;

		if (wm[i]->Connect())
			++connected;
		else
		{
			delete wm[i];
			wm[i] = NULL;
		}
	}

	return connected;
}
#endif

void LoadSettings()
{
	std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + WIIMOTE_INI_NAME ".ini";

	IniFile inifile;
	inifile.Load(ini_filename);

	for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
	{
		std::string secname("Wiimote");
		secname += (char)('1' + i);
		IniFile::Section& sec = *inifile.GetOrCreateSection(secname.c_str());

		sec.Get("Source", &g_wiimote_sources[i], i ? WIIMOTE_SRC_NONE : WIIMOTE_SRC_EMU);
	}
}

unsigned int Initialize()
{
	// Return if already initialized
	if (g_real_wiimotes_initialized)
		return g_wiimotes_found;

	memset(g_wiimotes, 0, sizeof(g_wiimotes));

	// Only call FindWiimotes with the number of slots configured for real wiimotes
	unsigned int wanted_wiimotes = 0;
	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		if (WIIMOTE_SRC_REAL & g_wiimote_sources[i])
			++wanted_wiimotes;

	// Don't bother initializing if we don't want any real wiimotes
	if (0 == wanted_wiimotes)
	{
		g_wiimotes_found = 0;
		return 0;
	}

	// Initialized
	g_real_wiimotes_initialized = true;

	g_wiimotes_found = FindWiimotes(g_wiimotes, wanted_wiimotes);

	DEBUG_LOG(WIIMOTE, "Found %i Real Wiimotes, %i wanted",
			g_wiimotes_found, wanted_wiimotes);

#ifndef _WIN32
	atexit(WiimoteReal::Shutdown);
	g_wiimotes_found = ConnectWiimotes(g_wiimotes);
#endif

	DEBUG_LOG(WIIMOTE, "Connected to %i Real Wiimotes", g_wiimotes_found);

	return g_wiimotes_found;
}

void Shutdown(void)
{
	if (false == g_real_wiimotes_initialized)
		return;

	// Uninitialized
	g_real_wiimotes_initialized = false;

	// Delete wiimotes
	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		if (g_wiimotes[i])
		{
			delete g_wiimotes[i];
			g_wiimotes[i] = NULL;
		}
}

// This is called from the GUI thread
void Refresh()
{
	std::lock_guard<std::mutex> lk(g_refresh_lock);

#ifdef _WIN32
	Shutdown();
	Initialize();
#else
	// Make sure real wiimotes have been initialized
	if (!g_real_wiimotes_initialized)
	{
		Initialize();
		return;
	}

	// Find the number of slots configured for real wiimotes
	unsigned int wanted_wiimotes = 0;
	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		if (WIIMOTE_SRC_REAL & g_wiimote_sources[i])
			++wanted_wiimotes;

	// Remove wiimotes that are paired with slots no longer configured for a
	// real wiimote or that are disconnected
	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		if (g_wiimotes[i] && (!(WIIMOTE_SRC_REAL & g_wiimote_sources[i]) ||
					!g_wiimotes[i]->IsConnected()))
		{
			delete g_wiimotes[i];
			g_wiimotes[i] = NULL;
			--g_wiimotes_found;
		}

	// Scan for wiimotes if we want more
	if (wanted_wiimotes > g_wiimotes_found)
	{
		// Scan for wiimotes
		unsigned int num_wiimotes = FindWiimotes(g_wiimotes, wanted_wiimotes);

		DEBUG_LOG(WIIMOTE, "Found %i Real Wiimotes, %i wanted", num_wiimotes, wanted_wiimotes);

		// Connect newly found wiimotes.
		int num_new_wiimotes = ConnectWiimotes(g_wiimotes);

		DEBUG_LOG(WIIMOTE, "Connected to %i additional Real Wiimotes", num_new_wiimotes);

		g_wiimotes_found = num_wiimotes;
	}
#endif
}

void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	std::lock_guard<std::mutex> lk(g_refresh_lock);

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->InterruptChannel(_channelID, _pData, _Size);
}

void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	std::lock_guard<std::mutex> lk(g_refresh_lock);

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->ControlChannel(_channelID, _pData, _Size);
}


// Read the Wiimote once
void Update(int _WiimoteNumber)
{
	std::lock_guard<std::mutex> lk(g_refresh_lock);

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->Update();
}

void StateChange(EMUSTATE_CHANGE newState)
{
	//std::lock_guard<std::mutex> lk(g_refresh_lock);

	// TODO: disable/enable auto reporting, maybe
}

bool IsValidBluetoothName(const char* name) {
	static const char* kValidWiiRemoteBluetoothNames[] = {
		"Nintendo RVL-CNT-01",
		"Nintendo RVL-CNT-01-TR",
		"Nintendo RVL-WBC-01",
	};
	if (name == NULL)
		return false;
	for (size_t i = 0; i < ARRAYSIZE(kValidWiiRemoteBluetoothNames); i++)
		if (strcmp(name, kValidWiiRemoteBluetoothNames[i]) == 0)
			return true;
	return false;
}


}; // end of namespace
