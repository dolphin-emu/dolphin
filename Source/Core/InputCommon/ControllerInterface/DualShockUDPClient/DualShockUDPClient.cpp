// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <tuple>

#include <SFML/Network/SocketSelector.hpp>
#include <SFML/Network/UdpSocket.hpp>
#include <fmt/format.h>

#include "Common/Config/Config.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/Random.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Core/CoreTiming.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPProto.h"

namespace ciface::DualShockUDPClient
{
constexpr std::string_view DUALSHOCKUDP_SOURCE_NAME = "DSUClient";

namespace Settings
{
const Config::Info<std::string> SERVER_ADDRESS{
    {Config::System::DualShockUDPClient, "Server", "IPAddress"}, ""};
const Config::Info<int> SERVER_PORT{{Config::System::DualShockUDPClient, "Server", "Port"}, 0};
const Config::Info<std::string> SERVERS{{Config::System::DualShockUDPClient, "Server", "Entries"},
                                        ""};
const Config::Info<bool> SERVERS_ENABLED{{Config::System::DualShockUDPClient, "Server", "Enabled"},
                                         false};
}  // namespace Settings

// Clock type used for querying timeframes
using SteadyClock = std::chrono::steady_clock;

std::atomic<int> g_calibration_device_index = -1;
bool g_calibration_device_found = false;
s16 g_calibration_touch_x_min = std::numeric_limits<s16>::max();
s16 g_calibration_touch_y_min = std::numeric_limits<s16>::max();
s16 g_calibration_touch_x_max = std::numeric_limits<s16>::min();
s16 g_calibration_touch_y_max = std::numeric_limits<s16>::min();

constexpr ControlState TOUCH_SPEED = 0.0125;  // Just a value that seemed good

class UDPDevice final : public Core::Device
{
private:
  template <class T>
  class Button final : public Input
  {
  public:
    Button(const char* name, const T& buttons, T mask)
        : m_name(name), m_buttons(buttons), m_mask(mask)
    {
    }
    std::string GetName() const override { return m_name; }
    ControlState GetState() const override { return (m_buttons & m_mask) != 0; }

  private:
    const char* const m_name;
    const T& m_buttons;
    const T m_mask;
  };

  template <class T = ControlState>
  class AnalogInput : public Input
  {
  public:
    AnalogInput(const char* name, const T& input, ControlState range, ControlState offset = 0,
                std::string_view old_name = "")
        : m_name(name), m_input(input), m_range(range), m_offset(offset), m_old_name(old_name)
    {
    }
    std::string GetName() const final override { return m_name; }
    ControlState GetState() const final override
    {
      return (ControlState(m_input) + m_offset) / m_range;
    }
    bool IsMatchingName(std::string_view name) const override
    {
      if (Input::IsMatchingName(name))
        return true;

      return m_old_name == name;
    }

  private:
    const char* m_name;
    const T& m_input;
    const ControlState m_range;
    const ControlState m_offset;
    const std::string_view m_old_name;
  };

  class RelativeTouchInput final : public RelativeInput<s16>
  {
  public:
    RelativeTouchInput(const char* name, ControlState scale) : RelativeInput(scale), m_name(name) {}
    std::string GetName() const final override { return m_name; }

  private:
    const char* m_name;
  };

  class TouchInput final : public AnalogInput<>
  {
  public:
    using AnalogInput::AnalogInput;
    bool IsDetectable() const override { return false; }
  };

  class MotionInput final : public AnalogInput<float>
  {
  public:
    using AnalogInput::AnalogInput;
    bool IsDetectable() const override { return false; }
  };

  using AccelerometerInput = MotionInput;
  using GyroInput = MotionInput;

  class BatteryInput final : public Input
  {
  public:
    using BatteryState = Proto::DsBattery;

    BatteryInput(const BatteryState& battery) : m_battery(battery) {}

    std::string GetName() const override { return "Battery"; }

    ControlState GetState() const override
    {
      switch (m_battery)
      {
      case BatteryState::Charging:  // We don't actually know the battery level in this case
      case BatteryState::Charged:
        return BATTERY_INPUT_MAX_VALUE;
      default:
        return ControlState(m_battery) / ControlState(BatteryState::Full) * BATTERY_INPUT_MAX_VALUE;
      }
    }

    bool IsDetectable() const override { return false; }

  private:
    const BatteryState& m_battery;
  };

