// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <queue>
#include <unordered_set>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Swap.h"
#include "Common/Thread.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteReal/IOAndroid.h"
#include "Core/HW/WiimoteReal/IOLinux.h"
#include "Core/HW/WiimoteReal/IOWin.h"
#include "Core/HW/WiimoteReal/IOhidapi.h"
#include "Core/System.h"

#include "InputCommon/ControllerInterface/Wiimote/WiimoteController.h"
#include "InputCommon/InputConfig.h"

#include "SFML/Network.hpp"

namespace WiimoteReal
{
using namespace WiimoteCommon;

static void TryToConnectBalanceBoard(std::unique_ptr<Wiimote>);
static bool TryToConnectWiimoteToSlot(std::unique_ptr<Wiimote>&, unsigned int);
static void HandleWiimoteDisconnect(int index);

static bool s_real_wiimotes_initialized = false;

// This is used to store connected Wiimotes' IDs, so we don't connect
// more than once to the same device.
static std::unordered_set<std::string> s_known_ids;
static std::mutex s_known_ids_mutex;

std::recursive_mutex g_wiimotes_mutex;

// Real wii remotes assigned to a particular slot.
// Assignments must be done from the CPU thread with the above mutex held.
std::unique_ptr<Wiimote> g_wiimotes[MAX_BBMOTES];

struct WiimotePoolEntry
{
  using Clock = std::chrono::steady_clock;

  std::unique_ptr<Wiimote> wiimote;
  Clock::time_point entry_time = Clock::now();

  bool IsExpired() const
  {
    // Keep wii remotes in the pool for a bit before disconnecting them.
    constexpr auto POOL_TIME = std::chrono::seconds{5};

    return (Clock::now() - entry_time) > POOL_TIME;
  }
};

// Connected wii remotes are placed here when no open slot is set to "Real".
// They are then automatically disconnected after some time.
static std::vector<WiimotePoolEntry> s_wiimote_pool;

static WiimoteScanner s_wiimote_scanner;

// Attempt to fill a real wiimote slot from the pool or by stealing from ControllerInterface.
static void TryToFillWiimoteSlot(u32 index)
{
  std::lock_guard lk(g_wiimotes_mutex);

  if (g_wiimotes[index] ||
      Config::Get(Config::GetInfoForWiimoteSource(index)) != WiimoteSource::Real)
  {
    return;
  }

  // If the pool is empty, attempt to steal from ControllerInterface.
  if (s_wiimote_pool.empty())
  {
    ciface::WiimoteController::ReleaseDevices(1);

    // Still empty?
    if (s_wiimote_pool.empty())
      return;
  }

  if (TryToConnectWiimoteToSlot(s_wiimote_pool.front().wiimote, index))
    s_wiimote_pool.erase(s_wiimote_pool.begin());
}

void PopulateDevices()
{
  // There is a very small chance of deadlocks if we didn't do it in another thread
  s_wiimote_scanner.PopulateDevices();
}

// Attempts to fill enabled real wiimote slots.
// Push/pull wiimotes to/from ControllerInterface as needed.
// Should be called PopulateDevices() to be in line with other implementations.
void ProcessWiimotePool()
{
  std::lock_guard lk(g_wiimotes_mutex);

  for (u32 index = 0; index != MAX_WIIMOTES; ++index)
    TryToFillWiimoteSlot(index);

  if (Config::Get(Config::MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE))
  {
    for (auto& entry : s_wiimote_pool)
      ciface::WiimoteController::AddDevice(std::move(entry.wiimote));

    s_wiimote_pool.clear();
  }
  else
  {
    ciface::WiimoteController::ReleaseDevices();
  }
}

bool IsScannerReady()
{
  return s_wiimote_scanner.IsReady();
}

void AddWiimoteToPool(std::unique_ptr<Wiimote> wiimote)
{
  // Our real wiimote class requires an index.
  // Within the pool it's only going to be used for logging purposes.
  static constexpr int POOL_WIIMOTE_INDEX = 99;

  if (!wiimote->Connect(POOL_WIIMOTE_INDEX))
  {
    ERROR_LOG_FMT(WIIMOTE, "Failed to connect real wiimote.");
    return;
  }

  wiimote->EmuStop();

  std::lock_guard lk(g_wiimotes_mutex);
  s_wiimote_pool.emplace_back(WiimotePoolEntry{std::move(wiimote)});
}

Wiimote::Wiimote()
{
  m_config_changed_callback_id = Config::AddConfigChangedCallback([this] { RefreshConfig(); });
  RefreshConfig();
}

Wiimote::~Wiimote()
{
  Config::RemoveConfigChangedCallback(m_config_changed_callback_id);
}

void Wiimote::Shutdown()
{
  std::lock_guard lk(s_known_ids_mutex);
  s_known_ids.erase(GetId());

  StopThread();
  ClearReadQueue();
  m_write_reports.Clear();

  NOTICE_LOG_FMT(WIIMOTE, "Disconnected real wiimote.");
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
  QueueReport(rpt);
}

void Wiimote::ClearReadQueue()
{
  Report rpt;

  // The "Clear" function isn't thread-safe :/
  while (m_read_reports.Pop(rpt))
  {
  }
}

void Wiimote::EventLinked()
{
  m_is_linked = true;

  ClearReadQueue();
  ResetDataReporting();
  EnablePowerAssertionInternal();
}

void Wiimote::EventUnlinked()
{
  if (m_really_disconnect)
    DisconnectInternal();
  else
    EmuStop();
}

void Wiimote::InterruptDataOutput(const u8* data, const u32 size)
{
  Report rpt(size + REPORT_HID_HEADER_SIZE);
  std::copy_n(data, size, rpt.data() + REPORT_HID_HEADER_SIZE);

  // Convert output DATA packets to SET_REPORT packets.
  // Nintendo Wiimotes work without this translation, but 3rd
  // party ones don't.
  rpt[0] = WR_SET_REPORT | BT_OUTPUT;

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
           (!m_speaker_enabled_in_dolphin_config || !m_speaker_enable || m_speaker_mute))
  {
    rpt.resize(3);
    // Translate undesired speaker data reports into rumble reports.
    rpt[1] = u8(OutputReportID::Rumble);
    // Keep only the rumble bit.
    rpt[2] &= 0x1;
  }

