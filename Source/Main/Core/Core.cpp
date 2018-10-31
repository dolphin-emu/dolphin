// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Core.h"

#include <atomic>
#include <cstring>
#include <locale>
#include <mutex>
#include <queue>
#include <utility>
#include <variant>

#ifdef _WIN32
#include <windows.h>
#endif

#include "AudioCommon/AudioCommon.h"

#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Logging/LogManager.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/Analytics.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/DSPEmulator.h"
#include "Core/Host.h"
#include "Core/MemTools.h"
#ifdef USE_MEMORYWATCHER
#include "Core/MemoryWatcher.h"
#endif
#include "Core/Boot/Boot.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/HW.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/IOS/IOS.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "Core/WiiRoot.h"

#ifdef USE_GDBSTUB
#include "Core/PowerPC/GDBStub.h"
#endif

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCAdapter.h"

#include "VideoCommon/Fifo.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace Core
{
static bool s_wants_determinism;

// Declarations and definitions
static Common::Timer s_timer;
static std::atomic<u32> s_drawn_frame;
static std::atomic<u32> s_drawn_video;

static bool s_is_stopping = false;
static bool s_hardware_initialized = false;
static bool s_is_started = false;
static Common::Flag s_is_booting;
static std::thread s_emu_thread;
static StateChangedCallbackFunc s_on_state_changed_callback;

static std::thread s_cpu_thread;
static bool s_request_refresh_info = false;
static bool s_is_throttler_temp_disabled = false;
static bool s_frame_step = false;

struct HostJob
{
  std::function<void()> job;
  bool run_after_stop;
};
static std::mutex s_host_jobs_lock;
static std::queue<HostJob> s_host_jobs_queue;

static thread_local bool tls_is_cpu_thread = false;

static void EmuThread(std::unique_ptr<BootParameters> boot, WindowSystemInfo wsi);

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

// Display messages and return values

// Formatted stop message
std::string StopMessage(bool main_thread, const std::string& message)
{
  return StringFromFormat("Stop [%s %i]\t%s", main_thread ? "Main Thread" : "Video Thread",
                          Common::CurrentThreadId(), message.c_str());
}

void DisplayMessage(const std::string& message, int time_in_ms)
{
  if (!IsRunning())
    return;

  // Actually displaying non-ASCII could cause things to go pear-shaped
  for (const char& c : message)
  {
    if (!std::isprint(c, std::locale::classic()))
      return;
  }

  OSD::AddMessage(message, time_in_ms);
  Host_UpdateTitle(message);
}

bool IsRunning()
{
  return (GetState() != State::Uninitialized || s_hardware_initialized) && !s_is_stopping;
}

bool IsRunningAndStarted()
{
  return s_is_started && !s_is_stopping;
}

bool IsRunningInCurrentThread()
{
  return IsRunning() && IsCPUThread();
}

bool IsCPUThread()
{
  return tls_is_cpu_thread;
}

bool IsGPUThread()
{
  const SConfig& _CoreParameter = SConfig::GetInstance();
  if (_CoreParameter.bCPUThread)
  {
    return (s_emu_thread.joinable() && (s_emu_thread.get_id() == std::this_thread::get_id()));
  }
  else
  {
    return IsCPUThread();
  }
}

bool WantsDeterminism()
{
  return s_wants_determinism;
}

// This is called from the GUI thread. See the booting call schedule in
// BootManager.cpp
bool Init(std::unique_ptr<BootParameters> boot, const WindowSystemInfo& wsi)
{
  if (s_emu_thread.joinable())
  {
    if (IsRunning())
    {
      PanicAlertT("Emu Thread already running");
      return false;
    }

    // The Emu Thread was stopped, synchronize with it.
    s_emu_thread.join();
  }

  // Drain any left over jobs
  HostDispatchJobs();

  Core::UpdateWantDeterminism(/*initial*/ true);

  INFO_LOG(BOOT, "Starting core = %s mode", SConfig::GetInstance().bWii ? "Wii" : "GameCube");
  INFO_LOG(BOOT, "CPU Thread separate = %s", SConfig::GetInstance().bCPUThread ? "Yes" : "No");

  Host_UpdateMainFrame();  // Disable any menus or buttons at boot

  // Start the emu thread
  s_emu_thread = std::thread(EmuThread, std::move(boot), wsi);
  return true;
}

static void ResetRumble()
{
#if defined(__LIBUSB__)
  GCAdapter::ResetRumble();
#endif
#if defined(CIFACE_USE_XINPUT) || defined(CIFACE_USE_DINPUT)
  if (!Pad::IsInitialized())
    return;
  for (int i = 0; i < 4; ++i)
    Pad::ResetRumble(i);
#endif
}

// Called from GUI thread
void Stop()  // - Hammertime!
{
  if (GetState() == State::Stopping || GetState() == State::Uninitialized)
    return;

  const SConfig& _CoreParameter = SConfig::GetInstance();

  s_is_stopping = true;

  // Dump left over jobs
  HostDispatchJobs();

  Fifo::EmulatorState(false);

  INFO_LOG(CONSOLE, "Stop [Main Thread]\t\t---- Shutting down ----");

  // Stop the CPU
  INFO_LOG(CONSOLE, "%s", StopMessage(true, "Stop CPU").c_str());
  CPU::Stop();

  if (_CoreParameter.bCPUThread)
  {
    // Video_EnterLoop() should now exit so that EmuThread()
    // will continue concurrently with the rest of the commands
    // in this function. We no longer rely on Postmessage.
    INFO_LOG(CONSOLE, "%s", StopMessage(true, "Wait for Video Loop to exit ...").c_str());

    g_video_backend->Video_ExitLoop();
  }

  ResetRumble();

#ifdef USE_MEMORYWATCHER
  MemoryWatcher::Shutdown();
#endif
}

void DeclareAsCPUThread()
{
  tls_is_cpu_thread = true;
}

void UndeclareAsCPUThread()
{
  tls_is_cpu_thread = false;
}

// For the CPU Thread only.
static void CPUSetInitialExecutionState()
{
  // The CPU starts in stepping state, and will wait until a new state is set before executing.
  // SetState must be called on the host thread, so we defer it for later.
  QueueHostJob([]() {
    SetState(SConfig::GetInstance().bBootToPause ? State::Paused : State::Running);
    Host_UpdateDisasmDialog();
    Host_UpdateMainFrame();
    Host_Message(HostMessageID::WMUserCreate);
  });
}

// Create the CPU thread, which is a CPU + Video thread in Single Core mode.
static void CpuThread(const std::optional<std::string>& savestate_path, bool delete_savestate)
{
  DeclareAsCPUThread();

  const SConfig& _CoreParameter = SConfig::GetInstance();
  if (_CoreParameter.bCPUThread)
    Common::SetCurrentThreadName("CPU thread");
  else
    Common::SetCurrentThreadName("CPU-GPU thread");

  // This needs to be delayed until after the video backend is ready.
  DolphinAnalytics::Instance()->ReportGameStart();

  if (_CoreParameter.bFastmem)
    EMM::InstallExceptionHandler();  // Let's run under memory watch

#ifdef USE_MEMORYWATCHER
  MemoryWatcher::Init();
#endif

  if (savestate_path)
  {
    ::State::LoadAs(*savestate_path);
    if (delete_savestate)
      File::Delete(*savestate_path);
  }

  s_is_started = true;
  CPUSetInitialExecutionState();

#ifdef USE_GDBSTUB
#ifndef _WIN32
  if (!_CoreParameter.gdb_socket.empty())
  {
    gdb_init_local(_CoreParameter.gdb_socket.data());
    gdb_break();
  }
  else
#endif
      if (_CoreParameter.iGDBPort > 0)
  {
    gdb_init(_CoreParameter.iGDBPort);
    // break at next instruction (the first instruction)
    gdb_break();
  }
#endif

  // Enter CPU run loop. When we leave it - we are done.
  CPU::Run();

  s_is_started = false;

  if (_CoreParameter.bFastmem)
    EMM::UninstallExceptionHandler();
}

static void FifoPlayerThread(const std::optional<std::string>& savestate_path,
                             bool delete_savestate)
{
  DeclareAsCPUThread();

  const SConfig& _CoreParameter = SConfig::GetInstance();
  if (_CoreParameter.bCPUThread)
    Common::SetCurrentThreadName("FIFO player thread");
  else
    Common::SetCurrentThreadName("FIFO-GPU thread");

  // Enter CPU run loop. When we leave it - we are done.
  if (auto cpu_core = FifoPlayer::GetInstance().GetCPUCore())
  {
    PowerPC::InjectExternalCPUCore(cpu_core.get());
    s_is_started = true;

    CPUSetInitialExecutionState();
    CPU::Run();

    s_is_started = false;
    PowerPC::InjectExternalCPUCore(nullptr);
    FifoPlayer::GetInstance().Close();
  }
  else
  {
    // FIFO log does not contain any frames, cannot continue.
    PanicAlert("FIFO file is invalid, cannot playback.");
    FifoPlayer::GetInstance().Close();
    return;
  }
}

// Initialize and create emulation thread
// Call browser: Init():s_emu_thread().
// See the BootManager.cpp file description for a complete call schedule.
static void EmuThread(std::unique_ptr<BootParameters> boot, WindowSystemInfo wsi)
{
  const SConfig& core_parameter = SConfig::GetInstance();
  s_is_booting.Set();
  if (s_on_state_changed_callback)
    s_on_state_changed_callback(State::Starting);
  Common::ScopeGuard flag_guard{[] {
    s_is_booting.Clear();
    s_is_started = false;
    s_is_stopping = false;
    s_wants_determinism = false;

    if (s_on_state_changed_callback)
      s_on_state_changed_callback(State::Uninitialized);

    INFO_LOG(CONSOLE, "Stop\t\t---- Shutdown complete ----");
  }};

  Common::SetCurrentThreadName("Emuthread - Starting");

  // For a time this acts as the CPU thread...
  DeclareAsCPUThread();
  s_frame_step = false;

  Movie::Init(*boot);
  Common::ScopeGuard movie_guard{Movie::Shutdown};

  HW::Init();
  Common::ScopeGuard hw_guard{[] {
    // We must set up this flag before executing HW::Shutdown()
    s_hardware_initialized = false;
    INFO_LOG(CONSOLE, "%s", StopMessage(false, "Shutting down HW").c_str());
    HW::Shutdown();
    INFO_LOG(CONSOLE, "%s", StopMessage(false, "HW shutdown").c_str());

    // Clear on screen messages that haven't expired
    OSD::ClearMessages();

    // The config must be restored only after the whole HW has shut down,
    // not when it is still running.
    BootManager::RestoreConfig();

    PatchEngine::Shutdown();
    HLE::Clear();
  }};

  // Backend info has to be initialized before we can initialize the backend.
  // This is because when we load the config, we validate it against the current backend info.
  // We also should have the correct adapter selected for creating the device in Initialize().
  g_video_backend->InitBackendInfo();
  g_Config.Refresh();

  if (!g_video_backend->Initialize(wsi))
  {
    PanicAlert("Failed to initialize video backend!");
    return;
  }
  Common::ScopeGuard video_guard{[] { g_video_backend->Shutdown(); }};

  if (cpu_info.HTT)
    SConfig::GetInstance().bDSPThread = cpu_info.num_cores > 4;
  else
    SConfig::GetInstance().bDSPThread = cpu_info.num_cores > 2;

  if (!DSP::GetDSPEmulator()->Initialize(core_parameter.bWii, core_parameter.bDSPThread))
  {
    PanicAlert("Failed to initialize DSP emulation!");
    return;
  }

  bool init_controllers = false;
  if (!g_controller_interface.IsInit())
  {
    g_controller_interface.Initialize(wsi.render_surface);
    Pad::Initialize();
    Keyboard::Initialize();
    init_controllers = true;
  }
  else
  {
    // Update references in case controllers were refreshed
    Pad::LoadConfig();
    Keyboard::LoadConfig();
  }

  const std::optional<std::string> savestate_path = boot->savestate_path;
  const bool delete_savestate = boot->delete_savestate;

  // Load and Init Wiimotes - only if we are booting in Wii mode
  if (core_parameter.bWii && !SConfig::GetInstance().m_bt_passthrough_enabled)
  {
    if (init_controllers)
    {
      Wiimote::Initialize(savestate_path ? Wiimote::InitializeMode::DO_WAIT_FOR_WIIMOTES :
                                           Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
    }
    else
    {
      Wiimote::LoadConfig();
    }
  }

  Common::ScopeGuard controller_guard{[init_controllers] {
    if (!init_controllers)
      return;

    Wiimote::Shutdown();
    Keyboard::Shutdown();
    Pad::Shutdown();
    g_controller_interface.Shutdown();
  }};

  AudioCommon::InitSoundStream();
  Common::ScopeGuard audio_guard{AudioCommon::ShutdownSoundStream};

  // The hardware is initialized.
  s_hardware_initialized = true;
  s_is_booting.Clear();

  // Set execution state to known values (CPU/FIFO/Audio Paused)
  CPU::Break();

  // Load GCM/DOL/ELF whatever ... we boot with the interpreter core
  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);

  // Determine the CPU thread function
  void (*cpuThreadFunc)(const std::optional<std::string>& savestate_path, bool delete_savestate);
  if (std::holds_alternative<BootParameters::DFF>(boot->parameters))
    cpuThreadFunc = FifoPlayerThread;
  else
    cpuThreadFunc = CpuThread;

  if (!CBoot::BootUp(std::move(boot)))
    return;

  // Initialise Wii filesystem contents.
  // This is done here after Boot and not in HW to ensure that we operate
  // with the correct title context since save copying requires title directories to exist.
  Common::ScopeGuard wiifs_guard{Core::CleanUpWiiFileSystemContents};
  if (SConfig::GetInstance().bWii)
    Core::InitializeWiiFileSystemContents();
  else
    wiifs_guard.Dismiss();

  // This adds the SyncGPU handler to CoreTiming, so now CoreTiming::Advance might block.
  Fifo::Prepare();

  // Setup our core, but can't use dynarec if we are compare server
  if (core_parameter.cpu_core != PowerPC::CPUCore::Interpreter &&
      (!core_parameter.bRunCompareServer || core_parameter.bRunCompareClient))
  {
    PowerPC::SetMode(PowerPC::CoreMode::JIT);
  }
  else
  {
    PowerPC::SetMode(PowerPC::CoreMode::Interpreter);
  }

  // ENTER THE VIDEO THREAD LOOP
  if (core_parameter.bCPUThread)
  {
    // This thread, after creating the EmuWindow, spawns a CPU
    // thread, and then takes over and becomes the video thread
    Common::SetCurrentThreadName("Video thread");
    UndeclareAsCPUThread();

    // Spawn the CPU thread. The CPU thread will signal the event that boot is complete.
    s_cpu_thread = std::thread(cpuThreadFunc, savestate_path, delete_savestate);

    // become the GPU thread
    Fifo::RunGpuLoop();

    // We have now exited the Video Loop
    INFO_LOG(CONSOLE, "%s", StopMessage(false, "Video Loop Ended").c_str());

    // Join with the CPU thread.
    s_cpu_thread.join();
    INFO_LOG(CONSOLE, "%s", StopMessage(true, "CPU thread stopped.").c_str());
  }
  else  // SingleCore mode
  {
    // Become the CPU thread
    cpuThreadFunc(savestate_path, delete_savestate);
  }

#ifdef USE_GDBSTUB
  INFO_LOG(CONSOLE, "%s", StopMessage(true, "Stopping GDB ...").c_str());
  gdb_deinit();
  INFO_LOG(CONSOLE, "%s", StopMessage(true, "GDB stopped.").c_str());
#endif
}

// Set or get the running state

void SetState(State state)
{
  // State cannot be controlled until the CPU Thread is operational
  if (!IsRunningAndStarted())
    return;

  switch (state)
  {
  case State::Paused:
    // NOTE: GetState() will return State::Paused immediately, even before anything has
    //   stopped (including the CPU).
    CPU::EnableStepping(true);  // Break
    Wiimote::Pause();
    ResetRumble();
    break;
  case State::Running:
    CPU::EnableStepping(false);
    Wiimote::Resume();
    break;
  default:
    PanicAlert("Invalid state");
    break;
  }

  if (s_on_state_changed_callback)
    s_on_state_changed_callback(GetState());
}

State GetState()
{
  if (s_is_stopping)
    return State::Stopping;

  if (s_hardware_initialized)
  {
    if (CPU::IsStepping() || s_frame_step)
      return State::Paused;

    return State::Running;
  }

  if (s_is_booting.IsSet())
    return State::Starting;

  return State::Uninitialized;
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
  std::string path = GenerateScreenshotFolderPath();

  // append gameId, path only contains the folder here.
  path += SConfig::GetInstance().GetGameID();

  std::string name;
  for (int i = 1; File::Exists(name = StringFromFormat("%s-%d.png", path.c_str(), i)); ++i)
  {
    // TODO?
  }

  return name;
}

void SaveScreenShot(bool wait_for_completion)
{
  const bool bPaused = GetState() == State::Paused;

  SetState(State::Paused);

  g_renderer->SaveScreenshot(GenerateScreenshotName(), wait_for_completion);

  if (!bPaused)
    SetState(State::Running);
}

void SaveScreenShot(const std::string& name, bool wait_for_completion)
{
  const bool bPaused = GetState() == State::Paused;

  SetState(State::Paused);

  std::string filePath = GenerateScreenshotFolderPath() + name + ".png";

  g_renderer->SaveScreenshot(filePath, wait_for_completion);

  if (!bPaused)
    SetState(State::Running);
}

void RequestRefreshInfo()
{
  s_request_refresh_info = true;
}

static bool PauseAndLock(bool do_lock, bool unpause_on_unlock)
{
  // WARNING: PauseAndLock is not fully threadsafe so is only valid on the Host Thread
  if (!IsRunning())
    return true;

  bool was_unpaused = true;
  if (do_lock)
  {
    // first pause the CPU
    // This acquires a wrapper mutex and converts the current thread into
    // a temporary replacement CPU Thread.
    was_unpaused = CPU::PauseAndLock(true);
  }

  ExpansionInterface::PauseAndLock(do_lock, false);

  // audio has to come after CPU, because CPU thread can wait for audio thread (m_throttle).
  DSP::GetDSPEmulator()->PauseAndLock(do_lock, false);

  // video has to come after CPU, because CPU thread can wait for video thread
  // (s_efbAccessRequested).
  Fifo::PauseAndLock(do_lock, false);

  ResetRumble();

  // CPU is unlocked last because CPU::PauseAndLock contains the synchronization
  // mechanism that prevents CPU::Break from racing.
  if (!do_lock)
  {
    // The CPU is responsible for managing the Audio and FIFO state so we use its
    // mechanism to unpause them. If we unpaused the systems above when releasing
    // the locks then they could call CPU::Break which would require detecting it
    // and re-pausing with CPU::EnableStepping.
    was_unpaused = CPU::PauseAndLock(false, unpause_on_unlock, true);
  }

  return was_unpaused;
}

void RunAsCPUThread(std::function<void()> function)
{
  const bool is_cpu_thread = IsCPUThread();
  bool was_unpaused = false;
  if (!is_cpu_thread)
    was_unpaused = PauseAndLock(true, true);

  function();

  if (!is_cpu_thread)
    PauseAndLock(false, was_unpaused);
}

// Display FPS info
// This should only be called from VI
void VideoThrottle()
{
  // Update info per second
  u32 ElapseTime = (u32)s_timer.GetTimeDifference();
  if ((ElapseTime >= 1000 && s_drawn_video.load() > 0) || s_request_refresh_info)
  {
    UpdateTitle();

    // Reset counter
    s_timer.Update();
    s_drawn_frame.store(0);
    s_drawn_video.store(0);
  }

  s_drawn_video++;
}

// --- Callbacks for backends / engine ---

// Should be called from GPU thread when a frame is drawn
void Callback_VideoCopiedToXFB(bool video_update)
{
  if (video_update)
    s_drawn_frame++;

  Movie::FrameUpdate();

  if (s_frame_step)
  {
    s_frame_step = false;
    CPU::Break();
    if (s_on_state_changed_callback)
      s_on_state_changed_callback(Core::GetState());
  }
}

void UpdateTitle()
{
  u32 ElapseTime = (u32)s_timer.GetTimeDifference();
  s_request_refresh_info = false;
  SConfig& _CoreParameter = SConfig::GetInstance();

  if (ElapseTime == 0)
    ElapseTime = 1;

  float FPS = (float)(s_drawn_frame.load() * 1000.0 / ElapseTime);
  float VPS = (float)(s_drawn_video.load() * 1000.0 / ElapseTime);
  float Speed = (float)(s_drawn_video.load() * (100 * 1000.0) /
                        (VideoInterface::GetTargetRefreshRate() * ElapseTime));

  // Settings are shown the same for both extended and summary info
  std::string SSettings = StringFromFormat(
      "%s %s | %s | %s", PowerPC::GetCPUName(), _CoreParameter.bCPUThread ? "DC" : "SC",
      g_video_backend->GetDisplayName().c_str(), _CoreParameter.bDSPHLE ? "HLE" : "LLE");

  std::string SFPS;

  if (Movie::IsPlayingInput())
    SFPS = StringFromFormat("Input: %u/%u - VI: %u - FPS: %.0f - VPS: %.0f - %.0f%%",
                            (u32)Movie::GetCurrentInputCount(), (u32)Movie::GetTotalInputCount(),
                            (u32)Movie::GetCurrentFrame(), FPS, VPS, Speed);
  else if (Movie::IsRecordingInput())
    SFPS = StringFromFormat("Input: %u - VI: %u - FPS: %.0f - VPS: %.0f - %.0f%%",
                            (u32)Movie::GetCurrentInputCount(), (u32)Movie::GetCurrentFrame(), FPS,
                            VPS, Speed);
  else
  {
    SFPS = StringFromFormat("FPS: %.0f - VPS: %.0f - %.0f%%", FPS, VPS, Speed);
    if (SConfig::GetInstance().m_InterfaceExtendedFPSInfo)
    {
      // Use extended or summary information. The summary information does not print the ticks data,
      // that's more of a debugging interest, it can always be optional of course if someone is
      // interested.
      static u64 ticks = 0;
      static u64 idleTicks = 0;
      u64 newTicks = CoreTiming::GetTicks();
      u64 newIdleTicks = CoreTiming::GetIdleTicks();

      u64 diff = (newTicks - ticks) / 1000000;
      u64 idleDiff = (newIdleTicks - idleTicks) / 1000000;

      ticks = newTicks;
      idleTicks = newIdleTicks;

      float TicksPercentage =
          (float)diff / (float)(SystemTimers::GetTicksPerSecond() / 1000000) * 100;

      SFPS += StringFromFormat(" | CPU: ~%i MHz [Real: %i + IdleSkip: %i] / %i MHz (~%3.0f%%)",
                               (int)(diff), (int)(diff - idleDiff), (int)(idleDiff),
                               SystemTimers::GetTicksPerSecond() / 1000000, TicksPercentage);
    }
  }

  std::string message = StringFromFormat("%s | %s", SSettings.c_str(), SFPS.c_str());
  if (SConfig::GetInstance().m_show_active_title)
  {
    const std::string& title = SConfig::GetInstance().GetTitleDescription();
    if (!title.empty())
      message += " | " + title;
  }

  // Update the audio timestretcher with the current speed
  if (g_sound_stream)
  {
    Mixer* pMixer = g_sound_stream->GetMixer();
    pMixer->UpdateSpeed((float)Speed / 100);
  }

  Host_UpdateTitle(message);
}

void Shutdown()
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
  HostDispatchJobs();
}