  // Names are based on DS4 for simplicity
  struct DeviceInputNames
  {
    DeviceInputNames(const char* up, const char* down, const char* left, const char* right,
                     const char* square, const char* cross, const char* circle,
                     const char* triangle, const char* l1, const char* r1, const char* l2,
                     const char* r2, const char* l3, const char* r3, const char* share,
                     const char* options, const char* ps)
        : m_up(up), m_down(down), m_left(left), m_right(right), m_square(square), m_cross(cross),
          m_circle(circle), m_triangle(triangle), m_l1(l1), m_r1(r1), m_l2(l2), m_r2(r2), m_l3(l3),
          m_r3(r3), m_share(share), m_options(options), m_ps(ps)
    {
    }
    const char* m_up;
    const char* m_down;
    const char* m_left;
    const char* m_right;
    const char* m_square;
    const char* m_cross;
    const char* m_circle;
    const char* m_triangle;
    const char* m_l1;
    const char* m_r1;
    const char* m_l2;
    const char* m_r2;
    const char* m_l3;
    const char* m_r3;
    const char* m_share;
    const char* m_options;
    const char* m_ps;
  };

  static constexpr std::string_view DS4_INPUT_NAME = "DS4";
  static constexpr std::string_view DUALSENSE_INPUT_NAME = "DualSense";
  static constexpr std::string_view SWITCH_INPUT_NAME = "Switch";
  static constexpr std::string_view GENERIC_INPUT_NAME = "Generic";

  // Sure it would be nice if they were filled in from a config but this is fine
  const std::map<std::string_view, const DeviceInputNames> g_input_names_by_device = {
      {DS4_INPUT_NAME,
       DeviceInputNames("Up", "Down", "Left", "Right", "Square", "Cross", "Circle", "Triangle",
                        "L1", "R1", "L2", "R2", "L3", "R3", "Share", "Options", "PS")},
      {DUALSENSE_INPUT_NAME,
       DeviceInputNames("Up", "Down", "Left", "Right", "Square", "Cross", "Circle", "Triangle",
                        "L1", "R1", "L2", "R2", "L3", "R3", "Create", "Options", "PS")},
      {SWITCH_INPUT_NAME,
       DeviceInputNames("Up", "Down", "Left", "Right", "Y", "B", "A", "X", "L", "R", "ZL", "ZR",
                        "Left Stick", "Right Stick", "-", "+", "Home")},
      // Mandatory. Assume orientation of D-Pad and "action" buttons but not of central buttons
      {GENERIC_INPUT_NAME,
       DeviceInputNames("D-Pad N", "D-Pad S", "D-Pad W", "D-Pad E", "Button W", "Button S",
                        "Button E", "Button N", "Left Bumper", "Right Bumper", "Left Trigger",
                        "Right Trigger", "Left Stick", "Right Stick", "Special Button 1",
                        "Special Button 2", "Special Button 3")}};

public:
  void UpdateInput() override;

  UDPDevice(std::string name, int index, std::string server_address, u16 server_port,
            std::string device_type = "", std::string calibration = "",
            bool for_calibration = false);

  std::string GetName() const final override;
  std::string GetSource() const final override;
  std::optional<int> GetPreferredId() const final override;
  // Always add these at the end, given their hotplug nature
  int GetSortPriority() const override { return -2; }

private:
  void ResetPadData();

  const std::string m_name;
  const int m_index;
  sf::UdpSocket m_socket;
  SteadyClock::time_point m_next_reregister = SteadyClock::time_point::min();
  SteadyClock::time_point m_last_update;
  Proto::MessageType::PadDataResponse m_pad_data;
  ControlState m_touch_x;
  ControlState m_touch_y;
  Proto::Touch m_prev_touches[u8(InputChannel::Max)];
  RelativeTouchInput* m_relative_touch_inputs[4];
  std::string m_server_address;
  u16 m_server_port;
  bool m_for_calibration;

  s16 m_touch_x_min;
  s16 m_touch_y_min;
  s16 m_touch_x_max;
  s16 m_touch_y_max;
};

using MathUtil::GRAVITY_ACCELERATION;
constexpr auto SERVER_REREGISTER_INTERVAL = std::chrono::seconds{1};
constexpr auto SERVER_LISTPORTS_INTERVAL = std::chrono::seconds{1};
constexpr auto THREAD_MAX_WAIT_INTERVAL = std::chrono::milliseconds{250};
constexpr auto SERVER_UNRESPONSIVE_INTERVAL = std::chrono::seconds{1};  // Can be 0
constexpr auto RESET_INPUT_INTERVAL = std::chrono::seconds{1};
constexpr u32 SERVER_ASKED_PADS = 4;

struct Server
{
  Server(std::string description, std::string address, u16 port, std::string device_type = "",
         std::string calibration = "")
      : m_description{std::move(description)}, m_address{std::move(address)}, m_port{port},
        m_device_type{std::move(device_type)}, m_calibration{std::move(calibration)}
  {
  }
  Server(const Server&) = delete;
  Server(Server&& other) noexcept
  {
    m_description = std::move(other.m_description);
    m_address = std::move(other.m_address);
    m_port = other.m_port;
    m_port_info = std::move(other.m_port_info);
  }

