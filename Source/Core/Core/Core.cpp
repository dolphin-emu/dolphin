// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Core.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>
#include <queue>
#include <utility>
#include <variant>

#include <fmt/chrono.h>
#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "AudioCommon/AudioCommon.h"

#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FPURoundMode.h"
#include "Common/FatFsUtil.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Common/Version.h"

#include "Core/AchievementManager.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/CPUThreadConfigCallback.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/DSPEmulator.h"
#include "Core/DolphinAnalytics.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FreeLookManager.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/HW.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/IOS/IOS.h"
#include "Core/MemTools.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/GDBStub.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "Core/System.h"
#include "Core/WiiRoot.h"

#ifdef USE_MEMORYWATCHER
#include "Core/MemoryWatcher.h"
#endif

#include "DiscIO/RiivolutionPatcher.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCAdapter.h"

#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FrameDumper.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PerformanceMetrics.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoEvents.h"

namespace Core
{
static bool s_wants_determinism;

// Declarations and definitions
static std::thread s_emu_thread;
static std::vector<StateChangedCallbackFunc> s_on_state_changed_callbacks;

static std::thread s_cpu_thread;
static bool s_is_throttler_temp_disabled = false;
static std::atomic<double> s_last_actual_emulation_speed{1.0};
static bool s_frame_step = false;
static std::atomic<bool> s_stop_frame_step;

// The value Paused is never stored in this variable. The core is considered to be in
// the Paused state if this variable is Running and the CPU reports that it's stepping.
static std::atomic<State> s_state = State::Uninitialized;

#ifdef USE_MEMORYWATCHER
static std::unique_ptr<MemoryWatcher> s_memory_watcher;
#endif

struct HostJob
{
  std::function<void(Core::System&)> job;
  bool run_after_stop;
};
static std::mutex s_host_jobs_lock;
static std::queue<HostJob> s_host_jobs_queue;
static Common::Event s_cpu_thread_job_finished;

static thread_local bool tls_is_cpu_thread = false;
static thread_local bool tls_is_gpu_thread = false;
static thread_local bool tls_is_host_thread = false;

static void EmuThread(Core::System& system, std::unique_ptr<BootParameters> boot,
                      WindowSystemInfo wsi);

static Common::EventHook s_frame_presented = AfterPresentEvent::Register(
    [](auto& present_info) {
      const double last_speed_denominator = g_perf_metrics.GetLastSpeedDenominator();
      // The denominator should always be > 0 but if it's not, just return 1
      const double last_speed = last_speed_denominator > 0.0 ? (1.0 / last_speed_denominator) : 1.0;

      if (present_info.reason != PresentInfo::PresentReason::VideoInterfaceDuplicate)
        Core::Callback_FramePresented(last_speed);
    },
    "Core Frame Presented");

bool GetIsThrottlerTempDisabled()
{
  return s_is_throttler_temp_disabled;
}

void SetIsThrottlerTempDisabled(bool disable)
{
  s_is_throttler_temp_disabled = disable;
}

double GetActualEmulationSpeed()
{
  return s_last_actual_emulation_speed;
}

void FrameUpdateOnCPUThread()
{
  if (NetPlay::IsNetPlayRunning())
    NetPlay::NetPlayClient::SendTimeBase();
}

void OnFrameEnd(Core::System& system)
{
#ifdef USE_MEMORYWATCHER
  if (s_memory_watcher)
  {
    ASSERT(IsCPUThread());
    const CPUThreadGuard guard(system);

    s_memory_watcher->Step(guard);
  }
#endif
}

// Display messages and return values

// Formatted stop message
std::string StopMessage(bool main_thread, std::string_view message)
{
  return fmt::format("Stop [{} {}]\t{}", main_thread ? "Main Thread" : "Video Thread",
                     Common::CurrentThreadId(), message);
}

void DisplayMessage(std::string message, int time_in_ms)
{
  if (!IsRunningOrStarting(Core::System::GetInstance()))
    return;

  // Actually displaying non-ASCII could cause things to go pear-shaped
  if (!std::ranges::all_of(message, Common::IsPrintableCharacter))
    return;

  OSD::AddMessage(std::move(message), time_in_ms);
}

bool IsRunning(Core::System& system)
{
  return s_state.load() == State::Running;
}

bool IsRunningOrStarting(Core::System& system)
{
  const State state = s_state.load();
  return state == State::Running || state == State::Starting;
}

bool IsCPUThread()
{
  return tls_is_cpu_thread;
}

bool IsGPUThread()
{
  return tls_is_gpu_thread;
}

bool IsHostThread()
{
  return tls_is_host_thread;
}

bool WantsDeterminism()
{
  return s_wants_determinism;
}

// This is called from the GUI thread. See the booting call schedule in
// BootManager.cpp
bool Init(Core::System& system, std::unique_ptr<BootParameters> boot, const WindowSystemInfo& wsi)
{
  if (s_emu_thread.joinable())
  {
    if (IsRunning(system))
    {
      PanicAlertFmtT("Emu Thread already running");
      return false;
    }

    // The Emu Thread was stopped, synchronize with it.
    s_emu_thread.join();
  }

  // Drain any left over jobs
  HostDispatchJobs(system);

  INFO_LOG_FMT(BOOT, "Starting core = {} mode", system.IsWii() ? "Wii" : "GameCube");
  INFO_LOG_FMT(BOOT, "CPU Thread separate = {}", system.IsDualCoreMode() ? "Yes" : "No");

  Host_UpdateMainFrame();  // Disable any menus or buttons at boot

  // Manually reactivate the video backend in case a GameINI overrides the video backend setting.
  VideoBackendBase::PopulateBackendInfo(wsi);

  // Issue any API calls which must occur on the main thread for the graphics backend.
  WindowSystemInfo prepared_wsi(wsi);
  g_video_backend->PrepareWindow(prepared_wsi);

  // Start the emu thread
  s_state.store(State::Starting);
  s_emu_thread = std::thread(EmuThread, std::ref(system), std::move(boot), prepared_wsi);
  return true;
}

static void ResetRumble()
{
#if defined(__LIBUSB__)
  GCAdapter::ResetRumble();
#endif
  if (!Pad::IsInitialized())
    return;
  for (int i = 0; i < 4; ++i)
    Pad::ResetRumble(i);
}

// Called from GUI thread
void Stop(Core::System& system)  // - Hammertime!
{
  const State state = s_state.load();
  if (state == State::Stopping || state == State::Uninitialized)
    return;

  AchievementManager::GetInstance().CloseGame();

  s_state.store(State::Stopping);

  CallOnStateChangedCallbacks(State::Stopping);

  // Dump left over jobs
  HostDispatchJobs(system);

  system.GetFifo().EmulatorState(false);

  INFO_LOG_FMT(CONSOLE, "Stop [Main Thread]\t\t---- Shutting down ----");

  // Stop the CPU
  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Stop CPU"));
  system.GetCPU().Stop();

  if (system.IsDualCoreMode())
  {
    // Video_EnterLoop() should now exit so that EmuThread()
    // will continue concurrently with the rest of the commands
    // in this function. We no longer rely on Postmessage.
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Wait for Video Loop to exit ..."));

    g_video_backend->Video_ExitLoop();
  }

  s_last_actual_emulation_speed = 1.0;
}

void DeclareAsCPUThread()
{
  tls_is_cpu_thread = true;
}

void UndeclareAsCPUThread()
{
  tls_is_cpu_thread = false;
}

void DeclareAsGPUThread()
{
  tls_is_gpu_thread = true;
}

void UndeclareAsGPUThread()
{
  tls_is_gpu_thread = false;
}

void DeclareAsHostThread()
{
  tls_is_host_thread = true;
}

void UndeclareAsHostThread()
{
  tls_is_host_thread = false;
}

// For the CPU Thread only.
static void CPUSetInitialExecutionState(bool force_paused = false)
{
  // The CPU starts in stepping state, and will wait until a new state is set before executing.
  // SetState must be called on the host thread, so we defer it for later.
  QueueHostJob([force_paused](Core::System& system) {
    bool paused = SConfig::GetInstance().bBootToPause || force_paused;
    SetState(system, paused ? State::Paused : State::Running, true, true);
    Host_UpdateDisasmDialog();
    Host_UpdateMainFrame();
    Host_Message(HostMessageID::WMUserCreate);
  });
}

// Create the CPU thread, which is a CPU + Video thread in Single Core mode.
static void CpuThread(Core::System& system, const std::optional<std::string>& savestate_path,
                      bool delete_savestate)
{
  DeclareAsCPUThread();

  if (system.IsDualCoreMode())
    Common::SetCurrentThreadName("CPU thread");
  else
    Common::SetCurrentThreadName("CPU-GPU thread");

  // This needs to be delayed until after the video backend is ready.
  DolphinAnalytics::Instance().ReportGameStart();

  // Clear performance data collected from previous threads.
  g_perf_metrics.Reset();

  // The JIT need to be able to intercept faults, both for fastmem and for the BLR optimization.
  const bool exception_handler = EMM::IsExceptionHandlerSupported();
  if (exception_handler)
    EMM::InstallExceptionHandler();

#ifdef USE_MEMORYWATCHER
  s_memory_watcher = std::make_unique<MemoryWatcher>();
#endif

  if (savestate_path)
  {
    ::State::LoadAs(system, *savestate_path);
    if (delete_savestate)
      File::Delete(*savestate_path);
  }

  // If s_state is Starting, change it to Running. But if it's already been set to Stopping
  // by the host thread, don't change it.
  State expected = State::Starting;
  s_state.compare_exchange_strong(expected, State::Running);

  {
#ifndef _WIN32
    std::string gdb_socket = Config::Get(Config::MAIN_GDB_SOCKET);
    if (!gdb_socket.empty() && !AchievementManager::GetInstance().IsHardcoreModeActive())
    {
      GDBStub::InitLocal(gdb_socket.data());
      CPUSetInitialExecutionState(true);
    }
    else
#endif
    {
      int gdb_port = Config::Get(Config::MAIN_GDB_PORT);
      if (gdb_port > 0 && !AchievementManager::GetInstance().IsHardcoreModeActive())
      {
        GDBStub::Init(gdb_port);
        CPUSetInitialExecutionState(true);
      }
      else
      {
        CPUSetInitialExecutionState();
      }
    }
  }

  // Enter CPU run loop. When we leave it - we are done.
  system.GetCPU().Run();

#ifdef USE_MEMORYWATCHER
  s_memory_watcher.reset();
#endif

  if (exception_handler)
    EMM::UninstallExceptionHandler();

  if (GDBStub::IsActive())
  {
    GDBStub::Deinit();
    INFO_LOG_FMT(GDB_STUB, "Killed by CPU shutdown");
    return;
  }
}

static void FifoPlayerThread(Core::System& system, const std::optional<std::string>& savestate_path,
                             bool delete_savestate)
{
  DeclareAsCPUThread();

  if (system.IsDualCoreMode())
    Common::SetCurrentThreadName("FIFO player thread");
  else
    Common::SetCurrentThreadName("FIFO-GPU thread");

  // Enter CPU run loop. When we leave it - we are done.
  if (auto cpu_core = system.GetFifoPlayer().GetCPUCore())
  {
    system.GetPowerPC().InjectExternalCPUCore(cpu_core.get());

    // If s_state is Starting, change it to Running. But if it's already been set to Stopping
    // by the host thread, don't change it.
    State expected = State::Starting;
    s_state.compare_exchange_strong(expected, State::Running);

    CPUSetInitialExecutionState();
    system.GetCPU().Run();

    system.GetPowerPC().InjectExternalCPUCore(nullptr);
    system.GetFifoPlayer().Close();
  }
  else
  {
    // FIFO log does not contain any frames, cannot continue.
    PanicAlertFmt("FIFO file is invalid, cannot playback.");
    system.GetFifoPlayer().Close();
    return;
  }
}

// Initialize and create emulation thread
// Call browser: Init():s_emu_thread().
// See the BootManager.cpp file description for a complete call schedule.
static void EmuThread(Core::System& system, std::unique_ptr<BootParameters> boot,
                      WindowSystemInfo wsi)
{
  CallOnStateChangedCallbacks(State::Starting);
  Common::ScopeGuard flag_guard{[] {
    s_state.store(State::Uninitialized);

    CallOnStateChangedCallbacks(State::Uninitialized);

    INFO_LOG_FMT(CONSOLE, "Stop\t\t---- Shutdown complete ----");
  }};

  Common::SetCurrentThreadName("Emuthread - Starting");

  DeclareAsGPUThread();

  // For a time this acts as the CPU thread...
  DeclareAsCPUThread();
  s_frame_step = false;

  // If settings have changed since the previous run, notify callbacks.
  CPUThreadConfigCallback::CheckForConfigChanges();

  // Switch the window used for inputs to the render window. This way, the cursor position
  // is relative to the render window, instead of the main window.
  ASSERT(g_controller_interface.IsInit());
  g_controller_interface.ChangeWindow(wsi.render_window);

  Pad::LoadConfig();
  Pad::LoadGBAConfig();
  Keyboard::LoadConfig();

  BootSessionData boot_session_data = std::move(boot->boot_session_data);
  const std::optional<std::string>& savestate_path = boot_session_data.GetSavestatePath();
  const bool delete_savestate =
      boot_session_data.GetDeleteSavestate() == DeleteSavestateAfterBoot::Yes;

  bool sync_sd_folder = system.IsWii() && Config::Get(Config::MAIN_WII_SD_CARD) &&
                        Config::Get(Config::MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC);
  if (sync_sd_folder)
  {
    sync_sd_folder =
        Common::SyncSDFolderToSDImage([]() { return false; }, Core::WantsDeterminism());
  }

  Common::ScopeGuard sd_folder_sync_guard{[sync_sd_folder] {
    if (sync_sd_folder && Config::Get(Config::MAIN_ALLOW_SD_WRITES))
    {
      const bool sync_ok = Common::SyncSDImageToSDFolder([]() { return false; });
      if (!sync_ok)
      {
        PanicAlertFmtT(
            "Failed to sync SD card with folder. All changes made this session will be "
            "discarded on next boot if you do not manually re-issue a resync in Config > "
            "Wii > SD Card Settings > Convert File to Folder Now!");
      }
    }
  }};

  // Load Wiimotes - only if we are booting in Wii mode
  if (system.IsWii() && !Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED))
  {
    Wiimote::LoadConfig();
  }

