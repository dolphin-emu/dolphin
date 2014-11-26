// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <queue>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"
#include "Core/HW/WiimoteEmu/WiimoteHid.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include "SFML/Network.hpp"

unsigned int g_wiimote_sources[MAX_BBMOTES];

namespace WiimoteReal
{

void HandleFoundWiimotes(const std::vector<Wiimote*>&);
void TryToConnectBalanceBoard(Wiimote*);
void TryToConnectWiimote(Wiimote*);
void HandleWiimoteDisconnect(int index);
void DoneWithWiimote(int index);

static bool g_real_wiimotes_initialized = false;

std::recursive_mutex g_refresh_lock;

Wiimote* g_wiimotes[MAX_BBMOTES];
WiimoteScanner g_wiimote_scanner;

Wiimote::Wiimote()
	: index()
	, m_last_input_report()
	, m_channel(0)
	, m_rumble_state()
	, m_need_prepare()
{
	InitInternal();
}

Wiimote::~Wiimote()
{
	DisablePowerAssertionInternal();
	StopThread();
	ClearReadQueue();
	m_write_reports.Clear();
	TeardownInternal();
}

// to be called from CPU thread
void Wiimote::WriteReport(Report rpt)
{
	if (rpt.size() >= 3)
	{
		bool const new_rumble_state = (rpt[2] & 0x1) != 0;

		if (WM_RUMBLE == rpt[1] && new_rumble_state == m_rumble_state)
		{
			// If this is a rumble report and the rumble state didn't change, ignore
			//ERROR_LOG(WIIMOTE, "Ignoring rumble report.");
			return;
		}

		m_rumble_state = new_rumble_state;
	}

	m_write_reports.Push(std::move(rpt));
	IOWakeup();
}

// to be called from CPU thread
void Wiimote::QueueReport(u8 rpt_id, const void* _data, unsigned int size)
{
	auto const data = static_cast<const u8*>(_data);

	Report rpt(size + 2);
	rpt[0] = WM_SET_REPORT | WM_BT_OUTPUT;
	rpt[1] = rpt_id;
	std::copy_n(data, size, rpt.begin() + 2);
	WriteReport(std::move(rpt));
}

void Wiimote::DisableDataReporting()
{
	m_last_input_report.clear();

	// This probably accomplishes nothing.
	wm_report_mode rpt = {};
	rpt.mode = WM_REPORT_CORE;
	rpt.all_the_time = 0;
	rpt.continuous = 0;
	rpt.rumble = 0;
	QueueReport(WM_REPORT_MODE, &rpt, sizeof(rpt));
}

void Wiimote::EnableDataReporting(u8 mode)
{
	m_last_input_report.clear();

	wm_report_mode rpt = {};
	rpt.mode = mode;
	rpt.all_the_time = 1;
	rpt.continuous = 1;
	QueueReport(WM_REPORT_MODE, &rpt, sizeof(rpt));
}

void Wiimote::SetChannel(u16 channel)
{
	m_channel = channel;
}

void Wiimote::ClearReadQueue()
{
	Report rpt;

	// The "Clear" function isn't thread-safe :/
	while (m_read_reports.Pop(rpt))
	{}
}

void Wiimote::ControlChannel(const u16 channel, const void* const data, const u32 size)
{
	// Check for custom communication
	if (99 == channel)
	{
		EmuStop();
	}
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
	Report rpt(data, data + size);
	WiimoteEmu::Wiimote *const wm = (WiimoteEmu::Wiimote*)::Wiimote::GetConfig()->controllers[index];

	// Convert output DATA packets to SET_REPORT packets.
	// Nintendo Wiimotes work without this translation, but 3rd
	// party ones don't.
	if (rpt[0] == 0xa2)
	{
		rpt[0] = WM_SET_REPORT | WM_BT_OUTPUT;
	}

	// Disallow games from turning off all of the LEDs.
	// It makes Wiimote connection status confusing.
	if (rpt[1] == WM_LEDS)
	{
		auto& leds_rpt = *reinterpret_cast<wm_leds*>(&rpt[2]);
		if (0 == leds_rpt.leds)
		{
			// Turn on ALL of the LEDs.
			leds_rpt.leds = 0xf;
		}
	}
	else if (rpt[1] == WM_WRITE_SPEAKER_DATA &&
	         (!SConfig::GetInstance().m_WiimoteEnableSpeaker ||
	         (!wm->m_status.speaker || wm->m_speaker_mute)))
	{
		// Translate speaker data reports into rumble reports.
		rpt[1] = WM_RUMBLE;
		// Keep only the rumble bit.
		rpt[2] &= 0x1;
		rpt.resize(3);
	}

	WriteReport(std::move(rpt));
}

bool Wiimote::Read()
{
	Report rpt(MAX_PAYLOAD);
	auto const result = IORead(rpt.data());

	if (result > 0 && m_channel > 0)
	{
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.iBBDumpPort > 0 &&
		    index == WIIMOTE_BALANCE_BOARD)
		{
			static sf::UdpSocket Socket;
			Socket.send((char*)rpt.data(),
			            rpt.size(),
			            sf::IpAddress::LocalHost,
		                SConfig::GetInstance().m_LocalCoreStartupParameter.iBBDumpPort);
		}

		// Add it to queue
		rpt.resize(result);
		m_read_reports.Push(std::move(rpt));
		return true;
	}
	else if (0 == result)
	{
		ERROR_LOG(WIIMOTE, "Wiimote::IORead failed. Disconnecting Wiimote %d.", index + 1);
		DisconnectInternal();
	}

