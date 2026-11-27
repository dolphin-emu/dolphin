// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/GCAdapter.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>

#include <fmt/ranges.h>

#include "Common/BitUtils.h"
#include "Common/Config/Config.h"
#include "Common/Event.h"
#include "Common/ScopeGuard.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "InputCommon/GCAdapterBackend.h"
#include "InputCommon/GCPadStatus.h"

namespace GCAdapter
{
namespace
{
enum class ControllerType : u8
{
  None = 0,
  Wired = 1,
  Wireless = 2,
};

// Get ControllerType from first byte in input payload.
ControllerType IdentifyControllerType(u8 data)
{
  if (Common::ExtractBit<4>(data))
    return ControllerType::Wired;

  if (Common::ExtractBit<5>(data))
    return ControllerType::Wireless;

  return ControllerType::None;
}

static void ProcessPortInputs(std::span<const u8, 9> port_data, GCPadStatus* pad_status)
{
  auto& pad = *pad_status;

  const u8 b1 = port_data[1];
  for (auto bit_index = 0uz;
       auto button_value : {PAD_BUTTON_A, PAD_BUTTON_B, PAD_BUTTON_X, PAD_BUTTON_Y, PAD_BUTTON_LEFT,
                            PAD_BUTTON_RIGHT, PAD_BUTTON_DOWN, PAD_BUTTON_UP})
  {
    if (Common::ExtractBit(b1, bit_index++))
      pad.button |= button_value;
  }

  const u8 b2 = port_data[2];
  for (auto bit_index = 0uz;
       auto button_value : {PAD_BUTTON_START, PAD_TRIGGER_Z, PAD_TRIGGER_R, PAD_TRIGGER_L})
  {
    if (Common::ExtractBit(b2, bit_index++))
      pad.button |= button_value;
  }

  pad.stickX = port_data[3];
  pad.stickY = port_data[4];
  pad.substickX = port_data[5];
  pad.substickY = port_data[6];
  pad.triggerLeft = port_data[7];
  pad.triggerRight = port_data[8];
}

struct EMAFrequencyTracker
{
  double frequency = 0.0;
  TimePoint last_update = std::chrono::steady_clock::now();

  double UpdateNow(double alpha = 0.1)
  {
    const auto now = std::chrono::steady_clock::now();
    const auto dt = DT_s(now - std::exchange(last_update, now));

    if (dt < DT_us{1.0})
      return frequency;

    frequency = std::lerp(frequency, 1.0 / dt.count(), (frequency > 0) ? alpha : 1.0);
    return frequency;
  }

  double Decay(double alpha = 0.1) { return frequency *= (1.0 - alpha); }
};

// Adapts a GCAdapter::Backend instance to our GCAdapter global namespace interface.
class BackendAdapter
{
public:
  BackendAdapter() { RefreshConfig(); }

  ~BackendAdapter()
  {
    m_config_callback.reset();
    DestructBackend();
  }

  GCPadStatus Input(int chan)
  {
    std::lock_guard lk{m_read_mutex};

    auto& pad_state = m_port_states[chan];

    if (std::exchange(pad_state.is_new_connection, false))
    {
      // Return the "origin" state for the first input on a new connection.
      return pad_state.origin;
    }

    return pad_state.status;
  }

  void Output(int chan, u8 rumble_command)
  {
    auto& port_state = m_port_states[chan];

    if (port_state.rumble_command.exchange(rumble_command, std::memory_order_relaxed) !=
            rumble_command &&
        port_state.config_rumble_enabled.load(std::memory_order_relaxed))
    {
      m_write_event.Set();
    }
  }

  void ResetRumble()
  {
    bool changed = false;
    for (auto& state : m_port_states)
      changed |= (state.rumble_command.exchange(0, std::memory_order_relaxed) != 0);

    if (changed)
      m_write_event.Set();
  }

  bool IsDetected(std::string* error_message) const
  {
    std::lock_guard lk{m_backend_mutex};
    return m_backend && m_backend->IsConnected(error_message);
  }

  bool DeviceConnected(int chan) const
  {
    return m_port_states[chan].controller_type.load(std::memory_order_relaxed) !=
           ControllerType::None;
  }

  void ResetDeviceType(int chan)
  {
    m_port_states[chan].controller_type.store(ControllerType::None, std::memory_order_relaxed);
  }