  FreeLook::LoadInputConfig();

  system.GetCustomAssetLoader().Init();
  Common::ScopeGuard asset_loader_guard([&system] { system.GetCustomAssetLoader().Shutdown(); });

  system.GetMovie().Init(*boot);
  Common::ScopeGuard movie_guard([&system] { system.GetMovie().Shutdown(); });

  AudioCommon::InitSoundStream(system);
  Common::ScopeGuard audio_guard([&system] { AudioCommon::ShutdownSoundStream(system); });

  HW::Init(system,
           NetPlay::IsNetPlayRunning() ? &(boot_session_data.GetNetplaySettings()->sram) : nullptr);

  Common::ScopeGuard hw_guard{[&system] {
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "Shutting down HW"));
    HW::Shutdown(system);
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "HW shutdown"));

    // The config must be restored only after the whole HW has shut down,
    // not when it is still running.
    BootManager::RestoreConfig();

    PatchEngine::Shutdown();
    HLE::Clear();

    CPUThreadGuard guard(system);
    system.GetPowerPC().GetDebugInterface().Clear(guard);
  }};

  VideoBackendBase::PopulateBackendInfo(wsi);

  if (!g_video_backend->Initialize(wsi))
  {
    PanicAlertFmt("Failed to initialize video backend!");
    return;
  }
  Common::ScopeGuard video_guard{[] {
    // Clear on screen messages that haven't expired
    OSD::ClearMessages();

    g_video_backend->Shutdown();
  }};

  if (cpu_info.HTT)
    Config::SetBaseOrCurrent(Config::MAIN_DSP_THREAD, cpu_info.num_cores > 4);
  else
    Config::SetBaseOrCurrent(Config::MAIN_DSP_THREAD, cpu_info.num_cores > 2);

  if (!system.GetDSP().GetDSPEmulator()->Initialize(system.IsWii(),
                                                    Config::Get(Config::MAIN_DSP_THREAD)))
  {
    PanicAlertFmt("Failed to initialize DSP emulation!");
    return;
  }

  AudioCommon::PostInitSoundStream(system);

  // Set execution state to known values (CPU/FIFO/Audio Paused)
  system.GetCPU().Break();

  // Load GCM/DOL/ELF whatever ... we boot with the interpreter core
  system.GetPowerPC().SetMode(PowerPC::CoreMode::Interpreter);

  // Determine the CPU thread function
  void (*cpuThreadFunc)(Core::System & system, const std::optional<std::string>& savestate_path,
                        bool delete_savestate);
  if (std::holds_alternative<BootParameters::DFF>(boot->parameters))
    cpuThreadFunc = FifoPlayerThread;
  else
    cpuThreadFunc = CpuThread;

  std::optional<DiscIO::Riivolution::SavegameRedirect> savegame_redirect = std::nullopt;
  if (system.IsWii())
    savegame_redirect = DiscIO::Riivolution::ExtractSavegameRedirect(boot->riivolution_patches);

  {
    ASSERT(IsCPUThread());
    CPUThreadGuard guard(system);
    if (!CBoot::BootUp(system, guard, std::move(boot)))
      return;
  }

  // Initialise Wii filesystem contents.
  // This is done here after Boot and not in BootManager to ensure that we operate
  // with the correct title context since save copying requires title directories to exist.
  Common::ScopeGuard wiifs_guard{[&boot_session_data] {
    Core::CleanUpWiiFileSystemContents(boot_session_data);
    boot_session_data.InvokeWiiSyncCleanup();
  }};
  if (system.IsWii())
    Core::InitializeWiiFileSystemContents(savegame_redirect, boot_session_data);
  else
    wiifs_guard.Dismiss();

  // This adds the SyncGPU handler to CoreTiming, so now CoreTiming::Advance might block.
  system.GetFifo().Prepare();

  // Setup our core
  if (Config::Get(Config::MAIN_CPU_CORE) != PowerPC::CPUCore::Interpreter)
  {
    system.GetPowerPC().SetMode(PowerPC::CoreMode::JIT);
  }
  else
  {
    system.GetPowerPC().SetMode(PowerPC::CoreMode::Interpreter);
  }

  UpdateTitle(system);

  // ENTER THE VIDEO THREAD LOOP
  if (system.IsDualCoreMode())
  {
    // This thread, after creating the EmuWindow, spawns a CPU
    // thread, and then takes over and becomes the video thread
    Common::SetCurrentThreadName("Video thread");
    UndeclareAsCPUThread();
    Common::FPU::LoadDefaultSIMDState();

    // Spawn the CPU thread. The CPU thread will signal the event that boot is complete.
    s_cpu_thread =
        std::thread(cpuThreadFunc, std::ref(system), std::ref(savestate_path), delete_savestate);

    // become the GPU thread
    system.GetFifo().RunGpuLoop();

    // We have now exited the Video Loop
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "Video Loop Ended"));

    // Join with the CPU thread.
    s_cpu_thread.join();
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "CPU thread stopped."));

    // Redeclare this thread as the CPU thread, so that the code running in the scope guards doesn't
    // think we're doing anything unsafe by doing stuff that could race with the CPU thread.
    DeclareAsCPUThread();
  }
  else  // SingleCore mode
  {
    // Become the CPU thread
    cpuThreadFunc(system, savestate_path, delete_savestate);
  }

  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Stopping GDB ..."));
  GDBStub::Deinit();
  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "GDB stopped."));
}