	return false;
}

bool Wiimote::Write()
{
	if (!m_write_reports.Empty())
	{
		Report const& rpt = m_write_reports.Front();

		bool const is_speaker_data = rpt[1] == WM_WRITE_SPEAKER_DATA;

		if (!is_speaker_data || m_last_audio_report.GetTimeDifference() > 5)
		{
			if (SConfig::GetInstance().m_LocalCoreStartupParameter.iBBDumpPort > 0 && index == WIIMOTE_BALANCE_BOARD)
			{
				static sf::UdpSocket Socket;
				Socket.send((char*)rpt.data(), rpt.size(), sf::IpAddress::LocalHost, SConfig::GetInstance().m_LocalCoreStartupParameter.iBBDumpPort);
			}
			IOWrite(rpt.data(), rpt.size());

			if (is_speaker_data)
			{
				m_last_audio_report.Update();
			}

			m_write_reports.Pop();
			return true;
		}
	}

	return false;
}

static bool IsDataReport(const Report& rpt)
{
	return rpt.size() >= 2 && rpt[1] >= WM_REPORT_CORE;
}

// Returns the next report that should be sent
const Report& Wiimote::ProcessReadQueue()
{
	// Pop through the queued reports
	while (m_read_reports.Pop(m_last_input_report))
	{
		if (!IsDataReport(m_last_input_report))
		{
			// A non-data report, use it.
			return m_last_input_report;

			// Forget the last data report as it may be of the wrong type
			// or contain outdated button data
			// or it's not supposed to be sent at this time
			// It's just easier to be correct this way and it's probably not horrible.
		}
	}

	// If the last report wasn't a data report it's irrelevant.
	if (!IsDataReport(m_last_input_report))
		m_last_input_report.clear();

	// If it was a data report, we repeat that until something else comes in.
	return m_last_input_report;
}

void Wiimote::Update()
{
	if (!IsConnected())
	{
		HandleWiimoteDisconnect(index);
		return;
	}

	// Pop through the queued reports
	const Report& rpt = ProcessReadQueue();

	// Send the report
	if (!rpt.empty() && m_channel > 0)
	{
		Core::Callback_WiimoteInterruptChannel(index, m_channel,
			rpt.data(), (u32)rpt.size());
	}
}

void Wiimote::Prepare(int _index)
{
	index = _index;
	m_need_prepare = true;
}

bool Wiimote::PrepareOnThread()
{
	// core buttons, no continuous reporting
	u8 static const mode_report[] = {WM_SET_REPORT | WM_BT_OUTPUT, WM_REPORT_MODE, 0, WM_REPORT_CORE};

	// Set the active LEDs and turn on rumble.
	u8 static const led_report[] = {WM_SET_REPORT | WM_BT_OUTPUT, WM_LEDS, u8(WIIMOTE_LED_1 << (index%WIIMOTE_BALANCE_BOARD) | 0x1)};

	// Turn off rumble
	u8 static const rumble_report[] = {WM_SET_REPORT | WM_BT_OUTPUT, WM_RUMBLE, 0};

	// Request status report
	u8 static const req_status_report[] = {WM_SET_REPORT | WM_BT_OUTPUT, WM_REQUEST_STATUS, 0};
	// TODO: check for sane response?

	return (IOWrite(mode_report, sizeof(mode_report)) &&
	        IOWrite(led_report, sizeof(led_report)) &&
	        (SLEEP(200), IOWrite(rumble_report, sizeof(rumble_report))) &&
	        IOWrite(req_status_report, sizeof(req_status_report)));
}