  void ResetAllPorts()
  {
    for (int i = 0; i != SerialInterface::MAX_SI_CHANNELS; ++i)
      ResetDeviceType(i);

    m_poll_rate.store(0, std::memory_order_relaxed);
  }

  double GetCurrentPollRate() const { return m_poll_rate.load(std::memory_order_relaxed); }

private:
  // Backend is constructed/destructed based on user config wanting to use GCAdapter.
  void ConstructBackend()
  {
    std::lock_guard lk{m_backend_mutex};

    if (m_backend)
      return;

    m_backend = CreateBackend({
        .on_connected =
            [this] {
              NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter connected");
              OnConnected();
            },
        .on_disconnected =
            [this] {
              NOTICE_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter disconnected");
              OnDisconnected();
            },
    });
  }

  void DestructBackend()
  {
    std::lock_guard lk{m_backend_mutex};

    if (!m_backend)
      return;

    m_backend->SetCallbacks({});

    OnDisconnected();

    m_backend.reset();
  }

  // Callbacks from Backend
  void OnConnected()
  {
    if (m_read_thread.joinable())
      return;

    ResetRumble();

    m_read_thread = std::jthread{std::bind_front(&BackendAdapter::ReadThreadFunc, this)};
    m_write_thread = std::jthread{std::bind_front(&BackendAdapter::WriteThreadFunc, this)};
  }

  void OnDisconnected()
  {
    m_write_thread = {};
    m_read_thread = {};

    ResetAllPorts();
  }

  void ProcessInputPayload(const Backend::InputPayload& payload)
  {
    if (payload[0] != 0x21)
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "Unexpected payload[0]: {:02x}", payload[0]);
      return;
    }

    for (auto chan = 0uz; chan != SerialInterface::MAX_SI_CHANNELS; ++chan)
    {
      const auto port_data = std::span{payload}.subspan(1 + (9 * chan)).first<9>();

      const ControllerType controller_type = IdentifyControllerType(port_data[0]);

      PortState& port_state = m_port_states[chan];

      GCPadStatus pad{};

      if (controller_type != ControllerType::None)
      {
        ProcessPortInputs(port_data, &pad);
      }
      else if (!Core::WantsDeterminism())
      {
        // This is a hack to prevent a desync due to SI devices
        // being different and returning different values.
        // The corresponding code in DeviceGCAdapter has the same check
        pad.button = PAD_ERR_STATUS;
      }

      if (port_state.controller_type.exchange(controller_type, std::memory_order_relaxed) !=
          controller_type)
      {
        if (controller_type != ControllerType::None)
        {
          NOTICE_LOG_FMT(CONTROLLERINTERFACE, "New device connected to Port {} of Type: {:02x}",
                         chan + 1, port_data[0]);

          // Save the "origin" on a new connection.
          port_state.origin = pad;
          port_state.origin.button |= PAD_GET_ORIGIN;

          port_state.is_new_connection = true;
        }
        else
        {
          NOTICE_LOG_FMT(CONTROLLERINTERFACE, "Device disconnected from Port {}", chan + 1);
        }
      }

      port_state.status = pad;
    }
  }

  void ReadThreadFunc(std::stop_token stoken)
  {
    Common::SetCurrentThreadName("GCAdapter Read");

    // Measure poll rate for display in UI.
    EMAFrequencyTracker poll_rate_tracker;

    u32 consecutive_read_failures = 0;

    Backend::InputPayload input_payload;

    while (!stoken.stop_requested())
    {
      if (m_backend->Read(input_payload) != INPUT_PAYLOAD_SIZE)
      {
        m_poll_rate.store(poll_rate_tracker.Decay(), std::memory_order_relaxed);

        if (++consecutive_read_failures == 10)
        {
          ERROR_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter had many read failures");

          // What should we really do?

          m_poll_rate = {};
          consecutive_read_failures = 0;
          ResetAllPorts();
          Common::InterruptableSleepFor(stoken, std::chrono::milliseconds{1000});
          continue;
        }

        WARN_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter read failed");
        // Pause for ~10 to ~1000ms.
        Common::InterruptableSleepFor(stoken, DT_ms{5 * std::pow(1.8, consecutive_read_failures)});
        continue;
      }

      m_poll_rate.store(poll_rate_tracker.UpdateNow(), std::memory_order_relaxed);

      consecutive_read_failures = 0;

      std::lock_guard lk{m_read_mutex};
      ProcessInputPayload(input_payload);
    }
  }

