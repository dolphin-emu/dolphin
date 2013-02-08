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
#include <algorithm>
#include <stdlib.h>

#include "Common.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "Timer.h"
#include "Host.h"

#include "WiimoteReal.h"

#include "../WiimoteEmu/WiimoteHid.h"

unsigned int	g_wiimote_sources[MAX_WIIMOTES];

namespace WiimoteReal
{

void HandleFoundWiimotes(const std::vector<Wiimote*>&);
void HandleWiimoteConnect(Wiimote*);
void HandleWiimoteDisconnect(int index);

bool g_real_wiimotes_initialized = false;

std::recursive_mutex g_refresh_lock;

Wiimote* g_wiimotes[MAX_WIIMOTES];

WiimoteScanner g_wiimote_scanner;

Wiimote::Wiimote()
	: index()
#ifdef __APPLE__
	, inputlen(0)
#elif defined(__linux__) && HAVE_BLUEZ
	, cmd_sock(-1), int_sock(-1)
#elif defined(_WIN32)
	, dev_handle(0), stack(MSBT_STACK_UNKNOWN)
#endif
	, m_last_data_report(Report((u8 *)NULL, 0))
	, m_channel(0), m_run_thread(false)
{
#if defined(__linux__) && HAVE_BLUEZ
	bdaddr = (bdaddr_t){{0, 0, 0, 0, 0, 0}};
#endif
}

Wiimote::~Wiimote()
{
	StopThread();

	if (IsConnected())
		Disconnect();
	
	ClearReadQueue();

	// clear write queue
	Report rpt;
	while (m_write_reports.Pop(rpt))
		delete[] rpt.first;
}

// to be called from CPU thread
void Wiimote::QueueReport(u8 rpt_id, const void* _data, unsigned int size)
{
	auto const data = static_cast<const u8*>(_data);
	
	Report rpt;
	rpt.second = size + 2;
	rpt.first = new u8[rpt.second];
	rpt.first[0] = WM_SET_REPORT | WM_BT_OUTPUT;
	rpt.first[1] = rpt_id;
	std::copy(data, data + size, rpt.first + 2);
	m_write_reports.Push(rpt);
}

void Wiimote::DisableDataReporting()
{
	wm_report_mode rpt = {};
	rpt.mode = WM_REPORT_CORE;
	rpt.all_the_time = 0;
	rpt.continuous = 0;
	rpt.rumble = 0;
	QueueReport(WM_REPORT_MODE, &rpt, sizeof(rpt));
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
		EmuStop();
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

void Wiimote::InterruptChannel(const u16 channel, const void* const _data, const u32 size)
{
	// first interrupt/control channel sent
	if (channel != m_channel)
	{
		m_channel = channel;
		
		ClearReadQueue();

		EmuStart();
	}
	
	auto const data = static_cast<const u8*>(_data);

	Report rpt;
	rpt.first = new u8[size];
	rpt.second = (u8)size;
	std::copy(data, data + size, rpt.first);

	// Convert output DATA packets to SET_REPORT packets.
	// Nintendo Wiimotes work without this translation, but 3rd
	// party ones don't.
 	if (rpt.first[0] == 0xa2)
	{
		rpt.first[0] = WM_SET_REPORT | WM_BT_OUTPUT;
 	}

	m_write_reports.Push(rpt);
}

bool Wiimote::Read()
{
	Report rpt;
	
	rpt.first = new unsigned char[MAX_PAYLOAD];
	rpt.second = IORead(rpt.first);

	if (0 == rpt.second)
		Disconnect();

	if (rpt.second > 0 && m_channel > 0)
	{
		// Add it to queue
		m_read_reports.Push(rpt);
		return true;
	}

	delete[] rpt.first;
	return false;
}

bool Wiimote::Write()
{
	if (!m_write_reports.Empty())
	{
		Report const& rpt = m_write_reports.Front();
		
		bool const is_speaker_data = rpt.first[1] == WM_WRITE_SPEAKER_DATA;
		
		if (!is_speaker_data || last_audio_report.GetTimeDifference() > 5)
		{
			IOWrite(rpt.first, rpt.second);
			
			if (is_speaker_data)
				last_audio_report.Update();
			
			delete[] rpt.first;
			m_write_reports.Pop();
			return true;
		}
	}
	
	return false;
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

// Rumble briefly
void Wiimote::RumbleBriefly()
{
	unsigned char buffer = 0x01;
	DEBUG_LOG(WIIMOTE, "Starting rumble...");
	QueueReport(WM_CMD_RUMBLE, &buffer, sizeof(buffer));

	SLEEP(200);

	DEBUG_LOG(WIIMOTE, "Stopping rumble...");
	buffer = 0x00;
	QueueReport(WM_CMD_RUMBLE, &buffer, sizeof(buffer));
}

// Set the active LEDs.
// leds is a bitwise OR of WIIMOTE_LED_1 through WIIMOTE_LED_4.
void Wiimote::SetLEDs(int new_leds)
{
	// Remove the lower 4 bits because they control rumble
	u8 const buffer = (new_leds & 0xF0);
	QueueReport(WM_CMD_LED, &buffer, sizeof(buffer));
}

bool Wiimote::EmuStart()
{
	DisableDataReporting();
}

void Wiimote::EmuStop()
{
	m_channel = 0;

	DisableDataReporting();

	NOTICE_LOG(WIIMOTE, "Stopping wiimote data reporting");
}

unsigned int CalculateWantedWiimotes()
{
	// Figure out how many real wiimotes are required
	unsigned int wanted_wiimotes = 0;
	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		if (WIIMOTE_SRC_REAL & g_wiimote_sources[i] && !g_wiimotes[i])
			++wanted_wiimotes;

	return wanted_wiimotes;
}

void WiimoteScanner::WantWiimotes(size_t count)
{
	want_wiimotes = count;
}

void WiimoteScanner::StartScanning()
{
	run_thread = true;
	scan_thread = std::thread(std::mem_fun(&WiimoteScanner::ThreadFunc), this);
}

void WiimoteScanner::StopScanning()
{
	run_thread = false;
	if (scan_thread.joinable())
	{
		scan_thread.join();
		NOTICE_LOG(WIIMOTE, "Wiimote scanning has stopped");
	}
}

void WiimoteScanner::ThreadFunc()
{
	Common::SetCurrentThreadName("Wiimote Scanning Thread");

	NOTICE_LOG(WIIMOTE, "Wiimote scanning has started");

	while (run_thread)
	{
		auto const found_wiimotes = FindWiimotes(want_wiimotes);
		HandleFoundWiimotes(found_wiimotes);
#if 1
		{
		// TODO: this code here is ugly
		std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);
		for (unsigned int i = 0; i != MAX_WIIMOTES; ++i)
			if (g_wiimotes[i] && !g_wiimotes[i]->IsConnected())
				HandleWiimoteDisconnect(i);
		}
#endif	
		//std::this_thread::yield();
		Common::SleepCurrentThread(500);
	}
}

void Wiimote::StartThread()
{
	m_run_thread = true;
	m_wiimote_thread = std::thread(std::mem_fun(&Wiimote::ThreadFunc), this);
}

void Wiimote::StopThread()
{
	m_run_thread = false;
	if (m_wiimote_thread.joinable())
		m_wiimote_thread.join();
}

void Wiimote::ThreadFunc()
{
	Common::SetCurrentThreadName("Wiimote Device Thread");

	// main loop
	while (m_run_thread && IsConnected())
	{
#ifdef __APPLE__
		while (Write()) {}
		Common::SleepCurrentThread(1);
#else
		bool const did_something = Write() || Read();
		if (!did_something)
			Common::SleepCurrentThread(1);
#endif
	}
}

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

void Initialize()
{
	NOTICE_LOG(WIIMOTE, "WiimoteReal::Initialize");

	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	if (g_real_wiimotes_initialized)
		return;

	auto const wanted_wiimotes = CalculateWantedWiimotes();
	g_wiimote_scanner.WantWiimotes(wanted_wiimotes);

	//if (wanted_wiimotes > 0)
		g_wiimote_scanner.StartScanning();

	g_real_wiimotes_initialized = true;
}

void Shutdown(void)
{
	NOTICE_LOG(WIIMOTE, "WiimoteReal::Shutdown");
	
	g_wiimote_scanner.StopScanning();

	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	if (!g_real_wiimotes_initialized)
		return;

	g_real_wiimotes_initialized = false;

	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		HandleWiimoteDisconnect(i);
}

void ChangeWiimoteSource(unsigned int index, int source)
{
	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	g_wiimote_sources[index] = source;

	// source is emulated, kill any real connection
	if (!(WIIMOTE_SRC_REAL & g_wiimote_sources[index]))
		HandleWiimoteDisconnect(index);

	auto const wanted_wiimotes = CalculateWantedWiimotes();
	g_wiimote_scanner.WantWiimotes(wanted_wiimotes);
}

void HandleWiimoteConnect(Wiimote* wm)
{
	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	for (unsigned int i = 0; i != MAX_WIIMOTES; ++i)
	{
		if (WIIMOTE_SRC_REAL & g_wiimote_sources[i] && !g_wiimotes[i])
		{
			g_wiimotes[i] = wm;

			wm->index = i;
			wm->StartThread();
			
			wm->DisableDataReporting();
			wm->SetLEDs(WIIMOTE_LED_1 << i);
			wm->RumbleBriefly();

			Host_ConnectWiimote(i, true);
			
			NOTICE_LOG(WIIMOTE, "Connected to wiimote %i.", i + 1);

			wm = NULL;
			break;
		}
	}

	delete wm;

	auto const want_wiimotes = CalculateWantedWiimotes();
	g_wiimote_scanner.WantWiimotes(want_wiimotes);
}

void HandleWiimoteDisconnect(int index)
{
	// locked above
	//std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	Host_ConnectWiimote(index, false);

	if (g_wiimotes[index])
	{
		delete g_wiimotes[index];
		g_wiimotes[index] = NULL;
	
		NOTICE_LOG(WIIMOTE, "Disconnected wiimote %i.", index + 1);
	}

	auto const want_wiimotes = CalculateWantedWiimotes();
	g_wiimote_scanner.WantWiimotes(want_wiimotes);
}

void HandleFoundWiimotes(const std::vector<Wiimote*>& wiimotes)
{
	std::for_each(wiimotes.begin(), wiimotes.end(), [](Wiimote* const wm)
	{
		if (wm->Connect())
			HandleWiimoteConnect(wm);
		else
			delete wm;
	});
}

// This is called from the GUI thread
void Refresh()
{
	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	// TODO: stuff, maybe
}

void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->InterruptChannel(_channelID, _pData, _Size);
}

void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->ControlChannel(_channelID, _pData, _Size);
}


// Read the Wiimote once
void Update(int _WiimoteNumber)
{
	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->Update();
	else
		Host_ConnectWiimote(_WiimoteNumber, false);
}

void StateChange(EMUSTATE_CHANGE newState)
{
	//std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	// TODO: disable/enable auto reporting, maybe
}

bool IsValidBluetoothName(const std::string& name)
{
	std::string const prefix("Nintendo RVL-");
	return name.size() > prefix.size() &&
		std::equal(prefix.begin(), prefix.end(), name.begin());
}

}; // end of namespace