void Wiimote::EmuStart()
{
	DisableDataReporting();
	EnablePowerAssertionInternal();
}

void Wiimote::EmuStop()
{
	m_channel = 0;

	DisableDataReporting();

	NOTICE_LOG(WIIMOTE, "Stopping Wiimote data reporting.");

	DisablePowerAssertionInternal();
}

void Wiimote::EmuResume()
{
	WiimoteEmu::Wiimote *const wm = (WiimoteEmu::Wiimote*)::Wiimote::GetConfig()->controllers[index];

	m_last_input_report.clear();

	wm_report_mode rpt = {};
	rpt.mode = wm->m_reporting_mode;
	rpt.all_the_time = 1;
	rpt.continuous = 1;
	QueueReport(WM_REPORT_MODE, &rpt, sizeof(rpt));

	NOTICE_LOG(WIIMOTE, "Resuming Wiimote data reporting.");

	EnablePowerAssertionInternal();
}

void Wiimote::EmuPause()
{
	m_last_input_report.clear();

	wm_report_mode rpt = {};
	rpt.mode = WM_REPORT_CORE;
	rpt.all_the_time = 0;
	rpt.continuous = 0;
	QueueReport(WM_REPORT_MODE, &rpt, sizeof(rpt));

	NOTICE_LOG(WIIMOTE, "Pausing Wiimote data reporting.");

	DisablePowerAssertionInternal();
}

static unsigned int CalculateConnectedWiimotes()
{
	unsigned int connected_wiimotes = 0;
	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		if (g_wiimotes[i])
			++connected_wiimotes;

	return connected_wiimotes;
}

static unsigned int CalculateWantedWiimotes()
{
	// Figure out how many real Wiimotes are required
	unsigned int wanted_wiimotes = 0;
	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
		if (WIIMOTE_SRC_REAL & g_wiimote_sources[i] && !g_wiimotes[i])
			++wanted_wiimotes;

	return wanted_wiimotes;
}

static unsigned int CalculateWantedBB()
{
	unsigned int wanted_bb = 0;
	if (WIIMOTE_SRC_REAL & g_wiimote_sources[WIIMOTE_BALANCE_BOARD] && !g_wiimotes[WIIMOTE_BALANCE_BOARD])
		++wanted_bb;
	return wanted_bb;
}

void WiimoteScanner::WantWiimotes(bool do_want)
{
	m_want_wiimotes = do_want;
}


void WiimoteScanner::WantBB(bool do_want)
{
	m_want_bb = do_want;
}

void WiimoteScanner::StartScanning()
{
	if (!m_run_thread)
	{
		m_run_thread = true;
		m_scan_thread = std::thread(&WiimoteScanner::ThreadFunc, this);
	}
}

void WiimoteScanner::StopScanning()
{
	m_run_thread = false;
	if (m_scan_thread.joinable())
	{
		m_scan_thread.join();
	}
}

static void CheckForDisconnectedWiimotes()
{
	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
		if (g_wiimotes[i] && !g_wiimotes[i]->IsConnected())
			HandleWiimoteDisconnect(i);
}

void WiimoteScanner::ThreadFunc()
{
	Common::SetCurrentThreadName("Wiimote Scanning Thread");

	NOTICE_LOG(WIIMOTE, "Wiimote scanning has started.");

	while (m_run_thread)
	{
		std::vector<Wiimote*> found_wiimotes;
		Wiimote* found_board = nullptr;

		//NOTICE_LOG(WIIMOTE, "In loop");

		if (m_want_wiimotes || m_want_bb)
		{
			FindWiimotes(found_wiimotes, found_board);
		}
		else
		{
			// Does stuff needed to detect disconnects on Windows
			Update();
		}

		//NOTICE_LOG(WIIMOTE, "After update");

		// TODO: this is a fairly lame place for this
		CheckForDisconnectedWiimotes();

		if (m_want_wiimotes)
			HandleFoundWiimotes(found_wiimotes);
		if (m_want_bb && found_board)
			TryToConnectBalanceBoard(found_board);

		//std::this_thread::yield();
		Common::SleepCurrentThread(500);
	}

	NOTICE_LOG(WIIMOTE, "Wiimote scanning has stopped.");
}