  void WriteThreadFunc(std::stop_token stoken)
  {
    Common::SetCurrentThreadName("GCAdapter Write");

    m_write_event.Reset();  // Avoid more than one write on startup.

    std::stop_callback ck{stoken, [this] { m_write_event.Set(); }};

    Backend::OutputPayload write_payload = {0x11};

    while (!stoken.stop_requested())
    {
      for (int i = 1; auto& state : m_port_states)
      {
        write_payload[i++] = state.config_rumble_enabled.load(std::memory_order_relaxed) ?
                                 state.rumble_command.load(std::memory_order_relaxed) :
                                 0x00;
      }

      DEBUG_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter writing payload: {:02x}",
                    fmt::join(write_payload, " "));

      if (m_backend->Write(write_payload) != OUTPUT_PAYLOAD_SIZE)
      {
        WARN_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter write failed");

        // Try again in a second in hopes to avoid rumble being stuck on.
        m_write_event.WaitFor(std::chrono::milliseconds{1000});
        continue;
      }

      m_write_event.Wait();
    }

    // Reset rumble on stop if necessary.
    Backend::OutputPayload reset_rumble = {0x11};
    if (!std::ranges::equal(reset_rumble, write_payload))
      m_backend->Write(reset_rumble);
  }

  void RefreshConfig()
  {
    bool is_adapter_wanted = false;
    bool rumble_changed = false;
    for (int i = 0; i != SerialInterface::MAX_SI_CHANNELS; ++i)
    {
      is_adapter_wanted |= Config::Get(Config::GetInfoForSIDevice(i)) ==
                           SerialInterface::SIDevices::SIDEVICE_WIIU_ADAPTER;

      const bool new_rumble_enabled = Config::Get(Config::GetInfoForAdapterRumble(i));
      rumble_changed |= (m_port_states[i].config_rumble_enabled.exchange(
                             new_rumble_enabled, std::memory_order_relaxed) != new_rumble_enabled);
    }

    if (rumble_changed)
      m_write_event.Set();

    if (is_adapter_wanted)
      ConstructBackend();
    else
      DestructBackend();
  }

  struct PortState
  {
    GCPadStatus origin = {};
    GCPadStatus status = {};

    std::atomic<u8> rumble_command{};
    std::atomic_bool config_rumble_enabled{};

    std::atomic<ControllerType> controller_type{ControllerType::None};
    bool is_new_connection = false;
  };
  std::array<PortState, SerialInterface::MAX_SI_CHANNELS> m_port_states{};

  // Protects altering much of PortState.
  std::mutex m_read_mutex;

  std::atomic<double> m_poll_rate{};

  mutable std::recursive_mutex m_backend_mutex;
  std::unique_ptr<Backend> m_backend;

  Common::Event m_write_event;

  std::jthread m_read_thread;
  std::jthread m_write_thread;

  Config::ScopedConfigChangedCallbackID m_config_callback =
      Config::AddScopedConfigChangedCallback([this] { RefreshConfig(); });
};

std::unique_ptr<BackendAdapter> s_backend_adapter;
}  // namespace

void Init()
{
  s_backend_adapter = std::make_unique<BackendAdapter>();
}

void Shutdown()
{
  s_backend_adapter.reset();
}

GCPadStatus Input(int chan)
{
  return s_backend_adapter->Input(chan);
}

void Output(int chan, u8 rumble_command)
{
  s_backend_adapter->Output(chan, rumble_command);
}

void ResetRumble()
{
  s_backend_adapter->ResetRumble();
}

bool IsDetected(std::string* error_message)
{
  return s_backend_adapter->IsDetected(error_message);
}

bool DeviceConnected(int chan)
{
  return s_backend_adapter->DeviceConnected(chan);
}

void ResetDeviceType(int chan)
{
  s_backend_adapter->ResetDeviceType(chan);
}

double GetCurrentPollRate()
{
  return s_backend_adapter->GetCurrentPollRate();
}
}  // namespace GCAdapter
