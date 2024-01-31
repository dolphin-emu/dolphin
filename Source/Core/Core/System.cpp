// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/System.h"

#include <memory>

#include "AudioCommon/SoundStream.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/HSP/HSP.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/MemoryInterface.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/Sram.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/WII_IPC.h"
#include "Core/Movie.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "IOS/USB/Emulated/Infinity.h"
#include "IOS/USB/Emulated/Skylanders/Skylander.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFStateManager.h"

#ifdef _WIN32
#include "VideoBackends/D3D/VideoBackend.h"
#include "VideoBackends/D3D12/VideoBackend.h"
#endif
#include "VideoBackends/Null/VideoBackend.h"
#ifdef HAS_OPENGL
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoBackends/Software/VideoBackend.h"
#endif
#ifdef HAS_VULKAN
#include "VideoBackends/Vulkan/VideoBackend.h"
#endif
#ifdef __APPLE__
#include "VideoBackends/Metal/VideoBackend.h"
#endif

namespace Core
{
struct System::Impl
{
  explicit Impl(System& system)
      : m_audio_interface(system), m_core_timing(system), m_command_processor{system},
        m_cpu(system), m_dsp(system), m_dvd_interface(system), m_dvd_thread(system),
        m_expansion_interface(system), m_fifo{system}, m_gp_fifo(system), m_wii_ipc(system),
        m_memory(system), m_pixel_engine{system}, m_power_pc(system),
        m_mmu(system, m_memory, m_power_pc), m_processor_interface(system),
        m_serial_interface(system), m_system_timers(system), m_video_interface(system),
        m_interpreter(system, m_power_pc.GetPPCState(), m_mmu), m_jit_interface(system),
        m_fifo_player(system), m_fifo_recorder(system),
        m_movie(system), m_available_video_backends{MakeAvailableVideoBackends()}
  {
  }

  VideoBackendBase* GetDefaultVideoBackend() const;
  std::string GetDefaultVideoBackendName() const;
  void PopulateVideoBackendInfo(const WindowSystemInfo& wsi);
  void PopulateVideoBackendInfoFromUI(const WindowSystemInfo& wsi);
  void ActivateVideoBackend(std::string_view name);
  std::vector<std::unique_ptr<VideoBackendBase>> MakeAvailableVideoBackends();

  std::unique_ptr<SoundStream> m_sound_stream;
  bool m_sound_stream_running = false;
  bool m_audio_dump_started = false;

  AudioInterface::AudioInterfaceManager m_audio_interface;
  CoreTiming::CoreTimingManager m_core_timing;
  CommandProcessor::CommandProcessorManager m_command_processor;
  CPU::CPUManager m_cpu;
  DSP::DSPManager m_dsp;
  DVD::DVDInterface m_dvd_interface;
  DVD::DVDThread m_dvd_thread;
  ExpansionInterface::ExpansionInterfaceManager m_expansion_interface;
  Fifo::FifoManager m_fifo;
  GeometryShaderManager m_geometry_shader_manager;
  GPFifo::GPFifoManager m_gp_fifo;
  HSP::HSPManager m_hsp;
  IOS::HLE::USB::InfinityBase m_infinity_base;
  IOS::HLE::USB::SkylanderPortal m_skylander_portal;
  IOS::WiiIPC m_wii_ipc;
  Memory::MemoryManager m_memory;
  MemoryInterface::MemoryInterfaceManager m_memory_interface;
  PixelEngine::PixelEngineManager m_pixel_engine;
  PixelShaderManager m_pixel_shader_manager;
  PowerPC::PowerPCManager m_power_pc;
  PowerPC::MMU m_mmu;
  ProcessorInterface::ProcessorInterfaceManager m_processor_interface;
  SerialInterface::SerialInterfaceManager m_serial_interface;
  Sram m_sram;
  SystemTimers::SystemTimersManager m_system_timers;
  VertexShaderManager m_vertex_shader_manager;
  XFStateManager m_xf_state_manager;
  VideoInterface::VideoInterfaceManager m_video_interface;
  Interpreter m_interpreter;
  JitInterface m_jit_interface;
  VideoCommon::CustomAssetLoader m_custom_asset_loader;
  FifoPlayer m_fifo_player;
  FifoRecorder m_fifo_recorder;
  Movie::MovieManager m_movie;