// Set or get the running state

void SetState(Core::System& system, State state, bool report_state_change,
              bool initial_execution_state)
{
  // State cannot be controlled until the CPU Thread is operational
  if (s_state.load() != State::Running)
    return;

  switch (state)
  {
  case State::Paused:
#ifdef USE_RETRO_ACHIEVEMENTS
    if (!initial_execution_state && !AchievementManager::GetInstance().CanPause())
      return;
#endif  // USE_RETRO_ACHIEVEMENTS
    // NOTE: GetState() will return State::Paused immediately, even before anything has
    //   stopped (including the CPU).
    system.GetCPU().SetStepping(true);  // Break
    Wiimote::Pause();
    ResetRumble();
#ifdef USE_RETRO_ACHIEVEMENTS
    AchievementManager::GetInstance().DoIdle();
#endif  // USE_RETRO_ACHIEVEMENTS
    break;
  case State::Running:
  {
    system.GetCPU().SetStepping(false);
    Wiimote::Resume();
    break;
  }
  default:
    PanicAlertFmt("Invalid state");
    break;
  }

  // Certain callers only change the state momentarily. Sending a callback for them causes
  // unwanted updates, such as the Pause/Play button flickering between states on frame advance.
  if (report_state_change)
    CallOnStateChangedCallbacks(GetState(system));
}