  Server& operator=(const Server&) = delete;
  Server& operator=(Server&&) = delete;

  ~Server() = default;

  std::string m_description;
  std::string m_address;
  u16 m_port;
  std::array<Proto::MessageType::PortInfo, Proto::PORT_COUNT> m_port_info;
  sf::UdpSocket m_socket;
  std::string m_device_type;
  std::string m_calibration;
  SteadyClock::time_point m_disconnect_time = SteadyClock::now();
};

static bool s_has_init;
static bool s_servers_enabled;
static std::vector<Server> s_servers;
static u32 s_client_uid;
static SteadyClock::time_point s_next_listports_time;
static std::thread s_hotplug_thread;
static Common::Flag s_hotplug_thread_running;

static bool IsSameController(const Proto::MessageType::PortInfo& a,
                             const Proto::MessageType::PortInfo& b)
{
  // compare everything but battery_status
  return std::tie(a.pad_id, a.pad_state, a.model, a.connection_type, a.pad_mac_address) ==
         std::tie(b.pad_id, b.pad_state, b.model, b.connection_type, b.pad_mac_address);
}

static void HotplugThreadFunc()
{
  Common::SetCurrentThreadName("DualShockUDPClient Hotplug Thread");
  INFO_LOG_FMT(CONTROLLERINTERFACE, "DualShockUDPClient hotplug thread started");
  Common::ScopeGuard thread_stop_guard{
      [] { INFO_LOG_FMT(CONTROLLERINTERFACE, "DualShockUDPClient hotplug thread stopped"); }};

  std::vector<bool> timed_out_servers(s_servers.size(), false);

  while (s_hotplug_thread_running.IsSet())
  {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    const auto now = SteadyClock::now();
    if (now >= s_next_listports_time)
    {
      s_next_listports_time = now + SERVER_LISTPORTS_INTERVAL;

      for (size_t i = 0; i < s_servers.size(); ++i)
      {
        auto& server = s_servers[i];
        Proto::Message<Proto::MessageType::ListPorts> msg(s_client_uid);
        auto& list_ports = msg.m_message;
        // We ask for x possible devices. We will receive a message for every connected device.
        list_ports.pad_request_count = SERVER_ASKED_PADS;
        list_ports.pad_ids = {0, 1, 2, 3};
        msg.Finish();
        if (server.m_socket.send(&list_ports, sizeof list_ports, server.m_address, server.m_port) !=
            sf::Socket::Status::Done)
        {
          ERROR_LOG_FMT(CONTROLLERINTERFACE, "DualShockUDPClient HotplugThreadFunc send failed");
        }
        timed_out_servers[i] = true;
      }
    }

    sf::SocketSelector selector;
    for (auto& server : s_servers)
    {
      selector.add(server.m_socket);
    }

    auto timeout = duration_cast<milliseconds>(s_next_listports_time - SteadyClock::now());

    // Receive controller port info within a time from our request.
    // Run this even if we sent no new requests, to disconnect devices,
    // sleep (wait) the thread and catch old responses.
    do
    {
      // Selector's wait treats a timeout of zero as infinite timeout, which we don't want,
      // but we also don't want risk waiting for the whole SERVER_LISTPORTS_INTERVAL and hang
      // the thead trying to close this one in case we received no answers.
      const auto current_timeout = std::max(std::min(timeout, THREAD_MAX_WAIT_INTERVAL), 1ms);
      timeout -= current_timeout;
      // This will return at the first answer
      if (selector.wait(sf::milliseconds(current_timeout.count())))
      {
        // Now check all the servers because we don't know which one(s) sent a reply
        for (size_t i = 0; i < s_servers.size(); ++i)
        {
          auto& server = s_servers[i];
          if (!selector.isReady(server.m_socket))
          {
            continue;
          }

          Proto::Message<Proto::MessageType::FromServer> msg;
          std::size_t received_bytes;
          sf::IpAddress sender;
          u16 port;
          if (server.m_socket.receive(&msg, sizeof(msg), received_bytes, sender, port) !=
              sf::Socket::Status::Done)
          {
            continue;
          }

          if (auto port_info = msg.CheckAndCastTo<Proto::MessageType::PortInfo>())
          {
            server.m_disconnect_time = SteadyClock::now() + SERVER_UNRESPONSIVE_INTERVAL;
            // We have receive at least one valid update, that's enough. This is needed to avoid
            // false positive when checking for disconnection in case our thread waited too long
            timed_out_servers[i] = false;

            const bool port_changed =
                !IsSameController(*port_info, server.m_port_info[port_info->pad_id]);
            if (port_changed)
            {
              server.m_port_info[port_info->pad_id] = *port_info;
              // Just remove and re-add all the devices for simplicity
              g_controller_interface.PlatformPopulateDevices([] { PopulateDevices(); });
            }
          }
        }
      }
      if (!s_hotplug_thread_running.IsSet())  // Avoid hanging the thread for too long
        return;
    } while (timeout > 0ms);

    // If we have failed to receive any information from the server (or even send it),
    // disconnect all devices from it (after enough time has elapsed, to avoid false positives).
    for (size_t i = 0; i < s_servers.size(); ++i)
    {
      auto& server = s_servers[i];
      if (timed_out_servers[i] && SteadyClock::now() >= server.m_disconnect_time)
      {
        bool any_connected = false;
        for (size_t port_index = 0; port_index < server.m_port_info.size(); port_index++)
        {
          any_connected = any_connected ||
                          server.m_port_info[port_index].pad_state == Proto::DsState::Connected;
          server.m_port_info[port_index] = {};
          server.m_port_info[port_index].pad_id = static_cast<u8>(port_index);
        }
        // We can't only remove devices added by this server as we wouldn't know which they are
        if (any_connected)
          g_controller_interface.PlatformPopulateDevices([] { PopulateDevices(); });
      }
    }
  }
}

static void StartHotplugThread()
{
  // Mark the thread as running.
  if (!s_hotplug_thread_running.TestAndSet())
  {
    // It was already running.
    return;
  }

  s_hotplug_thread = std::thread(HotplugThreadFunc);
}

static void StopHotplugThread()
{
  // Tell the hotplug thread to stop.
  if (!s_hotplug_thread_running.TestAndClear())
  {
    // It wasn't running, we're done.
    return;
  }

  s_hotplug_thread.join();

  for (auto& server : s_servers)
  {
    server.m_socket.unbind();  // interrupt blocking socket
  }
}

// Also just start
static void Restart()
{
  INFO_LOG_FMT(CONTROLLERINTERFACE, "DualShockUDPClient Restart");

  StopHotplugThread();

  for (auto& server : s_servers)
  {
    for (size_t port_index = 0; port_index < server.m_port_info.size(); port_index++)
    {
      server.m_port_info[port_index] = {};
      server.m_port_info[port_index].pad_id = static_cast<u8>(port_index);
    }
  }

  // Only removes devices as servers have been cleaned
  g_controller_interface.PlatformPopulateDevices([] { PopulateDevices(); });

  s_client_uid = Common::Random::GenerateValue<u32>();
  s_next_listports_time = SteadyClock::now();

  if (s_servers_enabled && !s_servers.empty())
    StartHotplugThread();
}

static void ConfigChanged()
{
  if (!s_has_init)
    return;

  const bool servers_enabled = Config::Get(Settings::SERVERS_ENABLED);
  const std::string servers_setting = Config::Get(Settings::SERVERS);

  std::string old_servers_setting;
  for (const auto& server : s_servers)
  {
    // These are only useful to make sure we won't restart the thread unless
    // servers have actually changed, and to keep some flexibility in the serialization
    std::string format_string = "{}:{}:{}";
    if (!server.m_calibration.empty())
      format_string += ":{}:({})";
    else if (!server.m_device_type.empty())
      format_string += ":{}";
    format_string += ";";
    old_servers_setting += fmt::format(format_string, server.m_description, server.m_address,
                                       server.m_port, server.m_device_type, server.m_calibration);
  }

  if (servers_enabled != s_servers_enabled || servers_setting != old_servers_setting ||
      g_calibration_device_index >= 0)
  {
    // Stop the thread before writing to s_servers
    StopHotplugThread();

    s_servers_enabled = servers_enabled;
    s_servers.clear();

    const auto server_details = SplitString(servers_setting, ';');
    for (const auto& server_detail : server_details)
    {
      const auto server_info = SplitString(server_detail, ':');
      if (server_info.size() < 3)
        continue;

      const std::string description = server_info[0];
      const std::string server_address = server_info[1];
      const auto port = std::stoi(server_info[2]);
      const std::string device_type = server_info.size() > 3 ? server_info[3] : "";
      std::string calibration = server_info.size() > 4 ? server_info[4] : "";
      // Remove '(' and ')'
      if (!calibration.empty())
        calibration.erase(0, 1);
      if (!calibration.empty())
        calibration.pop_back();
      if (port >= std::numeric_limits<u16>::max())
      {
        continue;
      }
      u16 server_port = static_cast<u16>(port);

      s_servers.emplace_back(description, server_address, server_port, device_type, calibration);
    }
    Restart();
  }
}

void Init()
{
  // Does not support multiple init calls
  s_has_init = true;

  // The following is added for backwards compatibility
  const auto server_address_setting = Config::Get(Settings::SERVER_ADDRESS);
  const auto server_port_setting = Config::Get(Settings::SERVER_PORT);

  if (!server_address_setting.empty() && server_port_setting != 0)
  {
    // We have added even more settings now but they don't have to be defined
    const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS,
                             servers_setting + fmt::format("{}:{}:{};", "DS4",
                                                           server_address_setting,
                                                           server_port_setting));
    Config::SetBase(Settings::SERVER_ADDRESS, "");
    Config::SetBase(Settings::SERVER_PORT, 0);
  }

