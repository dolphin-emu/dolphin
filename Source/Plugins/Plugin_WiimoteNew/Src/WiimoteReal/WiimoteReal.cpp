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
#include "Thread.h"
#include "StringUtil.h"
#include "Timer.h"
#include "pluginspecs_wiimote.h"

#include "wiiuse.h"
#include "WiimoteReal.h"

#include "../WiimoteEmu/WiimoteHid.h"

unsigned int	g_wiimote_sources[MAX_WIIMOTES];

namespace WiimoteReal
{

bool	g_real_wiimotes_initialized = false;
wiimote_t**		g_wiimotes_from_wiiuse = NULL;
unsigned int	g_wiimotes_found = 0;

volatile bool	g_run_wiimote_thread = false;
Common::Thread	*g_wiimote_thread = NULL;
Common::CriticalSection		g_wiimote_critsec;

THREAD_RETURN WiimoteThreadFunc(void* arg);

// silly, copying data n stuff, o well, don't use this too often
void SendPacket(wiimote_t* const wm, const u8 rpt_id, const void* const data, const unsigned int size)
{
	u8* const rpt = new u8[size + 2];
	rpt[0] = 0xA1;
	rpt[1] = rpt_id;
	memcpy(rpt + 2, data, size);
	wiiuse_io_write(wm, (byte*)rpt, size + 2);
	delete[] rpt;
}

class Wiimote
{
public:
	Wiimote(wiimote_t* const wm, const unsigned int index);
	~Wiimote();

	void ControlChannel(const u16 channel, const void* const data, const u32 size);
	void InterruptChannel(const u16 channel, const void* const data, const u32 size);
	void Update();

	void Read();

	void Disconnect();

private:
	void ClearReports();

	wiimote_t* const	m_wiimote;
	const unsigned int	m_index;
	
	u16		m_channel;
	u8		m_last_data_report[MAX_PAYLOAD];
	bool	m_last_data_report_valid;