State GetState(Core::System& system)
{
  const State state = s_state.load();
  if (state == State::Running && system.GetCPU().IsStepping())
    return State::Paused;
  else
    return state;
}

static std::string GenerateScreenshotFolderPath()
{
  const std::string& gameId = SConfig::GetInstance().GetGameID();
  std::string path = File::GetUserPath(D_SCREENSHOTS_IDX) + gameId + DIR_SEP_CHR;

  if (!File::CreateFullPath(path))
  {
    // fallback to old-style screenshots, without folder.
    path = File::GetUserPath(D_SCREENSHOTS_IDX);
  }

  return path;
}

static std::string GenerateScreenshotName()
{
  // append gameId, path only contains the folder here.
  const std::string path_prefix =
      GenerateScreenshotFolderPath() + SConfig::GetInstance().GetGameID();

  const std::time_t cur_time = std::time(nullptr);
  const std::string base_name =
      fmt::format("{}_{:%Y-%m-%d_%H-%M-%S}", path_prefix, fmt::localtime(cur_time));

  // First try a filename without any suffixes, if already exists then append increasing numbers
  std::string name = fmt::format("{}.png", base_name);
  if (File::Exists(name))
  {
    for (u32 i = 1; File::Exists(name = fmt::format("{}_{}.png", base_name, i)); ++i)
      ;
  }

  return name;
}