bool Wiimote::Connect()
{
	m_thread_ready = false;
	StartThread();
	WaitReady();
	return IsConnected();
}

void Wiimote::StartThread()
{
	m_run_thread = true;
	m_wiimote_thread = std::thread(&Wiimote::ThreadFunc, this);
}

void Wiimote::StopThread()
{
	m_run_thread = false;
	IOWakeup();
	if (m_wiimote_thread.joinable())
		m_wiimote_thread.join();
#if defined(__APPLE__)
#endif
}

void Wiimote::SetReady()
{
	if (!m_thread_ready)
	{
		{
			std::lock_guard<std::mutex> Guard(m_thread_ready_mutex);
			m_thread_ready = true;
		}
		m_thread_ready_cond.notify_all();
	}
}

void Wiimote::WaitReady()
{
	std::unique_lock<std::mutex> lock(m_thread_ready_mutex);
	while (!m_thread_ready)
	{
		m_thread_ready_cond.wait(lock);
	}
}

void Wiimote::ThreadFunc()
{
	Common::SetCurrentThreadName("Wiimote Device Thread");

	bool ok = ConnectInternal();

	SetReady();

	if (!ok)
	{
		return;
	}

	// main loop
	while (IsConnected() && m_run_thread)
	{
		if (m_need_prepare)
		{
			m_need_prepare = false;
			if (!PrepareOnThread())
			{
				ERROR_LOG(WIIMOTE, "Wiimote::PrepareOnThread failed.  Disconnecting Wiimote %d.", index + 1);
				break;
			}
		}
		Write();
		Read();
	}

	DisconnectInternal();
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
		IniFile::Section& sec = *inifile.GetOrCreateSection(secname);

		sec.Get("Source", &g_wiimote_sources[i], i ? WIIMOTE_SRC_NONE : WIIMOTE_SRC_EMU);
	}

	std::string secname("BalanceBoard");
	IniFile::Section& sec = *inifile.GetOrCreateSection(secname);
	sec.Get("Source", &g_wiimote_sources[WIIMOTE_BALANCE_BOARD], WIIMOTE_SRC_NONE);
}

// config dialog calls this when some settings change
void Initialize(bool wait)
{
	if (SConfig::GetInstance().m_WiimoteContinuousScanning)
		g_wiimote_scanner.StartScanning();
	else
		g_wiimote_scanner.StopScanning();

	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	g_wiimote_scanner.WantWiimotes(0 != CalculateWantedWiimotes());
	g_wiimote_scanner.WantBB(0 != CalculateWantedBB());

	// wait for connection because it should exist before state load
	if (wait)
	{
		int timeout = 100;
		std::vector<Wiimote*> found_wiimotes;
		Wiimote* found_board = nullptr;
		g_wiimote_scanner.FindWiimotes(found_wiimotes, found_board);
		if (SConfig::GetInstance().m_WiimoteContinuousScanning)
		{
			while (CalculateWantedWiimotes() && CalculateConnectedWiimotes() < found_wiimotes.size() && timeout)
			{
				Common::SleepCurrentThread(100);
				timeout--;
			}
		}
	}

	if (g_real_wiimotes_initialized)
		return;

	NOTICE_LOG(WIIMOTE, "WiimoteReal::Initialize");

	g_real_wiimotes_initialized = true;
}

// called on emulation shutdown
void Stop()
{
	for (auto& wiimote : g_wiimotes)
		if (wiimote && wiimote->IsConnected())
			wiimote->EmuStop();
}

// called when the dolphin app exits
void Shutdown()
{
	g_wiimote_scanner.StopScanning();

	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	if (!g_real_wiimotes_initialized)
		return;

	NOTICE_LOG(WIIMOTE, "WiimoteReal::Shutdown");

	g_real_wiimotes_initialized = false;

	for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
		HandleWiimoteDisconnect(i);
}

void Resume()
{
	for (auto& wiimote : g_wiimotes)
		if (wiimote && wiimote->IsConnected())
			wiimote->EmuResume();
}

void Pause()
{
	for (auto& wiimote : g_wiimotes)
		if (wiimote && wiimote->IsConnected())
			wiimote->EmuPause();
}

void ChangeWiimoteSource(unsigned int index, int source)
{
	{
		std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);
		g_wiimote_sources[index] = source;
		g_wiimote_scanner.WantWiimotes(0 != CalculateWantedWiimotes());
		g_wiimote_scanner.WantBB(0 != CalculateWantedBB());
		// kill real connection (or swap to different slot)
		DoneWithWiimote(index);
	}

	// reconnect to the emulator
	Host_ConnectWiimote(index, false);
	if (WIIMOTE_SRC_EMU & source)
		Host_ConnectWiimote(index, true);
}

