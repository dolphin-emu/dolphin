// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Core.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
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
#include "Common/FPURoundMode.h"
#include "Common/FatFsUtil.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/OneShotEvent.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/TimeUtil.h"
#include "Common/Version.h"

#include "Core/AchievementManager.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/CPUThreadConfigCallback.h"
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

#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FrameDumper.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PerformanceMetrics.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoEvents.h"

namespace Core
{
static bool s_wants_determinism;

// Declarations and definitions
static std::thread s_emu_thread;
static Common::HookableEvent<Core::State> s_state_changed_event;

static bool s_is_throttler_temp_disabled = false;
static bool s_frame_step = false;
static std::atomic<bool> s_stop_frame_step;

// Threads other than the CPU thread must hold this when taking on the role of the CPU thread.
// The CPU thread is not required to hold this when doing normal work, but must hold it if writing
// to s_state.
static std::recursive_mutex s_core_mutex;

// The value Paused is never stored in this variable. The core is considered to be in
// the Paused state if this variable is Running and the CPU reports that it's stepping.
static std::atomic<State> s_state = State::Uninitialized;

#ifdef USE_MEMORYWATCHER
static std::unique_ptr<MemoryWatcher> s_memory_watcher;
#endif

static void Callback_FramePresented(const PresentInfo& present_info);

struct HostJob
{
  std::function<void(Core::System&)> job;
  bool run_after_stop;
};
static std::mutex s_host_jobs_lock;
static std::queue<HostJob> s_host_jobs_queue;

static thread_local bool tls_is_cpu_thread = false;
static thread_local bool tls_is_gpu_thread = false;

static void EmuThread(Core::System& system, std::unique_ptr<BootParameters> boot,
                      WindowSystemInfo wsi);

bool GetIsThrottlerTempDisabled()
{
  return s_is_throttler_temp_disabled;
}

void SetIsThrottlerTempDisabled(bool disable)
{
  s_is_throttler_temp_disabled = disable;
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

bool IsUninitialized(Core::System& system)
{
  return s_state.load() == State::Uninitialized;
}

bool IsCPUThread()
{
  return tls_is_cpu_thread;
}

bool IsGPUThread()
{
  return tls_is_gpu_thread;
}

bool WantsDeterminism()
{
  return s_wants_determinism;
}

// This is called from the GUI thread. See the booting call schedule in
// BootManager.cpp
bool Init(Core::System& system, std::unique_ptr<BootParameters> boot, const WindowSystemInfo& wsi)
{
  std::lock_guard lock(s_core_mutex);

  if (s_emu_thread.joinable())
  {
    if (!IsUninitialized(system))
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
  {
    std::lock_guard lock(s_core_mutex);

    const State state = s_state.load();
    if (state == State::Stopping || state == State::Uninitialized)
      return;

    s_state.store(State::Stopping);
  }

  NotifyStateChanged(State::Stopping);

  AchievementManager::GetInstance().CloseGame();

  // Dump left over jobs
  HostDispatchJobs(system);

  system.GetFifo().EmulatorState(false);

  INFO_LOG_FMT(CONSOLE, "Stop [Main Thread]\t\t---- Shutting down ----");

  // Stop the CPU
  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Stop CPU"));
  system.GetCPU().Stop();
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

// For the CPU Thread only.
static void CPUSetInitialExecutionState(bool force_paused = false)
{
  // The CPU starts in stepping state, and will wait until a new state is set before executing.
  // SetState isn't safe to call from the CPU thread, so we ask the host thread to call it.
  QueueHostJob([force_paused](Core::System& system) {
    bool paused = SConfig::GetInstance().bBootToPause || force_paused;
    SetState(system, paused ? State::Paused : State::Running, true, true);
    Host_UpdateDisasmDialog();
    Host_Message(HostMessageID::WMUserCreate);
  });
}

// Create the CPU thread, which is a CPU + Video thread in Single Core mode.
static void CpuThread(Core::System& system, const std::optional<std::string>& savestate_path,
                      bool delete_savestate)
{
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

  {
    std::unique_lock core_lock(s_core_mutex);

    // If s_state is Starting, change it to Running. But if it's already been set to Stopping
    // because another thread called Stop, don't change it.
    State expected = State::Starting;
    s_state.compare_exchange_strong(expected, State::Running);
  }

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
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Stopping GDB ..."));
    GDBStub::Deinit();
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "GDB stopped."));
    INFO_LOG_FMT(GDB_STUB, "Killed by CPU shutdown");
  }
}

static void FifoPlayerThread(Core::System& system, const std::optional<std::string>& savestate_path,
                             bool delete_savestate)
{
  if (system.IsDualCoreMode())
    Common::SetCurrentThreadName("FIFO player thread");
  else
    Common::SetCurrentThreadName("FIFO-GPU thread");

  // Enter CPU run loop. When we leave it - we are done.
  if (auto cpu_core = system.GetFifoPlayer().GetCPUCore())
  {
    system.GetPowerPC().InjectExternalCPUCore(cpu_core.get());

    {
      std::lock_guard core_lock(s_core_mutex);

      // If s_state is Starting, change it to Running. But if it's already been set to Stopping
      // because another thread called Stop, don't change it.
      State expected = State::Starting;
      s_state.compare_exchange_strong(expected, State::Running);
    }

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

// Returns a RAII object for video backend initialization and deinitialization.
// Returns nullptr on failure.
[[nodiscard]] static auto GetInitializedVideoGuard(Core::System& system,
                                                   const WindowSystemInfo& wsi)
{
  using GuardType = Common::ScopeGuard<Common::MoveOnlyFunction<void()>>;
  using ReturnType = std::unique_ptr<GuardType>;

  const auto init_video = [&] {
    DeclareAsGPUThread();

    AsyncRequests::GetInstance()->SetPassthrough(!system.IsDualCoreMode());

    // Must happen on the proper thread for some video backends, e.g. OpenGL.
    return g_video_backend->Initialize(wsi);
  };

  const auto deinit_video = [] {
    // Clear on screen messages that haven't expired
    OSD::ClearMessages();

    g_video_backend->Shutdown();
  };

  if (system.IsDualCoreMode())
  {
    std::promise<bool> init_from_thread;

    // Spawn the GPU thread.
    std::thread gpu_thread{[&] {
      Common::SetCurrentThreadName("Video thread");

      const bool is_init = init_video();
      init_from_thread.set_value(is_init);

      if (!is_init)
        return;

      system.GetFifo().RunGpuLoop();
      INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "Video Loop Ended"));

      deinit_video();
    }};

    if (init_from_thread.get_future().get())
    {
      // Return a scope guard that signals the GPU thread to stop then joins it.
      return std::make_unique<GuardType>([&, gpu_thread = std::move(gpu_thread)]() mutable {
        INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Wait for Video Loop to exit ..."));
        system.GetFifo().ExitGpuLoop();
        gpu_thread.join();
        INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "GPU thread stopped."));
      });
    }

