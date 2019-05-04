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
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Swap.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteReal/IOAndroid.h"
#include "Core/HW/WiimoteReal/IOLinux.h"
#include "Core/HW/WiimoteReal/IOWin.h"
#include "Core/HW/WiimoteReal/IOdarwin.h"
#include "Core/HW/WiimoteReal/IOhidapi.h"
#include "InputCommon/InputConfig.h"

#include "SFML/Network.hpp"

std::array<std::atomic<u32>, MAX_BBMOTES> g_wiimote_sources;

namespace WiimoteReal
{
using namespace WiimoteCommon;

static void TryToConnectBalanceBoard(std::unique_ptr<Wiimote>);
static void TryToConnectWiimote(std::unique_ptr<Wiimote>);
static void HandleWiimoteDisconnect(int index);

static bool g_real_wiimotes_initialized = false;

// This is used to store connected Wiimotes' IDs, so we don't connect
// more than once to the same device.
static std::unordered_set<std::string> s_known_ids;
static std::mutex s_known_ids_mutex;

std::mutex g_wiimotes_mutex;

std::unique_ptr<Wiimote> g_wiimotes[MAX_BBMOTES];
WiimoteScanner g_wiimote_scanner;

Wiimote::Wiimote() = default;

void Wiimote::Shutdown()
{
  std::lock_guard<std::mutex> lk(s_known_ids_mutex);
  s_known_ids.erase(GetId());

  StopThread();
  ClearReadQueue();
  m_write_reports.Clear();

  NOTICE_LOG(WIIMOTE, "Disconnected real wiimote.");
}

// to be called from CPU thread
void Wiimote::WriteReport(Report rpt)
{
  if (rpt.size() >= 3)
  {
    bool const new_rumble_state = (rpt[2] & 0x1) != 0;

    switch (WiimoteCommon::OutputReportID(rpt[1]))
    {
    case OutputReportID::Rumble:
      // If this is a rumble report and the rumble state didn't change, we can drop this report.
      if (new_rumble_state == m_rumble_state)
        return;
      break;

    case OutputReportID::SpeakerEnable:
      m_speaker_enable = (rpt[2] & 0x4) != 0;
      break;

    case OutputReportID::SpeakerMute:
      m_speaker_mute = (rpt[2] & 0x4) != 0;
      break;

    case OutputReportID::ReportMode:
      // Force non-continuous reporting for less BT traffic.
      // We duplicate reports to maintain 200hz anyways.
      rpt[2] &= ~0x4;
      break;

    default:
      break;
    }

    m_rumble_state = new_rumble_state;
  }

  m_write_reports.Push(std::move(rpt));
  IOWakeup();
}

// to be called from CPU thread
void Wiimote::QueueReport(WiimoteCommon::OutputReportID rpt_id, const void* data, unsigned int size)
{
  auto const queue_data = static_cast<const u8*>(data);

  Report rpt(size + 2);
  rpt[0] = WR_SET_REPORT | BT_OUTPUT;
  rpt[1] = u8(rpt_id);
  std::copy_n(queue_data, size, rpt.begin() + 2);
  WriteReport(std::move(rpt));
}

void Wiimote::ResetDataReporting()
{
  m_last_input_report.clear();

  // "core" reporting in non-continuous mode is a wiimote's initial state.
  // FYI: This also disables rumble.
  OutputReportMode rpt = {};
  rpt.mode = InputReportID::ReportCore;
  rpt.continuous = 0;
  QueueReport(OutputReportID::ReportMode, &rpt, sizeof(rpt));
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
    {
      DisconnectInternal();
    }
    else
    {
      EmuStop();
    }
  }
  else
  {
    InterruptChannel(channel, data, size);
    const auto& hidp = *static_cast<const HIDPacket*>(data);
    if (hidp.type == HID_TYPE_SET_REPORT)
    {
      u8 handshake = HID_HANDSHAKE_SUCCESS;
      Core::Callback_WiimoteInterruptChannel(m_index, channel, &handshake, sizeof(handshake));
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

  // Convert output DATA packets to SET_REPORT packets.
  // Nintendo Wiimotes work without this translation, but 3rd
  // party ones don't.
  if (rpt[0] == 0xa2)
  {
    rpt[0] = WR_SET_REPORT | BT_OUTPUT;
  }

  // Disallow games from turning off all of the LEDs.
  // It makes Wiimote connection status confusing.
  if (rpt[1] == u8(OutputReportID::LED))
  {
    auto& leds_rpt = *reinterpret_cast<OutputReportLeds*>(&rpt[2]);
    if (0 == leds_rpt.leds)
    {
      // Turn on ALL of the LEDs.
      leds_rpt.leds = 0xf;
    }
  }
  else if (rpt[1] == u8(OutputReportID::SpeakerData) &&
           (!SConfig::GetInstance().m_WiimoteEnableSpeaker || !m_speaker_enable || m_speaker_mute))
  {
    // Translate undesired speaker data reports into rumble reports.
    rpt[1] = u8(OutputReportID::Rumble);
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
  // TODO: Use the structs for building these reports..
  static const u8 init_extension_rpt1[MAX_PAYLOAD] = {
      WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::WriteData), 0x04, 0xa4, 0x00, 0xf0, 0x01, 0x55};
  static const u8 init_extension_rpt2[MAX_PAYLOAD] = {
      WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::WriteData), 0x04, 0xa4, 0x00, 0xfb, 0x01, 0x00};
  static const u8 status_report[] = {WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::RequestStatus),
                                     0};
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