void SaveScreenShot()
{
  const Core::CPUThreadGuard guard(Core::System::GetInstance());
  g_frame_dumper->SaveScreenshot(GenerateScreenshotName());
}

void SaveScreenShot(std::string_view name)
{
  const Core::CPUThreadGuard guard(Core::System::GetInstance());
  g_frame_dumper->SaveScreenshot(fmt::format("{}{}.png", GenerateScreenshotFolderPath(), name));
}

static bool PauseAndLock(Core::System& system, bool do_lock, bool unpause_on_unlock)
{
  // WARNING: PauseAndLock is not fully threadsafe so is only valid on the Host Thread

  if (!IsRunning(system))
    return true;

  bool was_unpaused = true;
  if (do_lock)
  {
    // first pause the CPU
    // This acquires a wrapper mutex and converts the current thread into
    // a temporary replacement CPU Thread.
    was_unpaused = system.GetCPU().PauseAndLock(true);
  }

  // audio has to come after CPU, because CPU thread can wait for audio thread (m_throttle).
  system.GetDSP().GetDSPEmulator()->PauseAndLock(do_lock);

  // video has to come after CPU, because CPU thread can wait for video thread
  // (s_efbAccessRequested).
  system.GetFifo().PauseAndLock(do_lock, false);

  ResetRumble();

  // CPU is unlocked last because CPU::PauseAndLock contains the synchronization
  // mechanism that prevents CPU::Break from racing.
  if (!do_lock)
  {
    // The CPU is responsible for managing the Audio and FIFO state so we use its
    // mechanism to unpause them. If we unpaused the systems above when releasing
    // the locks then they could call CPU::Break which would require detecting it
    // and re-pausing with CPU::SetStepping.
    was_unpaused = system.GetCPU().PauseAndLock(false, unpause_on_unlock, true);
  }

  return was_unpaused;
}

