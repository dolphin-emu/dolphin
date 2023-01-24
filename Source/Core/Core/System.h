// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

class GeometryShaderManager;
class Interpreter;
class JitInterface;
class PixelShaderManager;
class SoundStream;
struct Sram;
class VertexShaderManager;

namespace AudioInterface
{
class AudioInterfaceManager;
};
namespace CPU
{
class CPUManager;
}
namespace CommandProcessor
{
class CommandProcessorManager;
}
namespace CoreTiming
{
class CoreTimingManager;
}
namespace DSP
{
class DSPManager;
}
namespace DVD
{
class DVDInterface;
class DVDThread;
}  // namespace DVD
namespace ExpansionInterface
{
class ExpansionInterfaceManager;
};
namespace Fifo
{
class FifoManager;
}
namespace GPFifo
{
class GPFifoManager;
}
namespace HSP
{
class HSPManager;
}
namespace IOS::HLE::USB
{
class SkylanderPortal;
class InfinityBase;
};  // namespace IOS::HLE::USB
namespace Memory
{
class MemoryManager;
};
namespace MemoryInterface
{
class MemoryInterfaceManager;
};
namespace PixelEngine
{
class PixelEngineManager;
};
namespace PowerPC
{
class MMU;
class PowerPCManager;
struct PowerPCState;
}  // namespace PowerPC
namespace ProcessorInterface
{
class ProcessorInterfaceManager;
}
namespace SerialInterface
{
class SerialInterfaceManager;
};
namespace VideoInterface
{
class VideoInterfaceManager;
};

namespace Core
{
// Central class that encapsulates the running system.
class System
{
public:
  ~System();

  System(const System&) = delete;
  System& operator=(const System&) = delete;

  System(System&&) = delete;
  System& operator=(System&&) = delete;

  // Intermediate instance accessor until global state is eliminated.
  static System& GetInstance()
  {
    static System instance;
    return instance;
  }

  void Initialize();

  bool IsDualCoreMode() const { return m_separate_cpu_and_gpu_threads; }
  bool IsMMUMode() const { return m_mmu_enabled; }
  bool IsPauseOnPanicMode() const { return m_pause_on_panic_enabled; }

  SoundStream* GetSoundStream() const;
  void SetSoundStream(std::unique_ptr<SoundStream> sound_stream);
  bool IsSoundStreamRunning() const;
  void SetSoundStreamRunning(bool running);
  bool IsAudioDumpStarted() const;
  void SetAudioDumpStarted(bool started);

  AudioInterface::AudioInterfaceManager& GetAudioInterface() const;
  CPU::CPUManager& GetCPU() const;
  CoreTiming::CoreTimingManager& GetCoreTiming() const;
  CommandProcessor::CommandProcessorManager& GetCommandProcessor() const;
  DSP::DSPManager& GetDSP() const;
  DVD::DVDInterface& GetDVDInterface() const;
  DVD::DVDThread& GetDVDThread() const;
  ExpansionInterface::ExpansionInterfaceManager& GetExpansionInterface() const;
  Fifo::FifoManager& GetFifo() const;
  GeometryShaderManager& GetGeometryShaderManager() const;
  GPFifo::GPFifoManager& GetGPFifo() const;
  HSP::HSPManager& GetHSP() const;
  Interpreter& GetInterpreter() const;
  JitInterface& GetJitInterface() const;
  IOS::HLE::USB::SkylanderPortal& GetSkylanderPortal() const;
  IOS::HLE::USB::InfinityBase& GetInfinityBase() const;
  Memory::MemoryManager& GetMemory() const;
  MemoryInterface::MemoryInterfaceManager& GetMemoryInterface() const;
  PowerPC::MMU& GetMMU() const;
  PixelEngine::PixelEngineManager& GetPixelEngine() const;
  PixelShaderManager& GetPixelShaderManager() const;
  PowerPC::PowerPCManager& GetPowerPC() const;
  PowerPC::PowerPCState& GetPPCState() const;
  ProcessorInterface::ProcessorInterfaceManager& GetProcessorInterface() const;
  SerialInterface::SerialInterfaceManager& GetSerialInterface() const;
  Sram& GetSRAM() const;
  VertexShaderManager& GetVertexShaderManager() const;
  VideoInterface::VideoInterfaceManager& GetVideoInterface() const;

private:
  System();

  struct Impl;
  std::unique_ptr<Impl> m_impl;

  bool m_separate_cpu_and_gpu_threads = false;
  bool m_mmu_enabled = false;
  bool m_pause_on_panic_enabled = false;
};
}  // namespace Core