    switch (InputReportID(buf[1]))
    {
    case InputReportID::Status:
    {
      const auto* status = reinterpret_cast<InputReportStatus*>(&buf[2]);
      // A Balance Board has a Balance Board extension.
      if (!status->extension)
        return false;
      // Read two bytes from 0xa400fe to identify the extension.
      static const u8 identify_ext_rpt[] = {WR_SET_REPORT | BT_OUTPUT,
                                            u8(OutputReportID::ReadData),
                                            0x04,
                                            0xa4,
                                            0x00,
                                            0xfe,
                                            0x02,
                                            0x00};
      ret = IOWrite(identify_ext_rpt, sizeof(identify_ext_rpt));
      break;
    }
    case InputReportID::ReadDataReply:
    {
      const auto* reply = reinterpret_cast<InputReportReadDataReply*>(&buf[2]);
      if (Common::swap16(reply->address) != 0x00fe)
      {
        ERROR_LOG(WIIMOTE, "IsBalanceBoard(): Received unexpected data reply for address %X",
                  Common::swap16(reply->address));
        return false;
      }
      // A Balance Board ext can be identified by checking for 0x0402.
      return reply->data[0] == 0x04 && reply->data[1] == 0x02;
    }
    case InputReportID::Ack:
    {
      const auto* ack = reinterpret_cast<InputReportAck*>(&buf[2]);
      if (ack->rpt_id == OutputReportID::ReadData && ack->error_code != ErrorCode::Success)
      {
        WARN_LOG(WIIMOTE, "Failed to read from 0xa400fe, assuming Wiimote is not a Balance Board.");
        return false;
      }
    }
    default:
      break;
    }
  }
  return false;
}

static bool IsDataReport(const Report& rpt)
{
  return rpt.size() >= 2 && rpt[1] >= u8(InputReportID::ReportCore);
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
  Report& rpt = ProcessReadQueue();
  if (rpt.size() >= 4)
  {
    const auto mode = InputReportID(rpt[1]);

    // TODO: Button data could also be pulled out of non-data reports if really wanted.
    if (DataReportBuilder::IsValidMode(mode))
    {
      auto builder = MakeDataReportManipulator(mode, rpt.data() + 2);
      ButtonData buttons = {};
      builder->GetCoreData(&buttons);

      return buttons.hex != 0;
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
  // TODO: use the structs..
  u8 static const mode_report[] = {WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::ReportMode), 0,
                                   u8(InputReportID::ReportCore)};

  // Set the active LEDs and turn on rumble.
  u8 static led_report[] = {WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::LED), 0};
  led_report[2] = u8(u8(LED::LED_1) << (m_index % WIIMOTE_BALANCE_BOARD) | 0x1);

  // Turn off rumble
  u8 static const rumble_report[] = {WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::Rumble), 0};

  // Request status report
  u8 static const req_status_report[] = {WR_SET_REPORT | BT_OUTPUT,
                                         u8(OutputReportID::RequestStatus), 0};
  // TODO: check for sane response?

  return (IOWrite(mode_report, sizeof(mode_report)) && IOWrite(led_report, sizeof(led_report)) &&
          (Common::SleepCurrentThread(200), IOWrite(rumble_report, sizeof(rumble_report))) &&
          IOWrite(req_status_report, sizeof(req_status_report)));
}

void Wiimote::EmuStart()
{
  ResetDataReporting();
  EnablePowerAssertionInternal();
}

