// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Common/Common.h"
#include "Common/Event.h"
#include "Common/FifoQueue.h"
#include "Common/Flag.h"
#include "Common/NonCopyable.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteReal/WiimoteRealBase.h"

class PointerWrap;

typedef std::vector<u8> Report;

namespace WiimoteReal
{
class Wiimote : NonCopyable
{
public:
  virtual ~Wiimote() {}
  // This needs to be called in derived destructors!
  void Shutdown();

  void ControlChannel(const u16 channel, const void* const data, const u32 size);
  void InterruptChannel(const u16 channel, const void* const data, const u32 size);
  void Update();
  void ConnectOnInput();

  const Report& ProcessReadQueue();

  void Read();
  void Write();

  void StartThread();
  void StopThread();

  // "handshake" / stop packets
  void EmuStart();
  void EmuStop();
  void EmuResume();
  void EmuPause();

  virtual void EnablePowerAssertionInternal() {}
  virtual void DisablePowerAssertionInternal() {}
  // connecting and disconnecting from physical devices
  // (using address inserted by FindWiimotes)
  // these are called from the Wiimote's thread.
  virtual bool ConnectInternal() = 0;
  virtual void DisconnectInternal() = 0;

  bool Connect(int index);

  // TODO: change to something like IsRelevant
  virtual bool IsConnected() const = 0;

  void Prepare();
  bool PrepareOnThread();

  void DisableDataReporting();
  void EnableDataReporting(u8 mode);
  void SetChannel(u16 channel);

  void QueueReport(u8 rpt_id, const void* data, unsigned int size);

  int GetIndex() const;

protected:
  Wiimote();
  int m_index;
  Report m_last_input_report;
  u16 m_channel;
  u8 m_last_connect_request_counter;
  // If true, the Wiimote will be really disconnected when it is disconnected by Dolphin.
  // In any other case, data reporting is not paused to allow reconnecting on any button press.
  // This is not enabled on all platforms as connecting a Wiimote can be a pain on some platforms.
  bool m_really_disconnect = false;

private:
  void ClearReadQueue();
  void WriteReport(Report rpt);

  virtual int IORead(u8* buf) = 0;
  virtual int IOWrite(u8 const* buf, size_t len) = 0;
  virtual void IOWakeup() = 0;

  void ThreadFunc();
  void SetReady();
  void WaitReady();

  bool m_rumble_state;

  std::thread m_wiimote_thread;
  // Whether to keep running the thread.
  std::atomic<bool> m_run_thread{false};
  // Whether to call PrepareOnThread.
  std::atomic<bool> m_need_prepare{false};
  // Whether the thread has finished ConnectInternal.
  std::atomic<bool> m_thread_ready{false};
  std::mutex m_thread_ready_mutex;
  std::condition_variable m_thread_ready_cond;

  Common::FifoQueue<Report> m_read_reports;
  Common::FifoQueue<Report> m_write_reports;
};

enum class WiimoteScanMode
{
  DO_NOT_SCAN,
  CONTINUOUSLY_SCAN,
  SCAN_ONCE
};

class WiimoteScanner
{
public:
  WiimoteScanner();
  ~WiimoteScanner();

  bool IsReady() const;

  void StartThread();
  void StopThread();
  void SetScanMode(WiimoteScanMode scan_mode);

  void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&);

  // function called when not looking for more Wiimotes
  void Update();

private:
  void ThreadFunc();

  std::thread m_scan_thread;

  Common::Event m_scan_mode_changed_event;
  Common::Flag m_scan_thread_running;
  std::atomic<WiimoteScanMode> m_scan_mode{WiimoteScanMode::DO_NOT_SCAN};

#if defined(_WIN32)
  void CheckDeviceType(std::basic_string<TCHAR>& devicepath, WinWriteMethod& write_method,
                       bool& real_wiimote, bool& is_bb);
#elif defined(__linux__) && HAVE_BLUEZ
  int device_id;
  int device_sock;
#endif
};

extern std::mutex g_wiimotes_mutex;
extern WiimoteScanner g_wiimote_scanner;
extern Wiimote* g_wiimotes[MAX_BBMOTES];

void InterruptChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void ControlChannel(int _WiimoteNumber, u16 _channelID, const void* _pData, u32 _Size);
void Update(int _WiimoteNumber);
void ConnectOnInput(int _WiimoteNumber);

void StateChange(EMUSTATE_CHANGE newState);
void ChangeWiimoteSource(unsigned int index, int source);

bool IsValidBluetoothName(const std::string& name);
bool IsBalanceBoardName(const std::string& name);

#ifdef ANDROID
void InitAdapterClass();
#endif

}  // WiimoteReal