void RunOnCPUThread(Core::System& system, std::function<void()> function, bool wait_for_completion)
{
  // If the CPU thread is not running, assume there is no active CPU thread we can race against.
  if (!IsRunning(system) || IsCPUThread())
  {
    function();
    return;
  }

  // Pause the CPU (set it to stepping mode).
  const bool was_running = PauseAndLock(system, true, true);

  // Queue the job function.
  if (wait_for_completion)
  {
    // Trigger the event after executing the function.
    s_cpu_thread_job_finished.Reset();
    system.GetCPU().AddCPUThreadJob([&function]() {
      function();
      s_cpu_thread_job_finished.Set();
    });
  }
  else
  {
    system.GetCPU().AddCPUThreadJob(std::move(function));
  }

  // Release the CPU thread, and let it execute the callback.
  PauseAndLock(system, false, was_running);

  // If we're waiting for completion, block until the event fires.
  if (wait_for_completion)
  {
    // Periodically yield to the UI thread, so we don't deadlock.
    while (!s_cpu_thread_job_finished.WaitFor(std::chrono::milliseconds(10)))
      Host_YieldToUI();
  }
}

// --- Callbacks for backends / engine ---

// Called from Renderer::Swap (GPU thread) when a new (non-duplicate)
// frame is presented to the host screen
void Callback_FramePresented(double actual_emulation_speed)
{
  g_perf_metrics.CountFrame();

  s_last_actual_emulation_speed = actual_emulation_speed;
  s_stop_frame_step.store(true);
}