static bool TryToConnectWiimoteN(Wiimote* wm, unsigned int i)
{
	if (WIIMOTE_SRC_REAL & g_wiimote_sources[i] && !g_wiimotes[i])
	{
		if (wm->Connect())
		{
			wm->Prepare(i);
			NOTICE_LOG(WIIMOTE, "Connected to Wiimote %i.", i + 1);
			g_wiimotes[i] = wm;
			Host_ConnectWiimote(i, true);
		}
		return true;
	}
	return false;
}

void TryToConnectWiimote(Wiimote* wm)
{
	std::unique_lock<std::recursive_mutex> lk(g_refresh_lock);

	for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
	{
		if (TryToConnectWiimoteN(wm, i))
		{
			wm = nullptr;
			break;
		}
	}

	g_wiimote_scanner.WantWiimotes(0 != CalculateWantedWiimotes());

	lk.unlock();

	delete wm;
}

void TryToConnectBalanceBoard(Wiimote* wm)
{
	std::unique_lock<std::recursive_mutex> lk(g_refresh_lock);

	if (TryToConnectWiimoteN(wm, WIIMOTE_BALANCE_BOARD))
	{
		wm = nullptr;
	}

	g_wiimote_scanner.WantBB(0 != CalculateWantedBB());

	lk.unlock();

	delete wm;
}

void DoneWithWiimote(int index)
{
	std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	Wiimote* wm = g_wiimotes[index];

	if (wm)
	{
		g_wiimotes[index] = nullptr;
		// First see if we can use this real Wiimote in another slot.
		TryToConnectWiimote(wm);
	}

	// else, just disconnect the Wiimote
	HandleWiimoteDisconnect(index);
}

void HandleWiimoteDisconnect(int index)
{
	Wiimote* wm = nullptr;

	{
		std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

		std::swap(wm, g_wiimotes[index]);
		g_wiimote_scanner.WantWiimotes(0 != CalculateWantedWiimotes());
		g_wiimote_scanner.WantBB(0 != CalculateWantedBB());
	}

	if (wm)
	{
		delete wm;
		NOTICE_LOG(WIIMOTE, "Disconnected Wiimote %i.", index + 1);
	}
}

void HandleFoundWiimotes(const std::vector<Wiimote*>& wiimotes)
{
	std::for_each(wiimotes.begin(), wiimotes.end(), TryToConnectWiimote);
}

// This is called from the GUI thread
void Refresh()
{
	g_wiimote_scanner.StopScanning();

	{
		std::unique_lock<std::recursive_mutex> lk(g_refresh_lock);
		std::vector<Wiimote*> found_wiimotes;
		Wiimote* found_board = nullptr;

		if (0 != CalculateWantedWiimotes() || 0 != CalculateWantedBB())
		{
			// Don't hang Dolphin when searching
			lk.unlock();
			g_wiimote_scanner.FindWiimotes(found_wiimotes, found_board);
			lk.lock();
		}

		CheckForDisconnectedWiimotes();

		// Brief rumble for already connected Wiimotes.
		// Don't do this for Balance Board as it doesn't have rumble anyway.
		for (int i = 0; i < MAX_WIIMOTES; ++i)
		{
			if (g_wiimotes[i])
			{
				g_wiimotes[i]->Prepare(i);
			}
		}

		HandleFoundWiimotes(found_wiimotes);
		if (found_board)
			TryToConnectBalanceBoard(found_board);
	}

	Initialize();
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

	// Wiimote::Update() may remove the Wiimote if it was disconnected.
	if (!g_wiimotes[_WiimoteNumber])
	{
		Host_ConnectWiimote(_WiimoteNumber, false);
	}
}

void StateChange(EMUSTATE_CHANGE newState)
{
	//std::lock_guard<std::recursive_mutex> lk(g_refresh_lock);

	// TODO: disable/enable auto reporting, maybe
}

bool IsValidBluetoothName(const std::string& name)
{
	return
		"Nintendo RVL-CNT-01" == name ||
		"Nintendo RVL-CNT-01-TR" == name ||
		IsBalanceBoardName(name);
}

bool IsBalanceBoardName(const std::string& name)
{
	return
	"Nintendo RVL-WBC-01" == name;
}

}; // end of namespace