	std::queue<u8*>		m_reports;
};

Wiimote::Wiimote(wiimote_t* const wm, const unsigned int index)
	: m_wiimote(wm)
	, m_index(index)
	, m_channel(0)
	, m_last_data_report_valid(false)
{
	{
	// disable reporting
	wm_report_mode rpt = wm_report_mode();
	rpt.mode = WM_REPORT_CORE;
	SendPacket(m_wiimote, WM_REPORT_MODE, &rpt, sizeof(rpt));
	}

	// clear all msgs, silly maybe
	while (wiiuse_io_read(m_wiimote));

	{
	// request status, will be sent to game on start
	wm_request_status rpt = wm_request_status();
	SendPacket(m_wiimote, WM_REQUEST_STATUS, &rpt, sizeof(rpt));
	}

	//{
	// LEDs test
	//wm_leds	rpt = wm_leds();
	//rpt.leds = 1 << i;
	//SendPacket(g_wiimotes_from_wiiuse[i], WM_LEDS, &rpt, sizeof(rpt));
	//}

	// set LEDs
	wiiuse_set_leds(m_wiimote, WIIMOTE_LED_1 << m_index);
}

Wiimote::~Wiimote()
{
	ClearReports();

	// send disconnect message to wii, maybe, i hope, naw shit messes up on emu-stop
	//if (g_WiimoteInitialize.pWiimoteInterruptChannel)
	//{
	//	//u8* const rpt = new u8[2];
	//	//rpt[0] = 0XA1; rpt[1] = 0x15;
	//	//m_reports.push(rpt);
	//	//Update();

	//	const u8 rpt[] = { 0xA1, 0x15 };
	//	g_WiimoteInitialize.pWiimoteInterruptChannel(m_index, m_channel, rpt, sizeof(rpt));
	//}
}

void Wiimote::ClearReports()
{
	while (m_reports.size())
	{
		delete[] m_reports.front();
		m_reports.pop();
	}
}

void Wiimote::ControlChannel(const u16 channel, const void* const data, const u32 size)
{
	// Check for custom communication
	if (99 == channel)
		Disconnect();
	else
		InterruptChannel(channel, data, size);
}

void Wiimote::InterruptChannel(const u16 channel, const void* const data, const u32 size)
{
	m_channel = channel;
	wiiuse_io_write(m_wiimote, (byte*)data, size);
}

void Wiimote::Read()
{
	if (wiiuse_io_read(m_wiimote))
	{
		// a data report, save it
		if (m_wiimote->event_buf[1] >= 0x30)
		{
			memcpy(m_last_data_report, m_wiimote->event_buf, MAX_PAYLOAD);
			m_last_data_report_valid = true;
		}
		else
		{
			// some other report, add it to queue
			u8* const rpt = new u8[MAX_PAYLOAD];
			memcpy(rpt, m_wiimote->event_buf, MAX_PAYLOAD);
			m_reports.push(rpt);
		}
	}
}

void Wiimote::Update()
{
	// do we have some queued reports
	if (m_reports.size())
	{
		u8* const rpt = m_reports.front();
		m_reports.pop();
		g_WiimoteInitialize.pWiimoteInterruptChannel(m_index, m_channel, rpt, MAX_PAYLOAD);
		delete[] rpt;
	}
	else if (m_last_data_report_valid)
	{
		// otherwise send the last data report, if there is one
		g_WiimoteInitialize.pWiimoteInterruptChannel(m_index, m_channel, m_last_data_report, MAX_PAYLOAD);
	}	
}

void Wiimote::Disconnect()
{
	{
	// disable reporting
	wm_report_mode rpt = wm_report_mode();
	rpt.mode = WM_REPORT_CORE;
	SendPacket(m_wiimote, WM_REPORT_MODE, &rpt, sizeof(rpt));
	}	

	// clear queue
	m_last_data_report_valid = false;
	ClearReports();

	// clear out wiiuse queue, or maybe not, silly? idk
	while (wiiuse_io_read(m_wiimote));
}

Wiimote* g_wiimotes[4];

void LoadSettings()
{
	std::string ini_filename = (std::string(File::GetUserPath(D_CONFIG_IDX)) + g_plugin.ini_name + ".ini" );

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
		if (WIIMOTE_SRC_REAL == g_wiimote_sources[i])
			++wanted_wiimotes;

	// don't bother initializing wiiuse if we don't want any real wiimotes
	if (0 == wanted_wiimotes)
	{
		g_wiimotes_found = 0;
		return 0;
	}

	// initialized
	g_real_wiimotes_initialized = true;

	// Call Wiiuse.dll
	g_wiimotes_from_wiiuse = wiiuse_init(MAX_WIIMOTES);
	g_wiimotes_found = wiiuse_find(g_wiimotes_from_wiiuse, wanted_wiimotes, 5);
	
	DEBUG_LOG(WIIMOTE, "Found %i Real Wiimotes, %i wanted", g_wiimotes_found, wanted_wiimotes);

	g_wiimotes_found =
		wiiuse_connect(g_wiimotes_from_wiiuse, g_wiimotes_found);

	DEBUG_LOG(WIIMOTE, "Connected to %i Real Wiimotes", g_wiimotes_found);

	g_wiimote_critsec.Enter();	// enter

	// create real wiimote class instances, assign wiimotes
	
	for (unsigned int i = 0, w = 0; i<MAX_WIIMOTES && w<g_wiimotes_found; ++i)
	{
		if (WIIMOTE_SRC_REAL != g_wiimote_sources[i])
			continue;

		// create/assign wiimote
		g_wiimotes[i] = new Wiimote(g_wiimotes_from_wiiuse[w++], i);
	}

	g_wiimote_critsec.Leave();	// leave

	// start wiimote thread
	g_run_wiimote_thread = true;
	g_wiimote_thread = new Common::Thread(WiimoteThreadFunc, 0);
	
	return g_wiimotes_found;
}

void Shutdown(void)
{
	if (false == g_real_wiimotes_initialized)
		return;

	// Uninitialized
	g_real_wiimotes_initialized = false;

	// stop wiimote thread
	if (g_wiimote_thread)
	{
		g_run_wiimote_thread = false;
		g_wiimote_thread->WaitForDeath();
		delete g_wiimote_thread;
		g_wiimote_thread = NULL;
	}

	g_wiimote_critsec.Enter();	// enter

	// delete wiimotes
	for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
		if (g_wiimotes[i])
		{
			delete g_wiimotes[i];
			g_wiimotes[i] = NULL;
		}

	g_wiimote_critsec.Leave();	// leave

	// set all LEDs on, idk
	//for (unsigned int i=0; i<g_wiimotes_found; ++i)
	//{
	//	wiiuse_set_leds(g_wiimotes_from_wiiuse[i], 0xF0);
	//}

	// Clean up wiiuse
	wiiuse_cleanup(g_wiimotes_from_wiiuse, MAX_WIIMOTES);
}

void Refresh()
{
	// should be fine i think
	Shutdown();
	Initialize();	
}

void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	g_wiimote_critsec.Enter();	// enter

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->InterruptChannel(_channelID, _pData, _Size);

	g_wiimote_critsec.Leave();	// leave
}

void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size)
{
	g_wiimote_critsec.Enter();	// enter

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->ControlChannel(_channelID, _pData, _Size);

	g_wiimote_critsec.Leave();	// leave
}


// Read the Wiimote once
void Update(int _WiimoteNumber)
{
	g_wiimote_critsec.Enter();	// enter

	if (g_wiimotes[_WiimoteNumber])
		g_wiimotes[_WiimoteNumber]->Update();

	g_wiimote_critsec.Leave();	// leave
}

void StateChange(PLUGIN_EMUSTATE newState)
{
	g_wiimote_critsec.Enter();	// enter

	// TODO: disable/enable auto reporting, maybe

	g_wiimote_critsec.Leave();	// leave
}

THREAD_RETURN WiimoteThreadFunc(void* arg)
{
	while (g_run_wiimote_thread)
	{
		g_wiimote_critsec.Enter();	// enter

		for (unsigned int i=0; i<MAX_WIIMOTES; ++i)
			if (g_wiimotes[i])
				g_wiimotes[i]->Read();

		g_wiimote_critsec.Leave();	// leave

		// hmmm, i get occasional lockups without this :/
		Common::SleepCurrentThread(1);
	}

	return 0;
}

}; // end of namespace