// Called from VideoInterface::Update (CPU thread) at emulated field boundaries
void Callback_NewField(Core::System& system)
{
  if (s_frame_step)
  {
    // To ensure that s_stop_frame_step is up to date, wait for the GPU thread queue to empty,
    // since it is may contain a swap event (which will call Callback_FramePresented). This hurts
    // the performance a little, but luckily, performance matters less when using frame stepping.
    AsyncRequests::GetInstance()->WaitForEmptyQueue();

    // Only stop the frame stepping if a new frame was displayed
    // (as opposed to the previous frame being displayed for another frame).
    if (s_stop_frame_step.load())
    {
      s_frame_step = false;
      system.GetCPU().Break();
      CallOnStateChangedCallbacks(Core::GetState(system));
    }
  }

  AchievementManager::GetInstance().DoFrame();
}

void UpdateTitle(Core::System& system)
{
  // Settings are shown the same for both extended and summary info
  const std::string SSettings = fmt::format(
      "{} {} | {} | {}", system.GetPowerPC().GetCPUName(), system.IsDualCoreMode() ? "DC" : "SC",
      g_video_backend->GetDisplayName(), Config::Get(Config::MAIN_DSP_HLE) ? "HLE" : "LLE");

  std::string message = fmt::format("{} | {}", Common::GetScmRevStr(), SSettings);
  if (Config::Get(Config::MAIN_SHOW_ACTIVE_TITLE))
  {
    const std::string& title = SConfig::GetInstance().GetTitleDescription();
    if (!title.empty())
      message += " | " + title;
  }

  Host_UpdateTitle(message);
}

void Shutdown(Core::System& system)
{
  // During shutdown DXGI expects us to handle some messages on the UI thread.
  // Therefore we can't immediately block and wait for the emu thread to shut
  // down, so we join the emu thread as late as possible when the UI has already
  // shut down.
  // For more info read "DirectX Graphics Infrastructure (DXGI): Best Practices"
  // on MSDN.
  if (s_emu_thread.joinable())
    s_emu_thread.join();

  // Make sure there's nothing left over in case we're about to exit.
  HostDispatchJobs(system);
}

int AddOnStateChangedCallback(StateChangedCallbackFunc callback)
{
  for (size_t i = 0; i < s_on_state_changed_callbacks.size(); ++i)
  {
    if (!s_on_state_changed_callbacks[i])
    {
      s_on_state_changed_callbacks[i] = std::move(callback);
      return int(i);
    }
  }
  s_on_state_changed_callbacks.emplace_back(std::move(callback));
  return int(s_on_state_changed_callbacks.size()) - 1;
}

bool RemoveOnStateChangedCallback(int* handle)
{
  if (handle && *handle >= 0 && s_on_state_changed_callbacks.size() > static_cast<size_t>(*handle))
  {
    s_on_state_changed_callbacks[*handle] = StateChangedCallbackFunc();
    *handle = -1;
    return true;
  }
  return false;
}