  WriteReport(std::move(rpt));
}

void Wiimote::Read()
{
  Report rpt(MAX_PAYLOAD);
  auto const result = IORead(rpt.data());

  if (0 == result)
  {
    ERROR_LOG_FMT(WIIMOTE, "Wiimote::IORead failed. Disconnecting Wii Remote {}.", m_index + 1);
    DisconnectInternal();

    return;
  }

  // Drop the report if not connected.
  if (!m_is_linked)
    return;

  if (result > 0)
  {
    if (m_balance_board_dump_port > 0 && m_index == WIIMOTE_BALANCE_BOARD)
    {
      static sf::UdpSocket Socket;
      Socket.send((char*)rpt.data(), rpt.size(), sf::IpAddress::LocalHost,
                  m_balance_board_dump_port);
    }

    // Add it to queue
    rpt.resize(result);
    m_read_reports.Push(std::move(rpt));
  }
}

bool Wiimote::Write()
{
  // nothing written, but this is not an error
  if (m_write_reports.Empty())
    return true;

  Report const& rpt = m_write_reports.Front();

  if (m_balance_board_dump_port > 0 && m_index == WIIMOTE_BALANCE_BOARD)
  {
    static sf::UdpSocket Socket;
    Socket.send((char*)rpt.data(), rpt.size(), sf::IpAddress::LocalHost, m_balance_board_dump_port);
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
    ERROR_LOG_FMT(WIIMOTE, "IsBalanceBoard(): Failed to initialise extension.");
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
        ERROR_LOG_FMT(WIIMOTE, "IsBalanceBoard(): Received unexpected data reply for address {:X}",
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
        WARN_LOG_FMT(WIIMOTE,
                     "Failed to read from 0xa400fe, assuming Wiimote is not a Balance Board.");
        return false;
      }
      break;
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

bool Wiimote::GetNextReport(Report* report)
{
  return m_read_reports.Pop(*report);
}

// Returns the next report that should be sent
Report& Wiimote::ProcessReadQueue(bool repeat_last_data_report)
{
  // If we're not repeating data reports or had a non-data report, any old report is irrelevant.
  if (!repeat_last_data_report || !IsDataReport(m_last_input_report))
    m_last_input_report.clear();

  // Step through the read queue.
  while (GetNextReport(&m_last_input_report))
  {
    // Stop on a non-data report.
    if (!IsDataReport(m_last_input_report))
      break;
  }

  return m_last_input_report;
}

u8 Wiimote::GetWiimoteDeviceIndex() const
{
  return m_bt_device_index;
}

void Wiimote::SetWiimoteDeviceIndex(u8 index)
{
  m_bt_device_index = index;
}

void Wiimote::PrepareInput(WiimoteEmu::DesiredWiimoteState* target_state,
                           SensorBarState sensor_bar_state)
{
  // Nothing to do here on real Wiimotes.
}

void Wiimote::Update(const WiimoteEmu::DesiredWiimoteState& target_state)
{
  // Wii remotes send input at 200hz once a Wii enables "sniff mode" on the connection.
  // PC bluetooth stacks do not enable sniff mode causing remotes to send input at only 100hz.
  //
  // Commercial games do not send speaker data unless input rates approach 200hz.
  // Some games will also present input issues.
  // e.g. Super Mario Galaxy's star cursor drops in and out.
  //
  // If we want speaker data we must pretend input is at 200hz.
  // We duplicate data reports to accomplish this.
  // Unfortunately this breaks detection of motion gestures in some games.
  // e.g. Sonic and the Secret Rings, Jett Rocket
  const bool repeat_reports_to_maintain_200hz =
      Config::Get(Config::MAIN_REAL_WII_REMOTE_REPEAT_REPORTS);

  const Report& rpt = ProcessReadQueue(repeat_reports_to_maintain_200hz);

  if (rpt.empty())
    return;

  InterruptCallback(rpt.front(), rpt.data() + REPORT_HID_HEADER_SIZE,
                    u32(rpt.size() - REPORT_HID_HEADER_SIZE));
}

ButtonData Wiimote::GetCurrentlyPressedButtons()
{
  Report& rpt = m_last_input_report;
  if (rpt.size() >= 4)
  {
    const auto mode = InputReportID(rpt[1]);

    // TODO: Button data could also be pulled out of non-data reports if really wanted.
    if (DataReportBuilder::IsValidMode(mode))
    {
      auto builder = MakeDataReportManipulator(mode, rpt.data() + 2);
      ButtonData buttons = {};
      builder->GetCoreData(&buttons);

      return buttons;
    }
  }
  return ButtonData{};
}

void Wiimote::Prepare()
{
  m_need_prepare.Set();
  IOWakeup();
}

bool Wiimote::PrepareOnThread()
{
  // Set reporting mode to non-continuous core buttons and turn on rumble.
  u8 static const mode_report[] = {WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::ReportMode), 1,
                                   u8(InputReportID::ReportCore)};

  // Request status and turn off rumble.
  u8 static const req_status_report[] = {WR_SET_REPORT | BT_OUTPUT,
                                         u8(OutputReportID::RequestStatus), 0};

  return IOWrite(mode_report, sizeof(mode_report)) &&
         (Common::SleepCurrentThread(200), IOWrite(req_status_report, sizeof(req_status_report)));
}

void Wiimote::EmuStop()
{
  m_is_linked = false;

  ResetDataReporting();
  DisablePowerAssertionInternal();
}

void Wiimote::EmuResume()
{
  EnablePowerAssertionInternal();
}

void Wiimote::EmuPause()
{
  DisablePowerAssertionInternal();
}

static unsigned int CalculateWantedWiimotes()
{
  std::lock_guard lk(g_wiimotes_mutex);
  // Figure out how many real Wiimotes are required
  unsigned int wanted_wiimotes = 0;
  for (int i = 0; i < MAX_WIIMOTES; ++i)
  {
    if (Config::Get(Config::GetInfoForWiimoteSource(i)) == WiimoteSource::Real && !g_wiimotes[i])
      ++wanted_wiimotes;
  }
  return wanted_wiimotes;
}

static unsigned int CalculateWantedBB()
{
  std::lock_guard lk(g_wiimotes_mutex);
  unsigned int wanted_bb = 0;
  if (Config::Get(Config::WIIMOTE_BB_SOURCE) == WiimoteSource::Real &&
      !g_wiimotes[WIIMOTE_BALANCE_BOARD])
  {
    ++wanted_bb;
  }
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
  if (m_scan_thread_running.IsSet())
  {
    SetScanMode(WiimoteScanMode::DO_NOT_SCAN);

    for (const auto& backend : m_backends)
    {
      backend->RequestStopSearching();
    }

    m_scan_thread_running.Clear();
    m_scan_thread.join();
  }
}

void WiimoteScanner::SetScanMode(WiimoteScanMode scan_mode)
{
  m_scan_mode.store(scan_mode);
  m_scan_mode_changed_or_population_event.Set();
}

bool WiimoteScanner::IsReady() const
{
  std::lock_guard lg(m_backends_mutex);
  return std::ranges::any_of(m_backends, [](const auto& backend) { return backend->IsReady(); });
}

static void CheckForDisconnectedWiimotes()
{
  std::lock_guard lk(g_wiimotes_mutex);
  for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
    if (g_wiimotes[i] && !g_wiimotes[i]->IsConnected())
      HandleWiimoteDisconnect(i);
}

void WiimoteScanner::PoolThreadFunc()
{
  Common::SetCurrentThreadName("Wiimote Pool Thread");

  // Toggle between 1010 and 0101.
  u8 led_value = 0b1010;

  auto next_time = std::chrono::steady_clock::now();

  while (m_scan_thread_running.IsSet())
  {
    std::this_thread::sleep_until(next_time);
    next_time += std::chrono::milliseconds(250);

    std::lock_guard lk(g_wiimotes_mutex);

    // Remove stale pool entries.
    for (auto it = s_wiimote_pool.begin(); it != s_wiimote_pool.end();)
    {
      if (!it->wiimote->IsConnected())
      {
        INFO_LOG_FMT(WIIMOTE, "Removing disconnected wiimote pool entry.");
        it = s_wiimote_pool.erase(it);
      }
      else if (it->IsExpired())
      {
        INFO_LOG_FMT(WIIMOTE, "Removing expired wiimote pool entry.");
        it = s_wiimote_pool.erase(it);
      }
      else
      {
        ++it;
      }
    }

    // Make wiimote pool LEDs dance.
    for (auto& wiimote : s_wiimote_pool)
    {
      OutputReportLeds leds = {};
      leds.leds = led_value;
      wiimote.wiimote->QueueReport(leds);
    }

    led_value ^= 0b1111;
  }
}

void WiimoteScanner::PopulateDevices()
{
  m_populate_devices.Set();
  m_scan_mode_changed_or_population_event.Set();
}

void WiimoteScanner::ThreadFunc()
{
  std::thread pool_thread(&WiimoteScanner::PoolThreadFunc, this);

  Common::SetCurrentThreadName("Wiimote Scanning Thread");

  NOTICE_LOG_FMT(WIIMOTE, "Wiimote scanning thread has started.");

  // Create and destroy scanner backends here to ensure all operations stay on the same thread. The
  // HIDAPI backend on macOS has an error condition when IOHIDManagerCreate and IOHIDManagerClose
  // are called on different threads (and so reference different CFRunLoops) which can cause an
  // EXC_BAD_ACCES crash.
  {
    std::lock_guard lg(m_backends_mutex);

    m_backends.emplace_back(std::make_unique<WiimoteScannerLinux>());
    m_backends.emplace_back(std::make_unique<WiimoteScannerAndroid>());
    m_backends.emplace_back(std::make_unique<WiimoteScannerWindows>());
    m_backends.emplace_back(std::make_unique<WiimoteScannerHidapi>());
  }

  while (m_scan_thread_running.IsSet())
  {
    m_scan_mode_changed_or_population_event.WaitFor(std::chrono::milliseconds(500));

    if (m_populate_devices.TestAndClear())
    {
      g_controller_interface.PlatformPopulateDevices([] { ProcessWiimotePool(); });
    }

    // Does stuff needed to detect disconnects on Windows
    for (const auto& backend : m_backends)
      backend->Update();

    CheckForDisconnectedWiimotes();

    if (m_scan_mode.load() == WiimoteScanMode::DO_NOT_SCAN)
      continue;

    // If we don't want Wiimotes in ControllerInterface, we may not need them at all.
    if (!Config::Get(Config::MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE))
    {
      auto& system = Core::System::GetInstance();
      // We don't want any remotes in passthrough mode or running in GC mode.
      const bool core_running = Core::GetState(system) != Core::State::Uninitialized;
      if (Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED) ||
          (core_running && !system.IsWii()))
      {
        continue;
      }

      // We don't want any remotes if we already connected everything we need.
      if (0 == CalculateWantedWiimotes() && 0 == CalculateWantedBB())
        continue;
    }

    for (const auto& backend : m_backends)
    {
      std::vector<Wiimote*> found_wiimotes;
      Wiimote* found_board = nullptr;
      backend->FindWiimotes(found_wiimotes, found_board);
      {
        std::unique_lock wm_lk(g_wiimotes_mutex);

        for (auto* wiimote : found_wiimotes)
        {
          {
            std::lock_guard lk(s_known_ids_mutex);
            s_known_ids.insert(wiimote->GetId());
          }

          AddWiimoteToPool(std::unique_ptr<Wiimote>(wiimote));
          g_controller_interface.PlatformPopulateDevices([] { ProcessWiimotePool(); });
        }

        if (found_board)
        {
          {
            std::lock_guard lk(s_known_ids_mutex);
            s_known_ids.insert(found_board->GetId());
          }

          TryToConnectBalanceBoard(std::unique_ptr<Wiimote>(found_board));
        }
      }
    }

    // Stop scanning if not in continuous mode.
    auto scan_mode = WiimoteScanMode::SCAN_ONCE;
    m_scan_mode.compare_exchange_strong(scan_mode, WiimoteScanMode::DO_NOT_SCAN);
  }

  {
    std::lock_guard lg(m_backends_mutex);
    m_backends.clear();
  }

  pool_thread.join();

  NOTICE_LOG_FMT(WIIMOTE, "Wiimote scanning thread has stopped.");
}

