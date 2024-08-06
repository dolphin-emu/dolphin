// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASControlState.h"

#include <atomic>

int TASControlState::GetValue() const
{
  const State ui_thread_state = m_ui_thread_state.load(std::memory_order_relaxed);
  const State cpu_thread_state = m_cpu_thread_state.load(std::memory_order_relaxed);

  return (ui_thread_state.version != cpu_thread_state.version ? cpu_thread_state : ui_thread_state)
      .value;
}

bool TASControlState::OnControllerValueChanged(const int new_value)
{
  const auto [version, value] = m_cpu_thread_state.load(std::memory_order_relaxed);

  if (value == new_value)
  {
    // The CPU thread state is already up to date with the controller. No need to do anything
    return false;
  }

  const State new_state{(version + 1), new_value};
  m_cpu_thread_state.store(new_state, std::memory_order_relaxed);

  return true;
}

void TASControlState::OnUIValueChanged(const int new_value)
{
  const auto [version, _value] = m_ui_thread_state.load(std::memory_order_relaxed);

  const State new_state{version, new_value};
  m_ui_thread_state.store(new_state, std::memory_order_relaxed);
}

int TASControlState::ApplyControllerValueChange()
{
  const auto [version, value] = m_ui_thread_state.load(std::memory_order_relaxed);
  const State cpu_thread_state = m_cpu_thread_state.load(std::memory_order_relaxed);

  if (version == cpu_thread_state.version)
  {
    // The UI thread state is already up to date with the CPU thread. No need to do anything
    return value;
  }
  m_ui_thread_state.store(cpu_thread_state, std::memory_order_relaxed);
  return cpu_thread_state.value;
}