void CallOnStateChangedCallbacks(Core::State state)
{
  for (const StateChangedCallbackFunc& on_state_changed_callback : s_on_state_changed_callbacks)
  {
    if (on_state_changed_callback)
      on_state_changed_callback(state);
  }
}

void UpdateWantDeterminism(Core::System& system, bool initial)
{
  // For now, this value is not itself configurable.  Instead, individual
  // settings that depend on it, such as GPU determinism mode. should have
  // override options for testing,
  bool new_want_determinism = system.GetMovie().IsMovieActive() || NetPlay::IsNetPlayRunning();
  if (new_want_determinism != s_wants_determinism || initial)
  {
    NOTICE_LOG_FMT(COMMON, "Want determinism <- {}", new_want_determinism ? "true" : "false");

    const Core::CPUThreadGuard guard(system);
    s_wants_determinism = new_want_determinism;
    const auto ios = system.GetIOS();
    if (ios)
      ios->UpdateWantDeterminism(new_want_determinism);

    system.GetFifo().UpdateWantDeterminism(new_want_determinism);

    // We need to clear the cache because some parts of the JIT depend on want_determinism,
    // e.g. use of FMA.
    system.GetJitInterface().ClearCache(guard);
  }
}

void QueueHostJob(std::function<void(Core::System&)> job, bool run_during_stop)
{
  if (!job)
    return;

  bool send_message = false;
  {
    std::lock_guard guard(s_host_jobs_lock);
    send_message = s_host_jobs_queue.empty();
    s_host_jobs_queue.emplace(HostJob{std::move(job), run_during_stop});
  }
  // If the the queue was empty then kick the Host to come and get this job.
  if (send_message)
    Host_Message(HostMessageID::WMUserJobDispatch);
}

void HostDispatchJobs(Core::System& system)
{
  // WARNING: This should only run on the Host Thread.
  // NOTE: This function is potentially re-entrant. If a job calls
  //   Core::Stop for instance then we'll enter this a second time.
  std::unique_lock guard(s_host_jobs_lock);
  while (!s_host_jobs_queue.empty())
  {
    HostJob job = std::move(s_host_jobs_queue.front());
    s_host_jobs_queue.pop();

    if (!job.run_after_stop)
    {
      const State state = s_state.load();
      if (state == State::Stopping || state == State::Uninitialized)
        continue;
    }

    guard.unlock();
    job.job(system);
    guard.lock();
  }
}

// NOTE: Host Thread
void DoFrameStep(Core::System& system)
{
  if (AchievementManager::GetInstance().IsHardcoreModeActive())
  {
    OSD::AddMessage("Frame stepping is disabled in RetroAchievements hardcore mode");
    return;
  }
  if (GetState(system) == State::Paused)
  {
    // if already paused, frame advance for 1 frame
    s_stop_frame_step = false;
    s_frame_step = true;
    SetState(system, State::Running, false);
  }
  else if (!s_frame_step)
  {
    // if not paused yet, pause immediately instead
    SetState(system, State::Paused);
  }
}

void UpdateInputGate(bool require_focus, bool require_full_focus)
{
  // If the user accepts background input, controls should pass even if an on screen interface is on
  const bool focus_passes =
      !require_focus ||
      ((Host_RendererHasFocus() || Host_TASInputHasFocus()) && !Host_UIBlocksControllerState());
  // Ignore full focus if we don't require basic focus
  const bool full_focus_passes =
      !require_focus || !require_full_focus || (focus_passes && Host_RendererHasFullFocus());
  ControlReference::SetInputGate(focus_passes && full_focus_passes);
}

CPUThreadGuard::CPUThreadGuard(Core::System& system)
    : m_system(system), m_was_cpu_thread(IsCPUThread())
{
  if (!m_was_cpu_thread)
    m_was_unpaused = PauseAndLock(system, true, true);
}

CPUThreadGuard::~CPUThreadGuard()
{
  if (!m_was_cpu_thread)
    PauseAndLock(m_system, false, m_was_unpaused);
}

}  // namespace Core
