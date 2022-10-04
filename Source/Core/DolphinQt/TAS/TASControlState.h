// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include "Common/CommonTypes.h"

class TASControlState
{
public:
  // Call this from the CPU thread to get the current value. (This function can also safely be
  // called from the UI thread, but you're effectively just getting the value the UI control has.)
  int GetValue() const;
  // Call this from the CPU thread when the controller state changes.
  // If the return value is true, queue up a call to ApplyControllerChangeValue on the UI thread.
  bool OnControllerValueChanged(int new_value);
  // Call this from the UI thread when the user changes the value using the UI.
  void OnUIValueChanged(int new_value);
  // Call this from the UI thread after OnControllerValueChanged returns true,
  // and set the state of the UI control to the return value.
  int ApplyControllerValueChange();

private:
  // A description of how threading is handled: The UI thread can update its copy of the state
  // whenever it wants to, and must *not* increment the version when doing so. The CPU thread can
  // update its copy of the state whenever it wants to, and *must* increment the version when doing
  // so. When the CPU thread updates its copy of the state, the UI thread should then (possibly
  // after a delay) mirror the change by copying the CPU thread's state to the UI thread's state.
  // This mirroring is the only way for the version number stored in the UI thread's state to
  // change. The version numbers of the two copies can be compared to check if the UI thread's view
  // of what has happened on the CPU thread is up to date.

  struct State
  {
    int version = 0;
    int value = 0;
  };

  std::atomic<State> m_ui_thread_state;
  std::atomic<State> m_cpu_thread_state;
};