  // It would be much better to unbind from this callback on DeInit but it's not possible as of now
  Config::AddConfigChangedCallback(ConfigChanged);
  ConfigChanged();  // Call it immediately to load settings
}

// This can be called by the host thread as well as the hotplug thread, concurrently.
// So use PlatformPopulateDevices().
// s_servers is already safe because it can only be modified when the DSU thread is not running,
// from the main thread
void PopulateDevices()
{
  INFO_LOG_FMT(CONTROLLERINTERFACE, "DualShockUDPClient PopulateDevices");

  // s_servers has already been updated so we can't use it to know which devices we removed,
  // also it's good to remove all of them before adding new ones so that their id will be set
  // correctly if they have the same name
  g_controller_interface.RemoveDevice(
      [](const auto* dev) { return dev->GetSource() == DUALSHOCKUDP_SOURCE_NAME; });

  int i = 0;
  // Users might have created more than one server on the same IP/Port.
  // Devices might end up being duplicated (if the server responds two all requests)
  // but they won't conflict.
  for (const auto& server : s_servers)
  {
    for (size_t port_index = 0; port_index < server.m_port_info.size(); port_index++)
    {
      const Proto::MessageType::PortInfo& port_info = server.m_port_info[port_index];
      if (port_info.pad_state != Proto::DsState::Connected)
        continue;

      // In case we create more than one device from a server, calibration should still work
      // but it will be unique to the server and not the pads
      g_controller_interface.AddDevice(std::make_shared<UDPDevice>(
          server.m_description, static_cast<int>(port_index), server.m_address, server.m_port,
          server.m_device_type, server.m_calibration, g_calibration_device_index == i));
    }
    ++i;
  }
}