bool Wiimote::Connect(int index)
{
  m_index = index;

  if (!m_run_thread.IsSet())
  {
    m_need_prepare.Set();
    m_run_thread.Set();
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

  if (!ok)
  {
    return;
  }

  // main loop
  while (IsConnected() && m_run_thread.IsSet())
  {
    if (m_need_prepare.TestAndClear() && !PrepareOnThread())
    {
      ERROR_LOG_FMT(WIIMOTE, "Wiimote::PrepareOnThread failed.  Disconnecting Wiimote {}.",
                    m_index + 1);
      break;
    }
    if (!Write())
    {
      ERROR_LOG_FMT(WIIMOTE, "Wiimote::Write failed.  Disconnecting Wiimote {}.", m_index + 1);
      break;
    }
    Read();
  }

  DisconnectInternal();
}

void Wiimote::RefreshConfig()
{
  m_speaker_enabled_in_dolphin_config = Config::Get(Config::MAIN_WIIMOTE_ENABLE_SPEAKER);
  m_balance_board_dump_port = Config::Get(Config::MAIN_BB_DUMP_PORT);
}

int Wiimote::GetIndex() const
{
  return m_index;
}

// config dialog calls this when some settings change
void Initialize(::Wiimote::InitializeMode init_mode)
{
  if (!s_real_wiimotes_initialized)
  {
    s_wiimote_scanner.StartThread();
  }

  if (Config::Get(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING))
    s_wiimote_scanner.SetScanMode(WiimoteScanMode::CONTINUOUSLY_SCAN);
  else
    s_wiimote_scanner.SetScanMode(WiimoteScanMode::DO_NOT_SCAN);

  // wait for connection because it should exist before state load
  if (init_mode == ::Wiimote::InitializeMode::DO_WAIT_FOR_WIIMOTES)
  {
    int timeout = 100;
    s_wiimote_scanner.SetScanMode(WiimoteScanMode::SCAN_ONCE);
    while (CalculateWantedWiimotes() && timeout)
    {
      Common::SleepCurrentThread(100);
      timeout--;
    }
  }

  if (s_real_wiimotes_initialized)
    return;

  NOTICE_LOG_FMT(WIIMOTE, "WiimoteReal::Initialize");

  s_real_wiimotes_initialized = true;
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
  s_real_wiimotes_initialized = false;
  s_wiimote_scanner.StopThread();

  NOTICE_LOG_FMT(WIIMOTE, "WiimoteReal::Shutdown");

  std::lock_guard lk(g_wiimotes_mutex);
  for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
    HandleWiimoteDisconnect(i);

  // Release remotes from ControllerInterface and empty the pool.
  ciface::WiimoteController::ReleaseDevices();
  s_wiimote_pool.clear();
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

// Called from the Wiimote scanner thread (or UI thread on source change)
static bool TryToConnectWiimoteToSlot(std::unique_ptr<Wiimote>& wm, unsigned int i)
{
  if (Config::Get(Config::GetInfoForWiimoteSource(i)) != WiimoteSource::Real || g_wiimotes[i])
    return false;

  if (!wm->Connect(i))
  {
    ERROR_LOG_FMT(WIIMOTE, "Failed to connect real wiimote.");
    return false;
  }

  wm->Prepare();

  // Set LEDs.
  OutputReportLeds led_report = {};
  led_report.leds = u8(1 << (i % WIIMOTE_BALANCE_BOARD));
  wm->QueueReport(led_report);

  {
    const Core::CPUThreadGuard guard(Core::System::GetInstance());
    g_wiimotes[i] = std::move(wm);
    WiimoteCommon::UpdateSource(i);
  }

  NOTICE_LOG_FMT(WIIMOTE, "Connected real wiimote to slot {}.", i + 1);

  return true;
}

static void TryToConnectBalanceBoard(std::unique_ptr<Wiimote> wm)
{
  if (TryToConnectWiimoteToSlot(wm, WIIMOTE_BALANCE_BOARD))
    return;

  NOTICE_LOG_FMT(WIIMOTE, "No open slot for real balance board.");
}

static void HandleWiimoteDisconnect(int index)
{
  const Core::CPUThreadGuard guard(Core::System::GetInstance());
  // The Wii Remote object must exist through the call to UpdateSource
  // to prevent WiimoteDevice from having a dangling HIDWiimote pointer.
  const auto temp_real_wiimote = std::move(g_wiimotes[index]);
  WiimoteCommon::UpdateSource(index);
}

// This is called from the GUI thread
void Refresh()
{
  if (!Config::Get(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING))
    s_wiimote_scanner.SetScanMode(WiimoteScanMode::SCAN_ONCE);
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
  std::lock_guard lk(s_known_ids_mutex);
  return !s_known_ids.contains(identifier);
}

void HandleWiimoteSourceChange(unsigned int index)
{
  std::lock_guard wm_lk(g_wiimotes_mutex);

  {
    const Core::CPUThreadGuard guard(Core::System::GetInstance());
    if (auto removed_wiimote = std::move(g_wiimotes[index]))
      AddWiimoteToPool(std::move(removed_wiimote));
  }
  g_controller_interface.PlatformPopulateDevices([] { ProcessWiimotePool(); });
}

void HandleWiimotesInControllerInterfaceSettingChange()
{
  g_controller_interface.PlatformPopulateDevices([] { ProcessWiimotePool(); });
}

}  // namespace WiimoteReal
