// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include <algorithm>
#include <mutex>
#include <unordered_set>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Swap.h"
#include "Common/Thread.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/Speaker.h"
#include "Core/HW/WiimoteReal/IOAndroid.h"
#include "Core/HW/WiimoteReal/IOLinux.h"
#include "Core/HW/WiimoteReal/IOWin.h"
#include "Core/HW/WiimoteReal/IOhidapi.h"
#include "Core/System.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Wiimote/WiimoteController.h"

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

    default:
      break;
    }

    m_rumble_state = new_rumble_state;
  }

  auto& core_timing = Core::System::GetInstance().GetCoreTiming();

  // When invoked from the CPU thread, send the report at the proper time.
  // From other threads (e.g. on construction/destruction) send the report as soon as possible.
  const auto report_time =
      Core::IsCPUThread() ? core_timing.GetTargetHostTime(core_timing.GetTicks()) : Clock::now();

  m_write_reports.Emplace(report_time, std::move(rpt));
  m_write_event.Set();
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
  else if (rpt[1] == u8(OutputReportID::SpeakerData) && !m_speaker_enabled_in_dolphin_config)
  {
    rpt.resize(3);
    // Translate undesired speaker data reports into rumble reports.
    rpt[1] = u8(OutputReportID::Rumble);
    // Keep only the rumble bit.
    rpt[2] &= 0x1;
  }

  WriteReport(std::move(rpt));
}

bool Wiimote::Read()
{
  Report rpt(MAX_PAYLOAD);
  auto const result = IORead(rpt.data());

  // Drop the report if not connected.
  if (m_is_linked && result > 0)
  {
    // Add it to queue
    rpt.resize(result);
    m_read_reports.Push(std::move(rpt));
  }

  return result != 0;
}

bool Wiimote::Write(const TimedReport& timed_report)
{
  auto const& rpt = timed_report.report;

  // Write the report at the proper time, mainly for speaker data, not that it will help much.
  std::this_thread::sleep_until(timed_report.time);
  int ret = IOWrite(rpt.data(), rpt.size());

  return ret != 0;
}

bool Wiimote::IsBalanceBoard()
{
  if (!ConnectInternal())
    return false;

  // This writes {0x00} to addr:0x09 of the speaker hardware.
  // This will turn off the speaker and ACK on a Wii Remote.
  // The speaker hardware is not present on the i2c bus of a Balance Board so it will NACK.
  const struct
  {
    u8 hid_type = WR_SET_REPORT | BT_OUTPUT;
    u8 rpt_id = u8(OutputReportID::WriteData);
    OutputReportWriteData payload = {
        .space = u8(AddressSpace::I2CBus),
        .slave_address = WiimoteEmu::SpeakerLogic::I2C_ADDR,
        .address{Common::BigEndianValue<u16>(0x09)},
        .size = 1,
    };
  } turn_off_speaker;
  static_assert(sizeof(turn_off_speaker) == sizeof(OutputReportWriteData) + 2);

  const auto deadline = Clock::now() + std::chrono::milliseconds{1000};

  int rw_result = IOWrite(reinterpret_cast<const u8*>(&turn_off_speaker), sizeof(turn_off_speaker));

  std::array<u8, MAX_PAYLOAD> buf{};
  while (rw_result != 0)
  {
    if (Clock::now() >= deadline)
    {
      WARN_LOG_FMT(WIIMOTE, "IsBalanceBoard timed out. Assuming this is a Wii Remote.");
      break;
    }

    rw_result = IORead(buf.data());
    if (rw_result == -1)
      continue;

    if (InputReportID(buf[1]) == InputReportID::Ack)
    {
      const InputReportAck ack = Common::BitCastPtr<InputReportAck>(buf.data() + 2);
      if (ack.rpt_id == OutputReportID::WriteData)
      {
        // ACK == Wii Remote. NACK == Balance Board.
        return ack.error_code == ErrorCode::Nack;
      }
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
  const auto now = Clock::now();

  // Set reporting mode to non-continuous core buttons and turn on rumble.
  Report mode_report = {WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::ReportMode), 1,
                        u8(InputReportID::ReportCore)};
  m_write_reports.Emplace(now, std::move(mode_report));

  // Request status and turn off rumble.
  Report req_status_report = {WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::RequestStatus), 0};
  m_write_reports.Emplace(now + std::chrono::milliseconds{200}, std::move(req_status_report));

  m_write_event.Set();
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

    // Currently does nothing. To be removed.
    for (const auto& backend : m_backends)
      backend->Update();

    CheckForDisconnectedWiimotes();

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

    // Stop scanning if not in continuous mode.
    auto scan_mode = WiimoteScanMode::SCAN_ONCE;
    m_scan_mode.compare_exchange_strong(scan_mode, WiimoteScanMode::DO_NOT_SCAN);

    // When not scanning we still look for already attached devices.
    // This allows Windows, hidapi, and DolphinBar remotes to be quickly discovered.
    const bool should_perform_inquiry = scan_mode != WiimoteScanMode::DO_NOT_SCAN;

    for (const auto& backend : m_backends)
    {
      auto results =
          should_perform_inquiry ? backend->FindNewWiimotes() : backend->FindAttachedWiimotes();
      {
        std::unique_lock wm_lk(g_wiimotes_mutex);

        for (auto& wiimote : results.wii_remotes)
        {
          {
            std::lock_guard lk(s_known_ids_mutex);
            s_known_ids.insert(wiimote->GetId());
          }

          AddWiimoteToPool(std::move(wiimote));
          g_controller_interface.PlatformPopulateDevices([] { ProcessWiimotePool(); });
        }

        for (auto& bboard : results.balance_boards)
        {
          {
            std::lock_guard lk(s_known_ids_mutex);
            s_known_ids.insert(bboard->GetId());
          }

          TryToConnectBalanceBoard(std::move(bboard));
        }
      }
    }
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
  StartThread();
  return IsConnected();
}