void SetOnStateChangedCallback(StateChangedCallbackFunc callback)
{
  s_on_state_changed_callback = std::move(callback);
}

void UpdateWantDeterminism(bool initial)
{
  // For now, this value is not itself configurable.  Instead, individual
  // settings that depend on it, such as GPU determinism mode. should have
  // override options for testing,
  bool new_want_determinism = Movie::IsMovieActive() || NetPlay::IsNetPlayRunning();
  if (new_want_determinism != s_wants_determinism || initial)
  {
    NOTICE_LOG(COMMON, "Want determinism <- %s", new_want_determinism ? "true" : "false");

    RunAsCPUThread([&] {
      s_wants_determinism = new_want_determinism;
      const auto ios = IOS::HLE::GetIOS();
      if (ios)
        ios->UpdateWantDeterminism(new_want_determinism);
      Fifo::UpdateWantDeterminism(new_want_determinism);
      // We need to clear the cache because some parts of the JIT depend on want_determinism,
      // e.g. use of FMA.
      JitInterface::ClearCache();
    });
  }
}

void QueueHostJob(std::function<void()> job, bool run_during_stop)
{
  if (!job)
    return;

  bool send_message = false;
  {
    std::lock_guard<std::mutex> guard(s_host_jobs_lock);
    send_message = s_host_jobs_queue.empty();
    s_host_jobs_queue.emplace(HostJob{std::move(job), run_during_stop});
  }
  // If the the queue was empty then kick the Host to come and get this job.
  if (send_message)
    Host_Message(HostMessageID::WMUserJobDispatch);
}

void HostDispatchJobs()
{
  // WARNING: This should only run on the Host Thread.
  // NOTE: This function is potentially re-entrant. If a job calls
  //   Core::Stop for instance then we'll enter this a second time.
  std::unique_lock<std::mutex> guard(s_host_jobs_lock);
  while (!s_host_jobs_queue.empty())
  {
    HostJob job = std::move(s_host_jobs_queue.front());
    s_host_jobs_queue.pop();

    // NOTE: Memory ordering is important. The booting flag needs to be
    //   checked first because the state transition is:
    //   Core::State::Uninitialized: s_is_booting -> s_hardware_initialized
    //   We need to check variables in the same order as the state
    //   transition, otherwise we race and get transient failures.
    if (!job.run_after_stop && !s_is_booting.IsSet() && !IsRunning())
      continue;

    guard.unlock();
    job.job();
    guard.lock();
  }
}

// NOTE: Host Thread
void DoFrameStep()
{
  if (GetState() == State::Paused)
  {
    // if already paused, frame advance for 1 frame
    s_frame_step = true;
    RequestRefreshInfo();
    SetState(State::Running);
  }
  else if (!s_frame_step)
  {
    // if not paused yet, pause immediately instead
    SetState(State::Paused);
  }
}

}  // namespace Core