    gpu_thread.join();
  }
  else  // SingleCore mode
  {
    if (init_video())
      return std::make_unique<GuardType>(deinit_video);
  }

  return ReturnType{};
}

// Initialize and create emulation thread
// Call browser: Init():s_emu_thread().
// See the BootManager.cpp file description for a complete call schedule.
static void EmuThread(Core::System& system, std::unique_ptr<BootParameters> boot,
                      WindowSystemInfo wsi)
{
  NotifyStateChanged(State::Starting);
  Common::ScopeGuard flag_guard{[] {
    {
      std::lock_guard lock(s_core_mutex);
      s_state.store(State::Uninitialized);
    }

    NotifyStateChanged(State::Uninitialized);

    INFO_LOG_FMT(CONSOLE, "Stop\t\t---- Shutdown complete ----");
  }};

  Common::SetCurrentThreadName("Emuthread - Starting");

  // This will become the CPU thread.
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
    sync_sd_folder = Common::SyncSDFolderToSDImage([] { return false; }, Core::WantsDeterminism());
  }

  Common::ScopeGuard sd_folder_sync_guard{[sync_sd_folder] {
    if (sync_sd_folder && Config::Get(Config::MAIN_ALLOW_SD_WRITES))
    {
      const bool sync_ok = Common::SyncSDImageToSDFolder([] { return false; });
      if (!sync_ok)
      {
        PanicAlertFmtT(
            "Failed to sync SD card with folder. All changes made this session will be "
            "discarded on next boot if you do not manually re-issue a resync in Config > "
            "Wii > SD Card Settings > {0}!",
            Common::GetStringT(Common::SD_UNPACK_TEXT));
      }
    }
  }};

  // Wiimote input config is loaded in OnESTitleChanged

  FreeLook::LoadInputConfig();

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

  // In single-core mode: This holds a video backend shutdown function.
  // In dual-core mode: This holds a GPU thread stopping function (which does the backend shutdown).
  const auto video_guard = GetInitializedVideoGuard(system, wsi);
  if (!video_guard)
  {
    PanicAlertFmt("Failed to initialize video backend!");
    return;
  }

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
  const auto cpu_thread_func =
      std::holds_alternative<BootParameters::DFF>(boot->parameters) ? FifoPlayerThread : CpuThread;

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

  const Common::EventHook frame_presented =
      GetVideoEvents().after_present_event.Register(&Core::Callback_FramePresented);

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

  // Become the CPU thread.
  cpu_thread_func(system, savestate_path, delete_savestate);
}