  VideoBackendBase* m_video_backend = nullptr;
  std::vector<std::unique_ptr<VideoBackendBase>> m_available_video_backends;
};

System::System() : m_impl{std::make_unique<Impl>(*this)}
{
}

System::~System() = default;

void System::Initialize()
{
  m_separate_cpu_and_gpu_threads = Config::Get(Config::MAIN_CPU_THREAD);
  m_mmu_enabled = Config::Get(Config::MAIN_MMU);
  m_pause_on_panic_enabled = Config::Get(Config::MAIN_PAUSE_ON_PANIC);
}

SoundStream* System::GetSoundStream() const
{
  return m_impl->m_sound_stream.get();
}

void System::SetSoundStream(std::unique_ptr<SoundStream> sound_stream)
{
  m_impl->m_sound_stream = std::move(sound_stream);
}

bool System::IsSoundStreamRunning() const
{
  return m_impl->m_sound_stream_running;
}

void System::SetSoundStreamRunning(bool running)
{
  m_impl->m_sound_stream_running = running;
}

bool System::IsAudioDumpStarted() const
{
  return m_impl->m_audio_dump_started;
}

void System::SetAudioDumpStarted(bool started)
{
  m_impl->m_audio_dump_started = started;
}

AudioInterface::AudioInterfaceManager& System::GetAudioInterface() const
{
  return m_impl->m_audio_interface;
}

CPU::CPUManager& System::GetCPU() const
{
  return m_impl->m_cpu;
}

CoreTiming::CoreTimingManager& System::GetCoreTiming() const
{
  return m_impl->m_core_timing;
}

CommandProcessor::CommandProcessorManager& System::GetCommandProcessor() const
{
  return m_impl->m_command_processor;
}

DSP::DSPManager& System::GetDSP() const
{
  return m_impl->m_dsp;
}

DVD::DVDInterface& System::GetDVDInterface() const
{
  return m_impl->m_dvd_interface;
}

DVD::DVDThread& System::GetDVDThread() const
{
  return m_impl->m_dvd_thread;
}

ExpansionInterface::ExpansionInterfaceManager& System::GetExpansionInterface() const
{
  return m_impl->m_expansion_interface;
}

Fifo::FifoManager& System::GetFifo() const
{
  return m_impl->m_fifo;
}

FifoPlayer& System::GetFifoPlayer() const
{
  return m_impl->m_fifo_player;
}

FifoRecorder& System::GetFifoRecorder() const
{
  return m_impl->m_fifo_recorder;
}

GeometryShaderManager& System::GetGeometryShaderManager() const
{
  return m_impl->m_geometry_shader_manager;
}

GPFifo::GPFifoManager& System::GetGPFifo() const
{
  return m_impl->m_gp_fifo;
}

HSP::HSPManager& System::GetHSP() const
{
  return m_impl->m_hsp;
}

Interpreter& System::GetInterpreter() const
{
  return m_impl->m_interpreter;
}

JitInterface& System::GetJitInterface() const
{
  return m_impl->m_jit_interface;
}

IOS::HLE::USB::SkylanderPortal& System::GetSkylanderPortal() const
{
  return m_impl->m_skylander_portal;
}

IOS::HLE::USB::InfinityBase& System::GetInfinityBase() const
{
  return m_impl->m_infinity_base;
}

IOS::WiiIPC& System::GetWiiIPC() const
{
  return m_impl->m_wii_ipc;
}

Memory::MemoryManager& System::GetMemory() const
{
  return m_impl->m_memory;
}

MemoryInterface::MemoryInterfaceManager& System::GetMemoryInterface() const
{
  return m_impl->m_memory_interface;
}

PowerPC::MMU& System::GetMMU() const
{
  return m_impl->m_mmu;
}

Movie::MovieManager& System::GetMovie() const
{
  return m_impl->m_movie;
}

PixelEngine::PixelEngineManager& System::GetPixelEngine() const
{
  return m_impl->m_pixel_engine;
}

PixelShaderManager& System::GetPixelShaderManager() const
{
  return m_impl->m_pixel_shader_manager;
}

PowerPC::PowerPCManager& System::GetPowerPC() const
{
  return m_impl->m_power_pc;
}

PowerPC::PowerPCState& System::GetPPCState() const
{
  return m_impl->m_power_pc.GetPPCState();
}

ProcessorInterface::ProcessorInterfaceManager& System::GetProcessorInterface() const
{
  return m_impl->m_processor_interface;
}

SerialInterface::SerialInterfaceManager& System::GetSerialInterface() const
{
  return m_impl->m_serial_interface;
}

Sram& System::GetSRAM() const
{
  return m_impl->m_sram;
}

SystemTimers::SystemTimersManager& System::GetSystemTimers() const
{
  return m_impl->m_system_timers;
}

VertexShaderManager& System::GetVertexShaderManager() const
{
  return m_impl->m_vertex_shader_manager;
}

XFStateManager& System::GetXFStateManager() const
{
  return m_impl->m_xf_state_manager;
}

VideoInterface::VideoInterfaceManager& System::GetVideoInterface() const
{
  return m_impl->m_video_interface;
}

VideoCommon::CustomAssetLoader& System::GetCustomAssetLoader() const
{
  return m_impl->m_custom_asset_loader;
}

VideoBackendBase* System::GetVideoBackend() const
{
  return m_impl->m_video_backend;
}

const std::vector<std::unique_ptr<VideoBackendBase>>& System::GetAvailableVideoBackends() const
{
  return m_impl->m_available_video_backends;
}

void System::PopulateVideoBackendInfo(const WindowSystemInfo& wsi)
{
  m_impl->PopulateVideoBackendInfo(wsi);
}

void System::PopulateVideoBackendInfoFromUI(const WindowSystemInfo& wsi)
{
  m_impl->PopulateVideoBackendInfoFromUI(wsi);
}

void System::ActivateVideoBackend(std::string_view name)
{
  m_impl->ActivateVideoBackend(name);
}

std::string System::GetDefaultVideoBackendName() const
{
  return m_impl->GetDefaultVideoBackendName();
}

std::vector<std::unique_ptr<VideoBackendBase>> System::Impl::MakeAvailableVideoBackends()
{
  std::vector<std::unique_ptr<VideoBackendBase>> backends;

  // OGL > D3D11 > D3D12 > Vulkan > SW > Null
  // On macOS, we prefer Vulkan over OpenGL due to OpenGL support being deprecated by Apple.
#ifdef HAS_OPENGL
  backends.push_back(std::make_unique<OGL::VideoBackend>());
#endif
#ifdef _WIN32
  backends.push_back(std::make_unique<DX11::VideoBackend>());
  backends.push_back(std::make_unique<DX12::VideoBackend>());
#endif
#ifdef HAS_VULKAN
#ifdef __APPLE__
  // Emplace the Vulkan backend at the beginning so it takes precedence over OpenGL.
  backends.emplace(backends.begin(), std::make_unique<Vulkan::VideoBackend>());
#else
  backends.push_back(std::make_unique<Vulkan::VideoBackend>());
#endif
#endif
#ifdef __APPLE__
  backends.emplace(backends.begin(), std::make_unique<Metal::VideoBackend>());
#endif
#ifdef HAS_OPENGL
  backends.push_back(std::make_unique<SW::VideoSoftware>());
#endif
  backends.push_back(std::make_unique<Null::VideoBackend>());

  if (!backends.empty())
    m_video_backend = backends.front().get();

  return backends;
}

std::string System::Impl::GetDefaultVideoBackendName() const
{
  const auto* default_backend = GetDefaultVideoBackend();
  return default_backend ? default_backend->GetName() : "";
}

VideoBackendBase* System::Impl::GetDefaultVideoBackend() const
{
  if (m_available_video_backends.empty())
    return nullptr;

  return m_available_video_backends.front().get();
}

void System::Impl::PopulateVideoBackendInfo(const WindowSystemInfo& wsi)
{
  g_Config.Refresh();

  // Reset backend_info so if the backend forgets to initialize something it doesn't end up using
  // a value from the previously used renderer
  g_Config.backend_info = {};

  ActivateVideoBackend(Config::Get(Config::MAIN_GFX_BACKEND));
  g_Config.backend_info.DisplayName = m_video_backend->GetDisplayName();
  m_video_backend->InitBackendInfo(wsi);

  // We validate the config after initializing the backend info, as system-specific settings
  // such as anti-aliasing, or the selected adapter may be invalid, and should be checked.
  g_Config.VerifyValidity();
}

void System::Impl::PopulateVideoBackendInfoFromUI(const WindowSystemInfo& wsi)
{
  // If the core is running, the backend info will have been populated already.
  // If we did it here, the UI thread can race with the GPU thread.
  if (Core::IsRunning())
    return;

  PopulateVideoBackendInfo(wsi);
}

void System::Impl::ActivateVideoBackend(std::string_view name)
{
  // If empty, set it to the default backend (expected behavior)
  if (name.empty())
    m_video_backend = GetDefaultVideoBackend();

  const auto& backends = m_available_video_backends;
  const auto iter = std::find_if(backends.begin(), backends.end(), [&name](const auto& backend) {
    return name == backend->GetName();
  });

  if (iter == backends.end())
    return;

  m_video_backend = iter->get();
}

}  // namespace Core
