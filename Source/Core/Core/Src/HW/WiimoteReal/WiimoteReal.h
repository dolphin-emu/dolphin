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


#ifndef WIIMOTE_REAL_H
#define WIIMOTE_REAL_H

#include <functional>
#include <vector>

#include "WiimoteRealBase.h"
#include "ChunkFile.h"
#include "Thread.h"
#include "FifoQueue.h"
#include "Timer.h"

#include "../Wiimote.h"
#include "../WiimoteEmu/WiimoteEmu.h"

#include "../../InputCommon/Src/InputConfig.h"

// Pointer to data, and size of data
typedef std::pair<u8*,u8> Report;

namespace WiimoteReal
{

class Wiimote : NonCopyable
{
friend class WiimoteEmu::Wiimote;
public:
	Wiimote();
	~Wiimote();

	void ControlChannel(const u16 channel, const void* const data, const u32 size);
	void InterruptChannel(const u16 channel, const void* const data, const u32 size);
	void Update();

	Report ProcessReadQueue();

	bool Read();
	bool Write();

	void StartThread();
	void StopThread();

	// "handshake" / stop packets
	void EmuStart();
	void EmuStop();

	// connecting and disconnecting from physical devices
	// (using address inserted by FindWiimotes)
	bool Connect();
	void Disconnect();

	// TODO: change to something like IsRelevant
	bool IsConnected() const;

	bool Prepare(int index);

	void DisableDataReporting();
	
	void QueueReport(u8 rpt_id, const void* data, unsigned int size);

	int index;

#if defined(__APPLE__)
	IOBluetoothDevice *btd;
	IOBluetoothL2CAPChannel *ichan;
	IOBluetoothL2CAPChannel *cchan;
	char input[MAX_PAYLOAD];
	int inputlen;
	bool m_connected;
#elif defined(__linux__) && HAVE_BLUEZ
	bdaddr_t bdaddr;					// Bluetooth address
	int cmd_sock;						// Command socket
	int int_sock;						// Interrupt socket

#elif defined(_WIN32)
	std::string devicepath;				// Unique wiimote reference
	//ULONGLONG btaddr;					// Bluetooth address
	HANDLE dev_handle;					// HID handle
	OVERLAPPED hid_overlap_read, hid_overlap_write;	// Overlap handle
	enum win_bt_stack_t stack;			// Type of bluetooth stack to use
#endif

protected:
	Report	m_last_data_report;
	u16	m_channel;

private:
	void ClearReadQueue();
	
	int IORead(u8* buf);
	int IOWrite(u8 const* buf, int len);

	void ThreadFunc();

	bool				m_run_thread;
	std::thread			m_wiimote_thread;
	
	Common::FifoQueue<Report>	m_read_reports;
	Common::FifoQueue<Report>	m_write_reports;
	
	Common::Timer m_last_audio_report;
};

class WiimoteScanner
{
public:
	WiimoteScanner();
	~WiimoteScanner();

	bool IsReady() const;
	
	void WantWiimotes(bool do_want);

	void StartScanning();
	void StopScanning();

	std::vector<Wiimote*> FindWiimotes();

	// function called when not looking for more wiimotes
	void Update();

private:
	void ThreadFunc();

	std::thread m_scan_thread;

	volatile bool m_run_thread;
	volatile bool m_want_wiimotes;

#if defined(_WIN32)
	

#elif defined(__linux__) && HAVE_BLUEZ
	int device_id;
	int device_sock;
#endif
};

extern std::recursive_mutex g_refresh_lock;
extern WiimoteScanner g_wiimote_scanner;
extern Wiimote *g_wiimotes[4];

void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void Update(int _WiimoteNumber);

void DoState(PointerWrap &p);
void StateChange(EMUSTATE_CHANGE newState);

int FindWiimotes(Wiimote** wm, int max_wiimotes);
void ChangeWiimoteSource(unsigned int index, int source);

bool IsValidBluetoothName(const std::string& name);

}; // WiimoteReal

#endif
