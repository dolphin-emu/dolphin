// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/System.h"

#include <memory>

#include "AudioCommon/SoundStream.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/HW/Sram.h"
#include "Core/HW/VideoInterface.h"

namespace Core
{
struct System::Impl
{
  std::unique_ptr<SoundStream> m_sound_stream;
  bool m_sound_stream_running = false;
  bool m_audio_dump_started = false;

  AudioInterface::AudioInterfaceState m_audio_interface_state;
  DSP::DSPState m_dsp_state;
  DVDInterface::DVDInterfaceState m_dvd_interface_state;
  DVDThread::DVDThreadState m_dvd_thread_state;
  Sram m_sram;
  VideoInterface::VideoInterfaceState m_video_interface_state;
};

System::System() : m_impl{std::make_unique<Impl>()}
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

AudioInterface::AudioInterfaceState& System::GetAudioInterfaceState() const
{
  return m_impl->m_audio_interface_state;
}

DSP::DSPState& System::GetDSPState() const
{
  return m_impl->m_dsp_state;
}

DVDInterface::DVDInterfaceState& System::GetDVDInterfaceState() const
{
  return m_impl->m_dvd_interface_state;
}

DVDThread::DVDThreadState& System::GetDVDThreadState() const
{
  return m_impl->m_dvd_thread_state;
}

Sram& System::GetSRAM() const
{
  return m_impl->m_sram;
}

VideoInterface::VideoInterfaceState& System::GetVideoInterfaceState() const
{
  return m_impl->m_video_interface_state;
}
}  // namespace Core