void DeInit()
{
  StopHotplugThread();

  s_has_init = false;
  s_servers_enabled = false;
  s_servers.clear();
}

UDPDevice::UDPDevice(std::string name, int index, std::string server_address, u16 server_port,
                     std::string device_type, std::string calibration, bool for_calibration)
    : m_name{std::move(name)}, m_index{index}, m_server_address{std::move(server_address)},
      m_server_port{server_port}, m_for_calibration(for_calibration)
{
  // We didn't have this setting before, so just default to DS4 as all the mapping would have been
  // added as that
  bool backwards_compatibility = false;
  if (device_type.empty())
  {
    device_type = DS4_INPUT_NAME;
    backwards_compatibility = true;
  }
  // Start from the default DS4 calibration (confirmed). Given that the DSU protocol offers no max
  // for touch, other non DS4 implementation could have a different range.
  m_touch_x_min = 0;
  m_touch_y_min = 0;
  m_touch_x_max = 1919;
  m_touch_y_max = (device_type == DUALSENSE_INPUT_NAME) ? 1079 : 941;
  const auto server_info = SplitString(calibration, ',');
  if (server_info.size() >= 4)
  {
    m_touch_x_min = std::stoi(server_info[0]);
    m_touch_y_min = std::stoi(server_info[1]);
    m_touch_x_max = std::stoi(server_info[2]);
    m_touch_y_max = std::stoi(server_info[3]);
  }

  m_socket.setBlocking(false);

  ResetPadData();

  m_last_update = SteadyClock::now();

  const DeviceInputNames* input_names;
  auto it = g_input_names_by_device.find(device_type);
  if (it != g_input_names_by_device.end())
    input_names = &it->second;
  else  // Generic profile is always available
    input_names = &g_input_names_by_device.at(GENERIC_INPUT_NAME);

  // Old names weren't following actual DS4 naming, make sure current configs are not broken
  if (backwards_compatibility)
  {
    AddInput(
        new AnalogInput<u8>(input_names->m_up, m_pad_data.button_dpad_up_analog, 255, 0, "Pad N"));
    AddInput(new AnalogInput<u8>(input_names->m_down, m_pad_data.button_dpad_down_analog, 255, 0,
                                 "Pad S"));
    AddInput(new AnalogInput<u8>(input_names->m_left, m_pad_data.button_dpad_left_analog, 255, 0,
                                 "Pad W"));
    AddInput(new AnalogInput<u8>(input_names->m_right, m_pad_data.button_dpad_right_analog, 255, 0,
                                 "Pad E"));
  }
  else
  {
    AddInput(new AnalogInput<u8>(input_names->m_up, m_pad_data.button_dpad_up_analog, 255));
    AddInput(new AnalogInput<u8>(input_names->m_down, m_pad_data.button_dpad_down_analog, 255));
    AddInput(new AnalogInput<u8>(input_names->m_left, m_pad_data.button_dpad_left_analog, 255));
    AddInput(new AnalogInput<u8>(input_names->m_right, m_pad_data.button_dpad_right_analog, 255));
  }
  AddInput(new AnalogInput<u8>(input_names->m_square, m_pad_data.button_square_analog, 255));
  AddInput(new AnalogInput<u8>(input_names->m_cross, m_pad_data.button_cross_analog, 255));
  AddInput(new AnalogInput<u8>(input_names->m_circle, m_pad_data.button_circle_analog, 255));
  AddInput(new AnalogInput<u8>(input_names->m_triangle, m_pad_data.button_triangle_analog, 255));
  AddInput(new AnalogInput<u8>(input_names->m_l1, m_pad_data.button_l1_analog, 255));
  AddInput(new AnalogInput<u8>(input_names->m_r1, m_pad_data.button_r1_analog, 255));

  AddInput(new AnalogInput<u8>(input_names->m_l2, m_pad_data.trigger_l2, 255));
  AddInput(new AnalogInput<u8>(input_names->m_r2, m_pad_data.trigger_r2, 255));

  AddInput(new Button<u8>(input_names->m_l3, m_pad_data.button_states1, 0x2));
  AddInput(new Button<u8>(input_names->m_r3, m_pad_data.button_states1, 0x4));
  AddInput(new Button<u8>(input_names->m_share, m_pad_data.button_states1, 0x1));
  AddInput(new Button<u8>(input_names->m_options, m_pad_data.button_states1, 0x8));
  AddInput(new Button<u8>(input_names->m_ps, m_pad_data.button_ps, 0x1));
  AddInput(new Button<u8>("Touch Button", m_pad_data.button_touch, 0x1));

  AddInput(new AnalogInput<u8>("Left X-", m_pad_data.left_stick_x, -128, -128));
  AddInput(new AnalogInput<u8>("Left X+", m_pad_data.left_stick_x, 127, -128));
  AddInput(new AnalogInput<u8>("Left Y-", m_pad_data.left_stick_y_inverted, -128, -128));
  AddInput(new AnalogInput<u8>("Left Y+", m_pad_data.left_stick_y_inverted, 127, -128));
  AddInput(new AnalogInput<u8>("Right X-", m_pad_data.right_stick_x, -128, -128));
  AddInput(new AnalogInput<u8>("Right X+", m_pad_data.right_stick_x, 127, -128));
  AddInput(new AnalogInput<u8>("Right Y-", m_pad_data.right_stick_y_inverted, -128, -128));
  AddInput(new AnalogInput<u8>("Right Y+", m_pad_data.right_stick_y_inverted, 127, -128));

  m_relative_touch_inputs[0] = new RelativeTouchInput("Relative Touch X-", -TOUCH_SPEED);
  m_relative_touch_inputs[1] = new RelativeTouchInput("Relative Touch X+", TOUCH_SPEED);
  m_relative_touch_inputs[2] = new RelativeTouchInput("Relative Touch Y-", -TOUCH_SPEED);
  m_relative_touch_inputs[3] = new RelativeTouchInput("Relative Touch Y+", TOUCH_SPEED);
  AddInput(m_relative_touch_inputs[0]);
  AddInput(m_relative_touch_inputs[1]);
  AddInput(m_relative_touch_inputs[2]);
  AddInput(m_relative_touch_inputs[3]);

  ControlState touch_x_range = (m_touch_x_max - m_touch_x_min) / 2.0;
  ControlState touch_x_offset = -(m_touch_x_min + touch_x_range);
  ControlState touch_y_range = (m_touch_y_max - m_touch_y_min) / 2.0;
  ControlState touch_y_offset = -(m_touch_y_min + touch_y_range);
  AddInput(new TouchInput("Touch X-", m_touch_x, -touch_x_range, touch_x_offset));
  AddInput(new TouchInput("Touch X+", m_touch_x, touch_x_range, touch_x_offset));
  AddInput(new TouchInput("Touch Y-", m_touch_y, -touch_y_range, touch_y_offset));
  AddInput(new TouchInput("Touch Y+", m_touch_y, touch_y_range, touch_y_offset));

  // Convert Gs to meters per second squared
  constexpr auto accel_scale = 1.0 / GRAVITY_ACCELERATION;

  AddInput(new AccelerometerInput("Accel Up", m_pad_data.accelerometer_y_g, -accel_scale));
  AddInput(new AccelerometerInput("Accel Down", m_pad_data.accelerometer_y_g, accel_scale));
  AddInput(new AccelerometerInput("Accel Left", m_pad_data.accelerometer_x_g, accel_scale));
  AddInput(new AccelerometerInput("Accel Right", m_pad_data.accelerometer_x_g, -accel_scale));
  AddInput(new AccelerometerInput("Accel Forward", m_pad_data.accelerometer_z_g, accel_scale));
  AddInput(new AccelerometerInput("Accel Backward", m_pad_data.accelerometer_z_g, -accel_scale));

  // Convert degrees per second to radians per second
  constexpr auto gyro_scale = 360.0 / MathUtil::TAU;

  AddInput(new GyroInput("Gyro Pitch Up", m_pad_data.gyro_pitch_deg_s, gyro_scale));
  AddInput(new GyroInput("Gyro Pitch Down", m_pad_data.gyro_pitch_deg_s, -gyro_scale));
  AddInput(new GyroInput("Gyro Roll Left", m_pad_data.gyro_roll_deg_s, -gyro_scale));
  AddInput(new GyroInput("Gyro Roll Right", m_pad_data.gyro_roll_deg_s, gyro_scale));
  AddInput(new GyroInput("Gyro Yaw Left", m_pad_data.gyro_yaw_deg_s, -gyro_scale));
  AddInput(new GyroInput("Gyro Yaw Right", m_pad_data.gyro_yaw_deg_s, gyro_scale));

  AddInput(new BatteryInput(m_pad_data.battery_status));
}

