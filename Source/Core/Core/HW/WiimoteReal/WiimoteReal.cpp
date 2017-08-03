// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <queue>
#include <unordered_set>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/IOAndroid.h"
#include "Core/HW/WiimoteReal/IOLinux.h"
#include "Core/HW/WiimoteReal/IOWin.h"
#include "Core/HW/WiimoteReal/IOdarwin.h"
#include "Core/HW/WiimoteReal/IOhidapi.h"
#include "InputCommon/InputConfig.h"

#include "SFML/Network.hpp"

unsigned int g_wiimote_sources[MAX_BBMOTES];

namespace WiimoteReal
{
void TryToConnectBalanceBoard(Wiimote*);
void TryToConnectWiimote(Wiimote*);
void HandleWiimoteDisconnect(int index);

static bool g_real_wiimotes_initialized = false;

// This is used to store connected Wiimotes' IDs, so we don't connect
// more than once to the same device.
static std::unordered_set<std::string> s_known_ids;
static std::mutex s_known_ids_mutex;

std::mutex g_wiimotes_mutex;

Wiimote* g_wiimotes[MAX_BBMOTES];
WiimoteScanner g_wiimote_scanner;

Wiimote::Wiimote() : m_index(), m_last_input_report(), m_channel(0), m_rumble_state()
{
}

void Wiimote::Shutdown()
{
  StopThread();
  ClearReadQueue();
  m_write_reports.Clear();
}

// to be called from CPU thread
void Wiimote::WriteReport(Report rpt)
{
  if (rpt.size() >= 3)
  {
    bool const new_rumble_state = (rpt[2] & 0x1) != 0;

    // If this is a rumble report and the rumble state didn't change, ignore.
    if (rpt[1] == RT_RUMBLE && new_rumble_state == m_rumble_state)
      return;

    m_rumble_state = new_rumble_state;
  }

  m_write_reports.Push(std::move(rpt));
  IOWakeup();
}

// to be called from CPU thread
void Wiimote::QueueReport(u8 rpt_id, const void* data, unsigned int size)
{
  auto const queue_data = static_cast<const u8*>(data);

  Report rpt(size + 2);
  rpt[0] = WR_SET_REPORT | BT_OUTPUT;
  rpt[1] = rpt_id;
  std::copy_n(queue_data, size, rpt.begin() + 2);
  WriteReport(std::move(rpt));
}

void Wiimote::DisableDataReporting()
{
  m_last_input_report.clear();

  // This probably accomplishes nothing.
  wm_report_mode rpt = {};
  rpt.mode = RT_REPORT_CORE;
  rpt.all_the_time = 0;
  rpt.continuous = 0;
  rpt.rumble = 0;
  QueueReport(RT_REPORT_MODE, &rpt, sizeof(rpt));
}

void Wiimote::EnableDataReporting(u8 mode)
{
  m_last_input_report.clear();

  wm_report_mode rpt = {};
  rpt.mode = mode;
  rpt.all_the_time = 1;
  rpt.continuous = 1;
  QueueReport(RT_REPORT_MODE, &rpt, sizeof(rpt));
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
  {
  }
}

void Wiimote::ControlChannel(const u16 channel, const void* const data, const u32 size)
{
  // Check for custom communication
  if (channel == 99)
  {
    if (m_really_disconnect)
      DisconnectInternal();
  }
  else
  {
    InterruptChannel(channel, data, size);
    const hid_packet* const hidp = reinterpret_cast<const hid_packet* const>(data);
    if (hidp->type == HID_TYPE_SET_REPORT)
    {
      u8 handshake_ok = HID_HANDSHAKE_SUCCESS;
      Core::Callback_WiimoteInterruptChannel(m_index, channel, &handshake_ok, sizeof(handshake_ok));
    }
  }
}

void Wiimote::InterruptChannel(const u16 channel, const void* const data, const u32 size)
{
  // first interrupt/control channel sent
  if (channel != m_channel)
  {
    m_channel = channel;

    ClearReadQueue();

    EmuStart();
  }

  auto const report_data = static_cast<const u8*>(data);
  Report rpt(report_data, report_data + size);
  WiimoteEmu::Wiimote* const wm =
      static_cast<WiimoteEmu::Wiimote*>(::Wiimote::GetConfig()->GetController(m_index));

  // Convert output DATA packets to SET_REPORT packets.
  // Nintendo Wiimotes work without this translation, but 3rd
  // party ones don't.
  if (rpt[0] == 0xa2)
  {
    rpt[0] = WR_SET_REPORT | BT_OUTPUT;
  }

  // Disallow games from turning off all of the LEDs.
  // It makes Wiimote connection status confusing.
  if (rpt[1] == RT_LEDS)
  {
    auto& leds_rpt = *reinterpret_cast<wm_leds*>(&rpt[2]);
    if (0 == leds_rpt.leds)
    {
      // Turn on ALL of the LEDs.
      leds_rpt.leds = 0xf;
    }
  }
  else if (rpt[1] == RT_WRITE_SPEAKER_DATA && (!SConfig::GetInstance().m_WiimoteEnableSpeaker ||
                                               (!wm->m_status.speaker || wm->m_speaker_mute)))
  {
    // Translate speaker data reports into rumble reports.
    rpt[1] = RT_RUMBLE;
    // Keep only the rumble bit.
    rpt[2] &= 0x1;
    rpt.resize(3);
  }

  WriteReport(std::move(rpt));
}

void Wiimote::Read()
{
  Report rpt(MAX_PAYLOAD);
  auto const result = IORead(rpt.data());

  if (result > 0 && m_channel > 0)
  {
    if (SConfig::GetInstance().iBBDumpPort > 0 && m_index == WIIMOTE_BALANCE_BOARD)
    {
      static sf::UdpSocket Socket;
      Socket.send((char*)rpt.data(), rpt.size(), sf::IpAddress::LocalHost,
                  SConfig::GetInstance().iBBDumpPort);
    }

    // Add it to queue
    rpt.resize(result);
    m_read_reports.Push(std::move(rpt));
  }
  else if (0 == result)
  {
    ERROR_LOG(WIIMOTE, "Wiimote::IORead failed. Disconnecting Wii Remote %d.", m_index + 1);
    DisconnectInternal();
  }
}

bool Wiimote::Write()
{
  // nothing written, but this is not an error
  if (m_write_reports.Empty())
    return true;

  Report const& rpt = m_write_reports.Front();

  if (SConfig::GetInstance().iBBDumpPort > 0 && m_index == WIIMOTE_BALANCE_BOARD)
  {
    static sf::UdpSocket Socket;
    Socket.send((char*)rpt.data(), rpt.size(), sf::IpAddress::LocalHost,
                SConfig::GetInstance().iBBDumpPort);
  }
  int ret = IOWrite(rpt.data(), rpt.size());

  m_write_reports.Pop();

  if (!m_write_reports.Empty())
    IOWakeup();

  return ret != 0;
}

bool Wiimote::IsBalanceBoard()
{
  if (!ConnectInternal())
    return false;
  // Initialise the extension by writing 0x55 to 0xa400f0, then writing 0x00 to 0xa400fb.
  static const u8 init_extension_rpt1[MAX_PAYLOAD] = {
      WR_SET_REPORT | BT_OUTPUT, RT_WRITE_DATA, 0x04, 0xa4, 0x00, 0xf0, 0x01, 0x55};
  static const u8 init_extension_rpt2[MAX_PAYLOAD] = {
      WR_SET_REPORT | BT_OUTPUT, RT_WRITE_DATA, 0x04, 0xa4, 0x00, 0xfb, 0x01, 0x00};
  static const u8 status_report[] = {WR_SET_REPORT | BT_OUTPUT, RT_REQUEST_STATUS, 0};
  if (!IOWrite(init_extension_rpt1, sizeof(init_extension_rpt1)) ||
      !IOWrite(init_extension_rpt2, sizeof(init_extension_rpt2)))
  {
    ERROR_LOG(WIIMOTE, "IsBalanceBoard(): Failed to initialise extension.");
    return false;
  }

  int ret = IOWrite(status_report, sizeof(status_report));
  u8 buf[MAX_PAYLOAD];
  while (ret != 0)
  {
    ret = IORead(buf);
    if (ret == -1)
      continue;

    switch (buf[1])
    {
    case RT_STATUS_REPORT:
    {
      const auto* status = reinterpret_cast<wm_status_report*>(&buf[2]);
      // A Balance Board has a Balance Board extension.
      if (!status->extension)
        return false;
      // Read two bytes from 0xa400fe to identify the extension.
      static const u8 identify_ext_rpt[] = {
          WR_SET_REPORT | BT_OUTPUT, RT_READ_DATA, 0x04, 0xa4, 0x00, 0xfe, 0x02, 0x00};
      ret = IOWrite(identify_ext_rpt, sizeof(identify_ext_rpt));
      break;
    }
    case RT_READ_DATA_REPLY:
    {
      const auto* reply = reinterpret_cast<wm_read_data_reply*>(&buf[2]);
      if (Common::swap16(reply->address) != 0x00fe)
      {
        ERROR_LOG(WIIMOTE, "IsBalanceBoard(): Received unexpected data reply for address %X",
                  Common::swap16(reply->address));
        return false;
      }
      // A Balance Board ext can be identified by checking for 0x0402.
      return reply->data[0] == 0x04 && reply->data[1] == 0x02;
    }
    case RT_ACK_DATA:
    {
      const auto* ack = reinterpret_cast<wm_acknowledge*>(&buf[2]);
      if (ack->reportID == RT_READ_DATA && ack->errorID != 0x00)
      {
        WARN_LOG(WIIMOTE, "Failed to read from 0xa400fe, assuming Wiimote is not a Balance Board.");
        return false;
      }
    }
    }
  }
  return false;
}

static bool IsDataReport(const Report& rpt)
{
  return rpt.size() >= 2 && rpt[1] >= RT_REPORT_CORE;
}

// Returns the next report that should be sent
Report& Wiimote::ProcessReadQueue()
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
    HandleWiimoteDisconnect(m_index);
    return;
  }

  // Pop through the queued reports
  const Report& rpt = ProcessReadQueue();

  // Send the report
  if (!rpt.empty() && m_channel > 0)
  {
    Core::Callback_WiimoteInterruptChannel(m_index, m_channel, rpt.data(), (u32)rpt.size());
  }
}

