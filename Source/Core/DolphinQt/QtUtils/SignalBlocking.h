// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSignalBlocker>

// Helper class for populating a GUI element without triggering its data change signals.

template <typename T>
class SignalBlockerProxy
{
public:
  explicit SignalBlockerProxy(T* object) : m_object(object), m_blocker(object) {}
  SignalBlockerProxy(const SignalBlockerProxy& other) = delete;
  SignalBlockerProxy(SignalBlockerProxy&& other) = default;
  SignalBlockerProxy& operator=(const SignalBlockerProxy& other) = delete;
  SignalBlockerProxy& operator=(SignalBlockerProxy&& other) = default;
  ~SignalBlockerProxy() = default;

  T* operator->() const { return m_object; }

private:
  T* m_object;
  QSignalBlocker m_blocker;
};

template <typename T>
SignalBlockerProxy<T> SignalBlocking(T* object)
{
  return SignalBlockerProxy<T>(object);
}