void Wiimote::EmuStop()
{
  m_channel = 0;
  ResetDataReporting();
  DisablePowerAssertionInternal();
}

void Wiimote::EmuResume()
{
  m_last_input_report.clear();

  EnablePowerAssertionInternal();
}

void Wiimote::EmuPause()
{
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

    if (!g_real_wiimotes_initialized)
      continue;

    // Does stuff needed to detect disconnects on Windows
    for (const auto& backend : m_backends)
      backend->Update();

    if (0 == CalculateWantedWiimotes() && 0 == CalculateWantedBB())
      continue;

    for (const auto& backend : m_backends)
    {
      std::vector<Wiimote*> found_wiimotes;
      Wiimote* found_board = nullptr;
      backend->FindWiimotes(found_wiimotes, found_board);
      {
        std::lock_guard<std::mutex> wm_lk(g_wiimotes_mutex);

        for (auto* wiimote : found_wiimotes)
        {
          {
            std::lock_guard<std::mutex> lk(s_known_ids_mutex);
            s_known_ids.insert(wiimote->GetId());
          }

          TryToConnectWiimote(std::unique_ptr<Wiimote>(wiimote));
        }

        if (found_board)
        {
          {
            std::lock_guard<std::mutex> lk(s_known_ids_mutex);
            s_known_ids.insert(found_board->GetId());
          }

          TryToConnectBalanceBoard(std::unique_ptr<Wiimote>(found_board));
        }
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

    unsigned int source = 0;
    sec.Get("Source", &source, i ? WIIMOTE_SRC_NONE : WIIMOTE_SRC_EMU);
    g_wiimote_sources[i] = source;
  }

  std::string secname("BalanceBoard");
  IniFile::Section& sec = *inifile.GetOrCreateSection(secname);

  unsigned int bb_source = 0;
  sec.Get("Source", &bb_source, WIIMOTE_SRC_NONE);
  g_wiimote_sources[WIIMOTE_BALANCE_BOARD] = bb_source;
}

// config dialog calls this when some settings change
void Initialize(::Wiimote::InitializeMode init_mode)
{
  if (!g_real_wiimotes_initialized)
  {
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

  if (previous_source == source)
  {
    // No change. Do nothing.
    return;
  }

  g_wiimote_sources[index] = source;

  {
    // Kill real wiimote connection (if any) (or swap to different slot)
    std::lock_guard<std::mutex> lk(g_wiimotes_mutex);
    if (auto removed_wiimote = std::move(g_wiimotes[index]))
    {
      // See if we can use this real Wiimote in another slot.
      // Otherwise it will be disconnected.
      TryToConnectWiimote(std::move(removed_wiimote));
    }
  }

  // Reconnect to the emulator.
  Core::RunAsCPUThread([index, previous_source, source] {
    if (previous_source != WIIMOTE_SRC_NONE)
      ::Wiimote::Connect(index, false);

    if (source == WIIMOTE_SRC_EMU)
      ::Wiimote::Connect(index, true);
  });
}

// Called from the Wiimote scanner thread (or UI thread on source change)
static bool TryToConnectWiimoteToSlot(std::unique_ptr<Wiimote>& wm, unsigned int i)
{
  if (WIIMOTE_SRC_REAL != g_wiimote_sources[i] || g_wiimotes[i])
    return false;

  if (!wm->Connect(i))
  {
    ERROR_LOG(WIIMOTE, "Failed to connect real wiimote.");
    return false;
  }

  g_wiimotes[i] = std::move(wm);
  Core::RunAsCPUThread([i] { ::Wiimote::Connect(i, true); });

  NOTICE_LOG(WIIMOTE, "Connected real wiimote to slot %i.", i + 1);

  return true;
}

static void TryToConnectWiimote(std::unique_ptr<Wiimote> wm)
{
  for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
  {
    if (TryToConnectWiimoteToSlot(wm, i))
      return;
  }

  NOTICE_LOG(WIIMOTE, "No open slot for real wiimote.");
}

static void TryToConnectBalanceBoard(std::unique_ptr<Wiimote> wm)
{
  if (TryToConnectWiimoteToSlot(wm, WIIMOTE_BALANCE_BOARD))
    return;

  NOTICE_LOG(WIIMOTE, "No open slot for real balance board.");
}

static void HandleWiimoteDisconnect(int index)
{
  g_wiimotes[index] = nullptr;
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

};  // namespace WiimoteReal