void UDPDevice::ResetPadData()
{
  for (size_t i = 0; i < std::size(m_prev_touches); ++i)
  {
    m_prev_touches[i].active = false;
    m_prev_touches[i].id = 0;
  }

  m_pad_data = Proto::MessageType::PadDataResponse{};

  // Make sure they start from resting values, not from 0
  m_touch_x = m_touch_x_min + ((m_touch_x_max - m_touch_x_min) / 2.0);
  m_touch_y = m_touch_y_min + ((m_touch_y_max - m_touch_y_min) / 2.0);
  m_pad_data.left_stick_x = 128;
  m_pad_data.left_stick_y_inverted = 128;
  m_pad_data.right_stick_x = 128;
  m_pad_data.right_stick_y_inverted = 128;
  m_pad_data.touch1.x = m_touch_x;
  m_pad_data.touch1.y = m_touch_y;
}

std::string UDPDevice::GetName() const
{
  return m_name;
}

std::string UDPDevice::GetSource() const
{
  return std::string(DUALSHOCKUDP_SOURCE_NAME);
}

void UDPDevice::UpdateInput()
{
  // Regularly tell the UDP server to feed us controller data
  const auto now = SteadyClock::now();
  if (now >= m_next_reregister)
  {
    m_next_reregister = now + SERVER_REREGISTER_INTERVAL;

    Proto::Message<Proto::MessageType::PadDataRequest> msg(s_client_uid);
    auto& data_req = msg.m_message;
    data_req.register_flags = Proto::RegisterFlags::PadID;
    data_req.pad_id_to_register = m_index;
    msg.Finish();
    if (m_socket.send(&data_req, sizeof(data_req), m_server_address, m_server_port) !=
        sf::Socket::Status::Done)
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "DualShockUDPClient UpdateInput send failed");
    }
  }

  // Receive and handle controller data
  Proto::Message<Proto::MessageType::FromServer> msg;
  std::size_t received_bytes;
  sf::IpAddress sender;
  u16 port;
  std::vector<Proto::MessageType::PadDataResponse> pad_datas;
  while (m_socket.receive(&msg, sizeof msg, received_bytes, sender, port) ==
         sf::Socket::Status::Done)
  {
    if (auto pad_data = msg.CheckAndCastTo<Proto::MessageType::PadDataResponse>())
    {
      pad_datas.push_back(*pad_data);
    }
  }

  // Given that packages are not synchronized to our updates one input channel
  // might receive all the new UDP packages while other channels get none, so we always update
  // the state to the latest received state. Precision losses should be very minor though
  // changes might be inconsistent.
  // The only way to make this work would be to make and sum up delta from all the input channels
  // and then commit the current sum of deltas as state in the update of your input channel.
  const size_t pad_datas_original_size = pad_datas.size();
  if (pad_datas_original_size > 0)
  {
    m_pad_data = pad_datas.back();
    // This acts as "last touch" as we don't check pad_data.touch1.active,
    // so the value persists after we stopped touching.
    // This is because we likely want to use this to control the wiimote cursor,
    // not as an analog stick, so we don't want the value to reset, also it's very easy for
    // your fingers to touch the borders and accidentally end the touch.
    m_touch_x = m_pad_data.touch1.x;
    m_touch_y = m_pad_data.touch1.y;

    if (m_for_calibration)
    {
      g_calibration_device_found = true;  // We don't set this as false on disconnect but it's fine
      if (m_pad_data.touch1.active)
      {
        g_calibration_touch_x_min = std::min(m_pad_data.touch1.x, g_calibration_touch_x_min);
        g_calibration_touch_y_min = std::min(m_pad_data.touch1.y, g_calibration_touch_y_min);
        g_calibration_touch_x_max = std::max(m_pad_data.touch1.x, g_calibration_touch_x_max);
        g_calibration_touch_y_max = std::max(m_pad_data.touch1.y, g_calibration_touch_y_max);
      }
    }
  }
  else
  {
    pad_datas.push_back(m_pad_data);
  }

  const u8 input_channel = u8(ControllerInterface::GetCurrentInputChannel());

  for (size_t i = 0; i < pad_datas.size(); ++i)
  {
    const Proto::MessageType::PadDataResponse& pad_data = pad_datas[i];

    if (pad_data.touch1.id != m_prev_touches[input_channel].id ||
        !m_prev_touches[input_channel].active)
    {
      for (size_t k = 0; k < std::size(m_relative_touch_inputs); ++k)
        m_relative_touch_inputs[k]->ResetState();
    }
    // Skip this update if the next touch has the same id, otherwise we'd lose the total deltas.
    // Note that if we had more different touch ids within the same update, we'd lose all deltas
    // except the last one.
    else if (i + 1 < pad_datas.size())
    {
      const Proto::MessageType::PadDataResponse& next_pad_data = pad_datas[i + 1];
      if (pad_data.touch1.id == next_pad_data.touch1.id)
        continue;
    }
    m_relative_touch_inputs[0]->UpdateState(pad_data.touch1.x);
    m_relative_touch_inputs[1]->UpdateState(pad_data.touch1.x);
    m_relative_touch_inputs[2]->UpdateState(pad_data.touch1.y);
    m_relative_touch_inputs[3]->UpdateState(pad_data.touch1.y);

    m_prev_touches[input_channel] = pad_data.touch1;
  }

  if (pad_datas_original_size == 0)
    pad_datas.clear();

  // If we haven't received an update in the last x seconds, reset inputs
  if (pad_datas.size() == 0 && (now - m_last_update) >= RESET_INPUT_INTERVAL)
  {
    ResetPadData();

    for (size_t i = 0; i < std::size(m_relative_touch_inputs); ++i)
      m_relative_touch_inputs[i]->ResetAllStates();

    m_last_update = now;  // Don't re-reset immediately
  }
  else if (pad_datas.size() > 0)
  {
    m_last_update = now;
  }
}

std::optional<int> UDPDevice::GetPreferredId() const
{
  return m_index;
}

}  // namespace ciface::DualShockUDPClient