bool Wiimote::CheckForButtonPress()
{
  const Report& rpt = ProcessReadQueue();
  if (rpt.size() >= 4)
  {
    switch (rpt[1])
    {
    case RT_REPORT_CORE:
    case RT_REPORT_CORE_ACCEL:
    case RT_REPORT_CORE_EXT8:
    case RT_REPORT_CORE_ACCEL_IR12:
    case RT_REPORT_CORE_EXT19:
    case RT_REPORT_CORE_ACCEL_EXT16:
    case RT_REPORT_CORE_IR10_EXT9:
    case RT_REPORT_CORE_ACCEL_IR10_EXT6:
    case RT_REPORT_INTERLEAVE1:
    case RT_REPORT_INTERLEAVE2:
      // check any button without checking accelerometer data
      if ((rpt[2] & 0x1F) != 0 || (rpt[3] & 0x9F) != 0)
      {
        return true;
      }
      break;
    default:
      break;
    }
  }
  return false;
}

void Wiimote::Prepare()
{
  m_need_prepare.Set();
  IOWakeup();
}

bool Wiimote::PrepareOnThread()
{
  // core buttons, no continuous reporting
  u8 static const mode_report[] = {WR_SET_REPORT | BT_OUTPUT, RT_REPORT_MODE, 0, RT_REPORT_CORE};

  // Set the active LEDs and turn on rumble.
  u8 static led_report[] = {WR_SET_REPORT | BT_OUTPUT, RT_LEDS, 0};
  led_report[2] = u8(WiimoteLED::LED_1 << (m_index % WIIMOTE_BALANCE_BOARD) | 0x1);

  // Turn off rumble
  u8 static const rumble_report[] = {WR_SET_REPORT | BT_OUTPUT, RT_RUMBLE, 0};

  // Request status report
  u8 static const req_status_report[] = {WR_SET_REPORT | BT_OUTPUT, RT_REQUEST_STATUS, 0};
  // TODO: check for sane response?

  return (IOWrite(mode_report, sizeof(mode_report)) && IOWrite(led_report, sizeof(led_report)) &&
          (Common::SleepCurrentThread(200), IOWrite(rumble_report, sizeof(rumble_report))) &&
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
  WiimoteEmu::Wiimote* const wm =
      static_cast<WiimoteEmu::Wiimote*>(::Wiimote::GetConfig()->GetController(m_index));

  m_last_input_report.clear();

  wm_report_mode rpt = {};
  rpt.mode = wm->m_reporting_mode;
  rpt.all_the_time = 1;
  rpt.continuous = 1;
  QueueReport(RT_REPORT_MODE, &rpt, sizeof(rpt));

  NOTICE_LOG(WIIMOTE, "Resuming Wiimote data reporting.");

  EnablePowerAssertionInternal();
}

void Wiimote::EmuPause()
{
  m_last_input_report.clear();

  wm_report_mode rpt = {};
  rpt.mode = RT_REPORT_CORE;
  rpt.all_the_time = 0;
  rpt.continuous = 0;
  QueueReport(RT_REPORT_MODE, &rpt, sizeof(rpt));

  NOTICE_LOG(WIIMOTE, "Pausing Wiimote data reporting.");

  DisablePowerAssertionInternal();
}

static unsigned int CalculateConnectedWiimotes()
{
  std::lock_guard<std::mutex> lk(g_wiimotes_mutex);
  unsigned int connected_wiimotes = 0;
  for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
    if (g_wiimotes[i])
      ++connected_wiimotes;

  return connected_wiimotes;
}

static unsigned int CalculateWantedWiimotes()
{
  std::lock_guard<std::mutex> lk(g_wiimotes_mutex);
  // Figure out how many real Wiimotes are required
  unsigned int wanted_wiimotes = 0;
  for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
    if (WIIMOTE_SRC_REAL & g_wiimote_sources[i] && !g_wiimotes[i])
      ++wanted_wiimotes;

  return wanted_wiimotes;
}

static unsigned int CalculateWantedBB()
{
  std::lock_guard<std::mutex> lk(g_wiimotes_mutex);
  unsigned int wanted_bb = 0;
  if (WIIMOTE_SRC_REAL & g_wiimote_sources[WIIMOTE_BALANCE_BOARD] &&
      !g_wiimotes[WIIMOTE_BALANCE_BOARD])
    ++wanted_bb;
  return wanted_bb;
}

void WiimoteScanner::StartThread()
{
  if (m_scan_thread_running.IsSet())
    return;
  m_scan_thread_running.Set();
  m_scan_thread = std::thread(&WiimoteScanner::ThreadFunc, this);
}

void WiimoteScanner::StopThread()
{
  if (m_scan_thread_running.TestAndClear())
  {
    SetScanMode(WiimoteScanMode::DO_NOT_SCAN);
    m_scan_thread.join();
  }
}

void WiimoteScanner::SetScanMode(WiimoteScanMode scan_mode)
{
  m_scan_mode.store(scan_mode);
  m_scan_mode_changed_event.Set();
}

bool WiimoteScanner::IsReady() const
{
  std::lock_guard<std::mutex> lg(m_backends_mutex);
  return std::any_of(m_backends.begin(), m_backends.end(),
                     [](const auto& backend) { return backend->IsReady(); });
}

static void CheckForDisconnectedWiimotes()
{
  std::lock_guard<std::mutex> lk(g_wiimotes_mutex);
  for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
    if (g_wiimotes[i] && !g_wiimotes[i]->IsConnected())
      HandleWiimoteDisconnect(i);
}

void WiimoteScanner::ThreadFunc()
{
  Common::SetCurrentThreadName("Wiimote Scanning Thread");

  NOTICE_LOG(WIIMOTE, "Wiimote scanning thread has started.");

  // Create and destroy scanner backends here to ensure all operations stay on the same thread. The
  // HIDAPI backend on macOS has an error condition when IOHIDManagerCreate and IOHIDManagerClose
  // are called on different threads (and so reference different CFRunLoops) which can cause an
  // EXC_BAD_ACCES crash.
  {
    std::lock_guard<std::mutex> lg(m_backends_mutex);

    m_backends.emplace_back(std::make_unique<WiimoteScannerLinux>());
    m_backends.emplace_back(std::make_unique<WiimoteScannerAndroid>());
    m_backends.emplace_back(std::make_unique<WiimoteScannerWindows>());
    m_backends.emplace_back(std::make_unique<WiimoteScannerDarwin>());
    m_backends.emplace_back(std::make_unique<WiimoteScannerHidapi>());
  }

  while (m_scan_thread_running.IsSet())
  {
    m_scan_mode_changed_event.WaitFor(std::chrono::milliseconds(500));

    CheckForDisconnectedWiimotes();

    if (m_scan_mode.load() == WiimoteScanMode::DO_NOT_SCAN)
      continue;

    for (const auto& backend : m_backends)
    {
      if (CalculateWantedWiimotes() != 0 || CalculateWantedBB() != 0)
      {
        std::vector<Wiimote*> found_wiimotes;
        Wiimote* found_board = nullptr;
        backend->FindWiimotes(found_wiimotes, found_board);
        {
          if (!g_real_wiimotes_initialized)
            continue;
          std::lock_guard<std::mutex> lk(g_wiimotes_mutex);
          std::for_each(found_wiimotes.begin(), found_wiimotes.end(), TryToConnectWiimote);
          if (found_board)
            TryToConnectBalanceBoard(found_board);
        }
      }
      else
      {
        backend->Update();  // Does stuff needed to detect disconnects on Windows
      }
    }

    if (m_scan_mode.load() == WiimoteScanMode::SCAN_ONCE)
      m_scan_mode.store(WiimoteScanMode::DO_NOT_SCAN);
  }

  {
    std::lock_guard<std::mutex> lg(m_backends_mutex);
    m_backends.clear();
  }
  NOTICE_LOG(WIIMOTE, "Wiimote scanning thread has stopped.");
}

bool Wiimote::Connect(int index)
{
  m_index = index;
  m_need_prepare.Set();

  if (!m_run_thread.IsSet())
  {
    StartThread();
    m_thread_ready_event.Wait();
  }
  return IsConnected();
}

void Wiimote::StartThread()
{
  m_wiimote_thread = std::thread(&Wiimote::ThreadFunc, this);
}

void Wiimote::StopThread()
{
  if (!m_run_thread.TestAndClear())
    return;
  IOWakeup();
  m_wiimote_thread.join();
}

void Wiimote::ThreadFunc()
{
  Common::SetCurrentThreadName("Wiimote Device Thread");

  bool ok = ConnectInternal();

  if (!ok)
  {
    // try again, it might take a moment to settle
    Common::SleepCurrentThread(100);
    ok = ConnectInternal();
  }

  m_thread_ready_event.Set();
  m_run_thread.Set();

  if (!ok)
  {
    return;
  }

  // main loop
  while (IsConnected() && m_run_thread.IsSet())
  {
    if (m_need_prepare.TestAndClear() && !PrepareOnThread())
    {
      ERROR_LOG(WIIMOTE, "Wiimote::PrepareOnThread failed.  Disconnecting Wiimote %d.",
                m_index + 1);
      break;
    }
    if (!Write())
    {
      ERROR_LOG(WIIMOTE, "Wiimote::Write failed.  Disconnecting Wiimote %d.", m_index + 1);
      break;
    }
    Read();
  }

  DisconnectInternal();
}

int Wiimote::GetIndex() const
{
  return m_index;
}

void LoadSettings()
{
  std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + WIIMOTE_INI_NAME ".ini";

  IniFile inifile;
  inifile.Load(ini_filename);

  for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
  {
    std::string secname("Wiimote");
    secname += static_cast<char>('1' + i);
    IniFile::Section& sec = *inifile.GetOrCreateSection(secname);

    sec.Get("Source", &g_wiimote_sources[i], i ? WIIMOTE_SRC_NONE : WIIMOTE_SRC_EMU);
  }

  std::string secname("BalanceBoard");
  IniFile::Section& sec = *inifile.GetOrCreateSection(secname);
  sec.Get("Source", &g_wiimote_sources[WIIMOTE_BALANCE_BOARD], WIIMOTE_SRC_NONE);
}

// config dialog calls this when some settings change
void Initialize(::Wiimote::InitializeMode init_mode)
{
  if (!g_real_wiimotes_initialized)
  {
    s_known_ids.clear();
    g_wiimote_scanner.StartThread();
  }

  if (SConfig::GetInstance().m_WiimoteContinuousScanning &&
      !SConfig::GetInstance().m_bt_passthrough_enabled)
    g_wiimote_scanner.SetScanMode(WiimoteScanMode::CONTINUOUSLY_SCAN);
  else
    g_wiimote_scanner.SetScanMode(WiimoteScanMode::DO_NOT_SCAN);

  // wait for connection because it should exist before state load
  if (init_mode == ::Wiimote::InitializeMode::DO_WAIT_FOR_WIIMOTES)
  {
    int timeout = 100;
    g_wiimote_scanner.SetScanMode(WiimoteScanMode::SCAN_ONCE);
    while (CalculateWantedWiimotes() > CalculateConnectedWiimotes() && timeout)
    {
      Common::SleepCurrentThread(100);
      timeout--;
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

// called when the Dolphin app exits
void Shutdown()
{
  g_real_wiimotes_initialized = false;
  g_wiimote_scanner.StopThread();

  NOTICE_LOG(WIIMOTE, "WiimoteReal::Shutdown");

  std::lock_guard<std::mutex> lk(g_wiimotes_mutex);
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
  const int previous_source = g_wiimote_sources[index];
  g_wiimote_sources[index] = source;
  {
    // kill real connection (or swap to different slot)
    std::lock_guard<std::mutex> lk(g_wiimotes_mutex);

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

  // reconnect to the emulator
  Core::RunAsCPUThread([index, previous_source, source] {
    if (previous_source != WIIMOTE_SRC_NONE)
      ::Wiimote::Connect(index, false);
    if (source & WIIMOTE_SRC_EMU)
      ::Wiimote::Connect(index, true);
  });
}

// Called from the Wiimote scanner thread
static bool TryToConnectWiimoteToSlot(Wiimote* wm, unsigned int i)
{
  if (WIIMOTE_SRC_REAL & g_wiimote_sources[i] && !g_wiimotes[i])
  {
    if (wm->Connect(i))
    {
      NOTICE_LOG(WIIMOTE, "Connected to Wiimote %i.", i + 1);
      g_wiimotes[i] = wm;
      Core::RunAsCPUThread([i] { ::Wiimote::Connect(i, true); });
      std::lock_guard<std::mutex> lk(s_known_ids_mutex);
      s_known_ids.insert(wm->GetId());
    }
    return true;
  }
  return false;
}

void TryToConnectWiimote(Wiimote* wm)
{
  for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
  {
    if (TryToConnectWiimoteToSlot(wm, i))
    {
      wm = nullptr;
      break;
    }
  }
  delete wm;
}

void TryToConnectBalanceBoard(Wiimote* wm)
{
  if (TryToConnectWiimoteToSlot(wm, WIIMOTE_BALANCE_BOARD))
  {
    wm = nullptr;
  }
  delete wm;
}

void HandleWiimoteDisconnect(int index)
{
  Wiimote* wm = nullptr;
  std::swap(wm, g_wiimotes[index]);
  if (wm)
  {
    std::lock_guard<std::mutex> lk(s_known_ids_mutex);
    s_known_ids.erase(wm->GetId());
    delete wm;
    NOTICE_LOG(WIIMOTE, "Disconnected Wiimote %i.", index + 1);
  }
}

// This is called from the GUI thread
void Refresh()
{
  if (!SConfig::GetInstance().m_WiimoteContinuousScanning)
    g_wiimote_scanner.SetScanMode(WiimoteScanMode::SCAN_ONCE);
}

void InterruptChannel(int wiimote_number, u16 channel_id, const void* data, u32 size)
{
  std::lock_guard<std::mutex> lk(g_wiimotes_mutex);
  if (g_wiimotes[wiimote_number])
    g_wiimotes[wiimote_number]->InterruptChannel(channel_id, data, size);
}

void ControlChannel(int wiimote_number, u16 channel_id, const void* data, u32 size)
{
  std::lock_guard<std::mutex> lk(g_wiimotes_mutex);
  if (g_wiimotes[wiimote_number])
    g_wiimotes[wiimote_number]->ControlChannel(channel_id, data, size);
}

// Read the Wiimote once
void Update(int wiimote_number)
{
  // Try to get a lock and return without doing anything if we fail
  // This avoids blocking the CPU thread
  if (!g_wiimotes_mutex.try_lock())
    return;

  if (g_wiimotes[wiimote_number])
    g_wiimotes[wiimote_number]->Update();

  g_wiimotes_mutex.unlock();

  // Wiimote::Update() may remove the Wiimote if it was disconnected.
  if (!g_wiimotes[wiimote_number])
    ::Wiimote::Connect(wiimote_number, false);
}

bool CheckForButtonPress(int wiimote_number)
{
  if (!g_wiimotes_mutex.try_lock())
    return false;

  bool button_pressed = false;

  if (g_wiimotes[wiimote_number])
    button_pressed = g_wiimotes[wiimote_number]->CheckForButtonPress();

  g_wiimotes_mutex.unlock();
  return button_pressed;
}

bool IsValidDeviceName(const std::string& name)
{
  return "Nintendo RVL-CNT-01" == name || "Nintendo RVL-CNT-01-TR" == name ||
         IsBalanceBoardName(name);
}

bool IsBalanceBoardName(const std::string& name)
{
  return "Nintendo RVL-WBC-01" == name;
}

// This is called from the scanner backends (currently on the scanner thread).
bool IsNewWiimote(const std::string& identifier)
{
  std::lock_guard<std::mutex> lk(s_known_ids_mutex);
  return s_known_ids.count(identifier) == 0;
}

};  // end of namespace
