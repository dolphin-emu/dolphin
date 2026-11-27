// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <span>

#include "Common/CommonTypes.h"

namespace GCAdapter
{
constexpr std::size_t INPUT_PAYLOAD_SIZE = 37;
constexpr std::size_t OUTPUT_PAYLOAD_SIZE = 5;

// Implementations shall attempt to have an adapter open whenever available.
// The current open state can be observed with callbacks or IsConnected().
// The public interface is intended to be thread safe.
// Destruction closes the adapter.
class Backend
{
public:
  using InputPayload = std::array<u8, INPUT_PAYLOAD_SIZE>;
  using OutputPayload = std::array<u8, OUTPUT_PAYLOAD_SIZE>;

  // Implementations shall *not* fire callbacks from within a Read/Write.
  // That would make dealing with deadlock scenarios inconvenient.
  struct Callbacks
  {
    // It's *not* safe to destruct the Backend from within these callbacks.
    std::function<void()> on_connected;
    std::function<void()> on_disconnected;
  };

  void SetCallbacks(Callbacks callbacks)
  {
    std::lock_guard lk{m_callbacks_mutex};
    m_callbacks = std::move(callbacks);
  }

  Backend(Callbacks callbacks) : m_callbacks{std::move(callbacks)} {}

  virtual ~Backend() = default;

  virtual bool IsConnected(std::string* error_message = nullptr) const = 0;

  // Read/Write will timeout after an implementation-specific amount of time.

  // Returns the number of bytes transferred, or a negative value on failure.
  // Note: The Android implementation currently requires an exact buffer size of 37
  //  as this is the only size we need at the moment.
  virtual int Read(std::span<u8> buffer) = 0;

  // Returns the number of bytes transferred, or a negative value on failure.
  virtual int Write(std::span<const u8> buffer) = 0;

protected:
  void InvokeCallback(auto callbacks_ptr_to_mem)
  {
    std::lock_guard lk{m_callbacks_mutex};
    if (auto& cb = std::invoke(callbacks_ptr_to_mem, &m_callbacks))
      std::invoke(cb);
  }

private:
  std::recursive_mutex m_callbacks_mutex;
  Callbacks m_callbacks;
};

// Creates a platform-specific backend instance.
// Note: Android currently only supports one of these at a time.
std::unique_ptr<Backend> CreateBackend(Backend::Callbacks callbacks = {});
}  // namespace GCAdapter
