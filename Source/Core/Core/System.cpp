// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/System.h"

#include <memory>

#include "AudioCommon/SoundStream.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/DVD/DVDThread.h"

namespace Core
{
struct System::Impl
{
  std::unique_ptr<SoundStream> m_sound_stream;
  bool m_sound_stream_running = false;
  bool m_audio_dump_started = false;

  DVDThread::DVDThreadState m_dvd_thread_state;
};

System::System() : m_impl{std::make_unique<Impl>()}
{
}

System::~System() = default;

void System::Initialize()
{
  m_separate_cpu_and_gpu_threads = Config::Get(Config::MAIN_CPU_THREAD);
  m_mmu_enabled = Config::Get(Config::MAIN_MMU);
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

DVDThread::DVDThreadState& System::GetDVDThreadState() const
{
  return m_impl->m_dvd_thread_state;
}
}  // namespace Core