void Wiimote::StartThread()
{
  if (m_write_thread.joinable())
    return;

  m_run_thread.Set();
  // Note that the write thread starts the read thread.
  m_write_thread = std::thread(&Wiimote::WriteThreadFunc, this);

  m_thread_ready_event.Wait();
}

void Wiimote::StopThread()
{
  if (!m_write_thread.joinable())
    return;

  m_run_thread.Clear();
  m_write_event.Set();

  // Note that the write thread stops the read thread.
  m_write_thread.join();
}

void Wiimote::ReadThreadFunc()
{
  Common::SetCurrentThreadName("Wiimote Read Thread");

  while (m_run_thread.IsSet())
  {
    if (!Read())
    {
      WARN_LOG_FMT(WIIMOTE, "Wiimote::Read failed on Wiimote {}.", m_index + 1);
      m_run_thread.Clear();
      m_write_event.Set();
      break;
    }
  }
}

void Wiimote::WriteThreadFunc()
{
  Common::SetCurrentThreadName("Wiimote Write Thread");

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

  std::thread read_thread{&Wiimote::ReadThreadFunc, this};

  // Windows and also the DolphinBar require performing a write
  //  to detect disconnections in a timely manner
  // If we haven't written a report in some time, attempt a rumble-off report.
  // This also has a minor benefit of preventing rumble from being stuck on.
  constexpr auto WRITE_TEST_INTERVAL = std::chrono::milliseconds{1000};

  TimePoint last_write_time = Clock::now();

  while (m_run_thread.IsSet())
  {
    bool write_success = false;

    if (!m_write_reports.Empty())
    {
      // Send a normal report.
      write_success = Write(m_write_reports.Front());
      m_write_reports.Pop();
    }
    else if (Clock::now() - last_write_time >= WRITE_TEST_INTERVAL)
    {
      // We haven't written in a while, test a write so we can check for a disconnect.
      DEBUG_LOG_FMT(WIIMOTE, "Sending periodic write test for Wiimote {}.", m_index + 1);
      const u8 rumble_off[] = {WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::Rumble), 0x00};

      write_success = IOWrite(std::data(rumble_off), std::size(rumble_off)) > 0;
    }
    else
    {
      // Nothing to do. Wait a while for a kick.
      m_write_event.WaitFor(WRITE_TEST_INTERVAL);
      continue;
    }

    last_write_time = Clock::now();

    if (!write_success)
    {
      WARN_LOG_FMT(WIIMOTE, "Wiimote::Write failed on Wiimote {}.", m_index + 1);
      m_run_thread.Clear();
      break;
    }
  }

  IOWakeup();
  read_thread.join();

  NOTICE_LOG_FMT(WIIMOTE, "Disconnecting Wiimote {}.", m_index + 1);
  DisconnectInternal();
}

void Wiimote::RefreshConfig()
{
  m_speaker_enabled_in_dolphin_config = Config::Get(Config::MAIN_WIIMOTE_ENABLE_SPEAKER);
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

bool IsValidDeviceName(std::string_view name)
{
  return IsWiimoteName(name) || IsBalanceBoardName(name);
}

bool IsWiimoteName(std::string_view name)
{
  // Wii software similarly checks just the start of the name.
  return name.starts_with("Nintendo RVL-CNT");
}

bool IsBalanceBoardName(std::string_view name)
{
  // Wii software similarly checks just the start of the name.
  return name.starts_with("Nintendo RVL-WBC");
}

// This is called from the scanner backends (currently on the scanner thread).
bool IsNewWiimote(const std::string& identifier)
{
  std::lock_guard lk(s_known_ids_mutex);
  return !s_known_ids.contains(identifier);
}

bool IsKnownDeviceId(const USBUtils::DeviceInfo& device_info)
{
  return device_info.vid == 0x057e && (device_info.pid == 0x0306 || device_info.pid == 0x0330);
}

void HandleWiimoteSourceChange(unsigned int index)
{
  std::lock_guard wm_lk(g_wiimotes_mutex);

  {
    const Core::CPUThreadGuard guard(Core::System::GetInstance());
    auto removed_wiimote = std::move(g_wiimotes[index]);

    // The Wiimote pool isn't currently designed to handle Balance Boards.
    // Refactoring will be needed to differentiate between remotes/boards.
    const bool is_balance_board = index == WIIMOTE_BALANCE_BOARD;

    if (removed_wiimote != nullptr && !is_balance_board)
      AddWiimoteToPool(std::move(removed_wiimote));
  }
  g_controller_interface.PlatformPopulateDevices([] { ProcessWiimotePool(); });
}

void HandleWiimotesInControllerInterfaceSettingChange()
{
  g_controller_interface.PlatformPopulateDevices([] { ProcessWiimotePool(); });
}

}  // namespace WiimoteReal