// Set or get the running state

void SetState(Core::System& system, State state, bool report_state_change,
              bool override_achievement_restrictions)
{
  {
    std::lock_guard lock(s_core_mutex);

    // State cannot be controlled until the CPU Thread is operational
    if (s_state.load() != State::Running)
      return;

    switch (state)
    {
    case State::Paused:
#ifdef USE_RETRO_ACHIEVEMENTS
      if (!override_achievement_restrictions && !AchievementManager::GetInstance().CanPause())
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
  }

  // Certain callers only change the state momentarily. Sending a callback for them causes
  // unwanted updates, such as the Pause/Play button flickering between states on frame advance.
  if (report_state_change)
    NotifyStateChanged(GetState(system));
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

static std::optional<std::string> GenerateScreenshotName()
{
  // append gameId, path only contains the folder here.
  const std::string path_prefix =
      GenerateScreenshotFolderPath() + SConfig::GetInstance().GetGameID();

  const std::time_t cur_time = std::time(nullptr);
  const auto local_time = Common::LocalTime(cur_time);
  if (!local_time)
    return std::nullopt;
  const std::string base_name = fmt::format("{}_{:%Y-%m-%d_%H-%M-%S}", path_prefix, *local_time);

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
  std::optional<std::string> name = GenerateScreenshotName();
  if (name)
    g_frame_dumper->SaveScreenshot(*name);
}

void SaveScreenShot(std::string_view name)
{
  const Core::CPUThreadGuard guard(Core::System::GetInstance());
  g_frame_dumper->SaveScreenshot(fmt::format("{}{}.png", GenerateScreenshotFolderPath(), name));
}

static bool PauseAndLock(Core::System& system)
{
  s_core_mutex.lock();

  if (!IsRunning(system))
    return true;

  // First pause the CPU.  This acquires a wrapper mutex and converts the current thread into
  // a temporary replacement CPU Thread.
  const bool was_unpaused = system.GetCPU().PauseAndLock();

  // audio has to come after CPU, because CPU thread can wait for audio thread (m_throttle).
  system.GetDSP().GetDSPEmulator()->PauseAndLock();

  // video has to come after CPU, because CPU thread can wait for video thread
  // (s_efbAccessRequested).
  system.GetFifo().PauseAndLock();

  ResetRumble();

  return was_unpaused;
}

static void RestoreStateAndUnlock(Core::System& system, const bool unpause_on_unlock)
{
  Common::ScopeGuard scope_guard([] { s_core_mutex.unlock(); });

  if (!IsRunning(system))
    return;

  system.GetDSP().GetDSPEmulator()->UnpauseAndUnlock();
  ResetRumble();

  // CPU is unlocked last because CPU::RestoreStateAndUnlock contains the synchronization mechanism
  // that prevents CPU::Break from racing.
  //
  // The CPU is responsible for managing the Audio and FIFO state so we use its mechanism to unpause
  // them. If we unpaused the systems above when releasing the locks then they could call CPU::Break
  // which would require detecting it and re-pausing with CPU::SetStepping.
  system.GetCPU().RestoreStateAndUnlock(unpause_on_unlock);
}

void RunOnCPUThread(Core::System& system, Common::MoveOnlyFunction<void()> function,
                    bool wait_for_completion)
{
  if (IsCPUThread())
  {
    function();
    return;
  }

  Common::OneShotEvent cpu_thread_job_finished;

  // Pause the CPU (set it to stepping mode).
  const bool was_running = PauseAndLock(system);

  if (!IsRunning(system))
  {
    // If the core hasn't been started, there is no active CPU thread we can race against.
    function();
    wait_for_completion = false;
  }
  else if (wait_for_completion)
  {
    // Queue the job function followed by triggering the event.
    system.GetCPU().AddCPUThreadJob([&function, &cpu_thread_job_finished] {
      function();
      cpu_thread_job_finished.Set();
    });
  }
  else
  {
    // Queue the job function.
    system.GetCPU().AddCPUThreadJob(std::move(function));
  }

  // Release the CPU thread, and let it execute the callback.
  RestoreStateAndUnlock(system, was_running);

  // If we're waiting for completion, block until the event fires.
  if (wait_for_completion)
  {
    // Periodically yield to the UI thread, so we don't deadlock.
    while (!cpu_thread_job_finished.WaitFor(std::chrono::milliseconds(10)))
      Host_YieldToUI();
  }
}

// --- Callbacks for backends / engine ---

// Called from Renderer::Swap (GPU thread) when a frame is presented to the host screen.
void Callback_FramePresented(const PresentInfo& present_info)
{
  g_perf_metrics.CountFrame();

  const auto presentation_offset =
      present_info.actual_present_time - present_info.intended_present_time;
  g_perf_metrics.SetLatestFramePresentationOffset(presentation_offset);

  if (present_info.reason == PresentInfo::PresentReason::VideoInterfaceDuplicate)
    return;

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
      NotifyStateChanged(Core::GetState(system));
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

Common::EventHook AddOnStateChangedCallback(StateChangedCallbackFunc callback)
{
  return s_state_changed_event.Register(std::move(callback));
}

void NotifyStateChanged(Core::State state)
{
  s_state_changed_event.Trigger(state);
  g_perf_metrics.OnEmulationStateChanged(state);
}

void UpdateWantDeterminism(Core::System& system, bool initial)
{
  const Core::CPUThreadGuard guard(system);

  // For now, this value is not itself configurable.  Instead, individual
  // settings that depend on it, such as GPU determinism mode. should have
  // override options for testing,
  bool new_want_determinism = system.GetMovie().IsMovieActive() || NetPlay::IsNetPlayRunning();
  if (new_want_determinism != s_wants_determinism || initial)
  {
    NOTICE_LOG_FMT(COMMON, "Want determinism <- {}", new_want_determinism ? "true" : "false");

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

  std::lock_guard lock(s_core_mutex);

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
    m_was_unpaused = PauseAndLock(system);
}

CPUThreadGuard::~CPUThreadGuard()
{
  if (!m_was_cpu_thread)
    RestoreStateAndUnlock(m_system, m_was_unpaused);
}

}  // namespace Core
