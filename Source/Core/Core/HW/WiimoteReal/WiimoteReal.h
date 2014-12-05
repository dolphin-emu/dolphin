// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/ChunkFile.h"
#include "Common/FifoQueue.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteRealBase.h"

#include "InputCommon/InputConfig.h"

typedef std::vector<u8> Report;

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

	const Report& ProcessReadQueue();

	bool Read();
	bool Write();

	void StartThread();
	void StopThread();

	// "handshake" / stop packets
	void EmuStart();
	void EmuStop();
	void EmuResume();
	void EmuPause();

	void EnablePowerAssertionInternal();
	void DisablePowerAssertionInternal();

	// connecting and disconnecting from physical devices
	// (using address inserted by FindWiimotes)
	// these are called from the wiimote's thread.
	bool ConnectInternal();
	void DisconnectInternal();

	void InitInternal();
	void TeardownInternal();

	bool Connect();

	// TODO: change to something like IsRelevant
	bool IsConnected() const;

	void Prepare(int index);
	bool PrepareOnThread();

	void DisableDataReporting();
	void EnableDataReporting(u8 mode);
	void SetChannel(u16 channel);

	void QueueReport(u8 rpt_id, const void* data, unsigned int size);

	int index;

#if defined(__APPLE__)
	IOBluetoothDevice *btd;
	IOBluetoothL2CAPChannel *ichan;
	IOBluetoothL2CAPChannel *cchan;
	unsigned char* input;
	int inputlen;
	bool m_connected;
	CFRunLoopRef m_wiimote_thread_run_loop;
	IOPMAssertionID m_pm_assertion;
#elif defined(__linux__) && HAVE_BLUEZ
	bdaddr_t bdaddr;                    // Bluetooth address
	int cmd_sock;                       // Command socket
	int int_sock;                       // Interrupt socket
	int wakeup_pipe_w, wakeup_pipe_r;

#elif defined(_WIN32)
	std::basic_string<TCHAR> devicepath; // Unique wiimote reference
	//ULONGLONG btaddr;                  // Bluetooth address
	HANDLE dev_handle;                   // HID handle
	OVERLAPPED hid_overlap_read, hid_overlap_write; // Overlap handle
	enum win_bt_stack_t stack;           // Type of bluetooth stack to use
#endif

protected:
	Report m_last_input_report;
	u16 m_channel;

private:
	void ClearReadQueue();
	void WriteReport(Report rpt);

	int IORead(u8* buf);
	int IOWrite(u8 const* buf, size_t len);
	void IOWakeup();

	void ThreadFunc();
	void SetReady();
	void WaitReady();

	bool m_rumble_state;

	std::thread               m_wiimote_thread;
	// Whether to keep running the thread.
	volatile bool             m_run_thread;
	// Whether to call PrepareOnThread.
	volatile bool             m_need_prepare;
	// Whether the thread has finished ConnectInternal.
	volatile bool             m_thread_ready;
	std::mutex                m_thread_ready_mutex;
	std::condition_variable   m_thread_ready_cond;

	Common::FifoQueue<Report> m_read_reports;
	Common::FifoQueue<Report> m_write_reports;

	Common::Timer m_last_audio_report;
};

class WiimoteScanner
{
public:
	WiimoteScanner();
	~WiimoteScanner();

	bool IsReady() const;

	void WantWiimotes(bool do_want);
	void WantBB(bool do_want);

	void StartScanning();
	void StopScanning();

	void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&);

	// function called when not looking for more wiimotes
	void Update();

private:
	void ThreadFunc();

	std::thread m_scan_thread;

	volatile bool m_run_thread;
	volatile bool m_want_wiimotes;
	volatile bool m_want_bb;

#if defined(_WIN32)
	void CheckDeviceType(std::basic_string<TCHAR> &devicepath, bool &real_wiimote, bool &is_bb);
#elif defined(__linux__) && HAVE_BLUEZ
	int device_id;
	int device_sock;
#endif
};

extern std::recursive_mutex g_refresh_lock;
extern WiimoteScanner g_wiimote_scanner;
extern Wiimote *g_wiimotes[MAX_BBMOTES];

void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void Update(int _WiimoteNumber);

void DoState(PointerWrap &p);
void StateChange(EMUSTATE_CHANGE newState);

int FindWiimotes(Wiimote** wm, int max_wiimotes);
void ChangeWiimoteSource(unsigned int index, int source);

bool IsValidBluetoothName(const std::string& name);
bool IsBalanceBoardName(const std::string& name);

} // WiimoteReal
