// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"

#include <chrono>
#include <cstring>

#include <SFML/Network/SocketSelector.hpp>
#include <SFML/Network/UdpSocket.hpp>

#include "Common/Config/Config.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/Matrix.h"
#include "Common/Random.h"
#include "Common/Thread.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPProto.h"

namespace ciface::DualShockUDPClient
{
class Device : public Core::Device
{
private:
  template <class T>
  class Button : public Core::Device::Input
  {
  public:
    Button(std::string name, const T& buttons, unsigned mask)
        : m_name(std::move(name)), m_buttons(buttons), m_mask(mask)
    {
    }
    std::string GetName() const override { return m_name; }
    ControlState GetState() const override { return (m_buttons & m_mask) != 0; }

  private:
    const std::string m_name;
    const T& m_buttons;
    unsigned m_mask;
  };

  template <class T>
  class AnalogInput : public Core::Device::Input
  {
  public:
    AnalogInput(std::string name, const T& input, ControlState range, ControlState offset = 0)
        : m_name(std::move(name)), m_input(input), m_range(range), m_offset(offset)
    {
    }
    std::string GetName() const override { return m_name; }
    ControlState GetState() const override { return (ControlState(m_input) + m_offset) / m_range; }

  private:
    const std::string m_name;
    const T& m_input;
    const ControlState m_range;
    const ControlState m_offset;
  };

  class TouchInput : public AnalogInput<int>
  {
  public:
    TouchInput(std::string name, const int& input, ControlState range)
        : AnalogInput(std::move(name), input, range)
    {
    }
    bool IsDetectable() override { return false; }
  };

  class AccelerometerInput : public AnalogInput<double>
  {
  public:
    AccelerometerInput(std::string name, const double& input, ControlState range)
        : AnalogInput(std::move(name), input, range)
    {
    }
    bool IsDetectable() override { return false; }
  };

  using GyroInput = AccelerometerInput;

public:
  void UpdateInput() override;

  Device(Proto::DsModel model, int index);

  std::string GetName() const final override;
  std::string GetSource() const final override;
  std::optional<int> GetPreferredId() const final override;

private:
  const Proto::DsModel m_model;
  const int m_index;
  u32 m_client_uid;
  sf::UdpSocket m_socket;
  Common::DVec3 m_accel;
  Common::DVec3 m_gyro;
  std::chrono::steady_clock::time_point m_next_reregister;
  Proto::MessageType::PadDataResponse m_pad_data;
  Proto::Touch m_prev_touch;
  bool m_prev_touch_valid;
  int m_touch_x;
  int m_touch_y;
};

using MathUtil::GRAVITY_ACCELERATION;
static constexpr char DEFAULT_SERVER_ADDRESS[] = "127.0.0.1";
static constexpr u16 DEFAULT_SERVER_PORT = 26760;
static constexpr auto SERVER_REREGISTER_INTERVAL = std::chrono::seconds{1};
static constexpr auto SERVER_LISTPORTS_INTERVAL = std::chrono::seconds{1};
static constexpr int TOUCH_X_AXIS_MAX = 1000;
static constexpr int TOUCH_Y_AXIS_MAX = 500;

namespace Settings
{
const Config::ConfigInfo<bool> SERVER_ENABLED{
    {Config::System::DualShockUDPClient, "Server", "Enabled"}, false};
const Config::ConfigInfo<std::string> SERVER_ADDRESS{
    {Config::System::DualShockUDPClient, "Server", "IPAddress"}, DEFAULT_SERVER_ADDRESS};
const Config::ConfigInfo<int> SERVER_PORT{{Config::System::DualShockUDPClient, "Server", "Port"},
                                          DEFAULT_SERVER_PORT};
}  // namespace Settings

static bool s_server_enabled;
static std::string s_server_address;
static u16 s_server_port;
static u32 s_client_uid;
static std::chrono::steady_clock::time_point s_next_listports;
static std::thread s_hotplug_thread;
static Common::Flag s_hotplug_thread_running;
static std::mutex s_port_info_mutex;
static Proto::MessageType::PortInfo s_port_info[Proto::PORT_COUNT];
static sf::UdpSocket s_socket;

static bool IsSameController(const Proto::MessageType::PortInfo& a,
                             const Proto::MessageType::PortInfo& b)
{
  // compare everything but battery_status
  return a.pad_id == b.pad_id && a.pad_state == b.pad_state && a.model == b.model &&
         a.connection_type == b.connection_type &&
         memcmp(a.pad_mac_address, b.pad_mac_address, sizeof a.pad_mac_address) == 0;
}

static sf::Socket::Status ReceiveWithTimeout(sf::UdpSocket& socket, void* data, std::size_t size,
                                             std::size_t& received, sf::IpAddress& remoteAddress,
                                             unsigned short& remotePort, sf::Time timeout)
{
  sf::SocketSelector selector;
  selector.add(socket);
  if (selector.wait(timeout))
    return socket.receive(data, size, received, remoteAddress, remotePort);
  else
    return sf::Socket::NotReady;
}

static void HotplugThreadFunc()
{
  Common::SetCurrentThreadName("DualShockUDPClient Hotplug Thread");
  NOTICE_LOG(SERIALINTERFACE, "DualShockUDPClient hotplug thread started");

  while (s_hotplug_thread_running.IsSet())
  {
    const auto now = std::chrono::steady_clock::now();
    if (now >= s_next_listports)
    {
      s_next_listports = now + SERVER_LISTPORTS_INTERVAL;

      // Request info on the four controller ports
      Proto::Message<Proto::MessageType::ListPorts> msg(s_client_uid);
      auto& list_ports = msg.m_message;
      list_ports.pad_request_count = 4;
      list_ports.pad_id[0] = 0;
      list_ports.pad_id[1] = 1;
      list_ports.pad_id[2] = 2;
      list_ports.pad_id[3] = 3;
      msg.Finish();
      if (s_socket.send(&list_ports, sizeof list_ports, s_server_address, s_server_port) !=
          sf::Socket::Status::Done)
        ERROR_LOG(SERIALINTERFACE, "DualShockUDPClient HotplugThreadFunc send failed");
    }

    // Receive controller port info
    Proto::Message<Proto::MessageType::FromServer> msg;
    const auto timeout = s_next_listports - std::chrono::steady_clock::now();
    // ReceiveWithTimeout treats a timeout of zero as infinite timeout, which we don't want
    auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
    timeout_ms = std::max<decltype(timeout_ms)>(timeout_ms, 1);
    std::size_t received_bytes;
    sf::IpAddress sender;
    u16 port;
    if (ReceiveWithTimeout(s_socket, &msg, sizeof(msg), received_bytes, sender, port,
                           sf::milliseconds(timeout_ms)) == sf::Socket::Status::Done)
    {
      if (auto port_info = msg.CheckAndCastTo<Proto::MessageType::PortInfo>())
      {
        const bool port_changed = !IsSameController(*port_info, s_port_info[port_info->pad_id]);
        {
          std::lock_guard<std::mutex> lock(s_port_info_mutex);
          s_port_info[port_info->pad_id] = *port_info;
        }
        if (port_changed)
          PopulateDevices();
      }
    }
  }
  NOTICE_LOG(SERIALINTERFACE, "DualShockUDPClient hotplug thread stopped");
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

  s_socket.unbind();  // interrupt blocking socket
  s_hotplug_thread.join();
}

static void Restart()
{
  NOTICE_LOG(SERIALINTERFACE, "DualShockUDPClient Restart");

  StopHotplugThread();

  s_client_uid = Common::Random::GenerateValue<u32>();
  s_next_listports = std::chrono::steady_clock::time_point::min();
  for (int port_index = 0; port_index < Proto::PORT_COUNT; port_index++)
  {
    s_port_info[port_index] = {};
    s_port_info[port_index].pad_id = port_index;
  }

  PopulateDevices();  // remove devices

  if (s_server_enabled)
    StartHotplugThread();
}

static void ConfigChanged()
{
  bool server_enabled = Config::Get(Settings::SERVER_ENABLED);
  std::string server_address = Config::Get(Settings::SERVER_ADDRESS);
  u16 server_port = Config::Get(Settings::SERVER_PORT);
  if (server_enabled != s_server_enabled || server_address != s_server_address ||
      server_port != s_server_port)
  {
    s_server_enabled = server_enabled;
    s_server_address = server_address;
    s_server_port = server_port;
    Restart();
  }
}

void Init()
{
  Config::AddConfigChangedCallback(ConfigChanged);
}

void PopulateDevices()
{
  NOTICE_LOG(SERIALINTERFACE, "DualShockUDPClient PopulateDevices");

  g_controller_interface.RemoveDevice(
      [](const auto* dev) { return dev->GetSource() == "DSUClient"; });

  std::lock_guard<std::mutex> lock(s_port_info_mutex);
  for (int port_index = 0; port_index < Proto::PORT_COUNT; port_index++)
  {
    Proto::MessageType::PortInfo port_info = s_port_info[port_index];
    if (port_info.pad_state == Proto::DsState::Connected)
      g_controller_interface.AddDevice(std::make_shared<Device>(port_info.model, port_index));
  }
}

void DeInit()
{
  StopHotplugThread();
}

Device::Device(Proto::DsModel model, int index)
    : m_model(model), m_index(index),
      m_client_uid(Common::Random::GenerateValue<u32>()), m_accel{}, m_gyro{},
      m_next_reregister(std::chrono::steady_clock::time_point::min()), m_pad_data{}, m_prev_touch{},
      m_prev_touch_valid(false), m_touch_x(0), m_touch_y(0)
{
  m_socket.setBlocking(false);

  AddInput(new AnalogInput<u8>("Pad W", m_pad_data.button_dpad_left_analog, 255));
  AddInput(new AnalogInput<u8>("Pad S", m_pad_data.button_dpad_down_analog, 255));
  AddInput(new AnalogInput<u8>("Pad E", m_pad_data.button_dpad_right_analog, 255));
  AddInput(new AnalogInput<u8>("Pad N", m_pad_data.button_dpad_up_analog, 255));
  AddInput(new AnalogInput<u8>("Square", m_pad_data.button_square_analog, 255));
  AddInput(new AnalogInput<u8>("Cross", m_pad_data.button_cross_analog, 255));
  AddInput(new AnalogInput<u8>("Circle", m_pad_data.button_circle_analog, 255));
  AddInput(new AnalogInput<u8>("Triangle", m_pad_data.button_triangle_analog, 255));
  AddInput(new AnalogInput<u8>("L1", m_pad_data.button_l1_analog, 255));
  AddInput(new AnalogInput<u8>("R1", m_pad_data.button_r1_analog, 255));

  AddInput(new AnalogInput<u8>("L2", m_pad_data.trigger_l2, 255));
  AddInput(new AnalogInput<u8>("R2", m_pad_data.trigger_r2, 255));

  AddInput(new Button<u8>("L3", m_pad_data.button_states1, 0x2));
  AddInput(new Button<u8>("R3", m_pad_data.button_states1, 0x4));
  AddInput(new Button<u8>("Share", m_pad_data.button_states1, 0x1));
  AddInput(new Button<u8>("Options", m_pad_data.button_states1, 0x8));
  AddInput(new Button<u8>("PS", m_pad_data.button_ps, 0x1));
  AddInput(new Button<u8>("Touch Button", m_pad_data.button_touch, 0x1));

  AddInput(new AnalogInput<u8>("Left X-", m_pad_data.left_stick_x, -128, -128));
  AddInput(new AnalogInput<u8>("Left X+", m_pad_data.left_stick_x, 127, -128));
  AddInput(new AnalogInput<u8>("Left Y-", m_pad_data.left_stick_y_inverted, -128, -128));
  AddInput(new AnalogInput<u8>("Left Y+", m_pad_data.left_stick_y_inverted, 127, -128));
  AddInput(new AnalogInput<u8>("Right X-", m_pad_data.right_stick_x, -128, -128));
  AddInput(new AnalogInput<u8>("Right X+", m_pad_data.right_stick_x, 127, -128));
  AddInput(new AnalogInput<u8>("Right Y-", m_pad_data.right_stick_y_inverted, -128, -128));
  AddInput(new AnalogInput<u8>("Right Y+", m_pad_data.right_stick_y_inverted, 127, -128));

  AddInput(new TouchInput("Touch X-", m_touch_x, -TOUCH_X_AXIS_MAX));
  AddInput(new TouchInput("Touch X+", m_touch_x, TOUCH_X_AXIS_MAX));
  AddInput(new TouchInput("Touch Y-", m_touch_y, -TOUCH_Y_AXIS_MAX));
  AddInput(new TouchInput("Touch Y+", m_touch_y, TOUCH_Y_AXIS_MAX));

  AddInput(new AccelerometerInput("Accel Left", m_accel.x, 1));
  AddInput(new AccelerometerInput("Accel Right", m_accel.x, -1));
  AddInput(new AccelerometerInput("Accel Backward", m_accel.y, 1));
  AddInput(new AccelerometerInput("Accel Forward", m_accel.y, -1));
  AddInput(new AccelerometerInput("Accel Up", m_accel.z, 1));
  AddInput(new AccelerometerInput("Accel Down", m_accel.z, -1));

  AddInput(new GyroInput("Gyro Pitch Up", m_gyro.x, -1));
  AddInput(new GyroInput("Gyro Pitch Down", m_gyro.x, 1));
  AddInput(new GyroInput("Gyro Roll Right", m_gyro.y, -1));
  AddInput(new GyroInput("Gyro Roll Left", m_gyro.y, 1));
  AddInput(new GyroInput("Gyro Yaw Right", m_gyro.z, -1));
  AddInput(new GyroInput("Gyro Yaw Left", m_gyro.z, 1));
}

std::string Device::GetName() const
{
  switch (m_model)
  {
  case Proto::DsModel::None:
    return "None";
  case Proto::DsModel::DS3:
    return "DualShock 3";
  case Proto::DsModel::DS4:
    return "DualShock 4";
  case Proto::DsModel::Generic:
    return "Generic Gamepad";
  default:
    return "Device";
  }
}

std::string Device::GetSource() const
{
  return "DSUClient";
}

void Device::UpdateInput()
{
  // Regularly tell the UDP server to feed us controller data
  const auto now = std::chrono::steady_clock::now();
  if (now >= m_next_reregister)
  {
    m_next_reregister = now + SERVER_REREGISTER_INTERVAL;

    Proto::Message<Proto::MessageType::PadDataRequest> msg(m_client_uid);
    auto& data_req = msg.m_message;
    data_req.register_flags = Proto::RegisterFlags::PadID;
    data_req.pad_id_to_register = m_index;
    msg.Finish();
    if (m_socket.send(&data_req, sizeof(data_req), s_server_address, s_server_port) !=
        sf::Socket::Status::Done)
      ERROR_LOG(SERIALINTERFACE, "DualShockUDPClient UpdateInput send failed");
  }

  // Receive and handle controller data
  Proto::Message<Proto::MessageType::FromServer> msg;
  std::size_t received_bytes;
  sf::IpAddress sender;
  u16 port;
  while (m_socket.receive(&msg, sizeof msg, received_bytes, sender, port) ==
         sf::Socket::Status::Done)
  {
    if (auto pad_data = msg.CheckAndCastTo<Proto::MessageType::PadDataResponse>())
    {
      m_pad_data = *pad_data;

      m_accel.x = m_pad_data.accelerometer_x_g;
      m_accel.z = -m_pad_data.accelerometer_y_g;
      m_accel.y = -m_pad_data.accelerometer_z_inverted_g;
      m_gyro.x = -m_pad_data.gyro_pitch_deg_s;
      m_gyro.z = -m_pad_data.gyro_yaw_deg_s;
      m_gyro.y = -m_pad_data.gyro_roll_deg_s;

      // Convert Gs to meters per second squared
      m_accel = m_accel * GRAVITY_ACCELERATION;

      // Convert degrees per second to radians per second
      m_gyro = m_gyro * (MathUtil::TAU / 360);

      // Update touch pad relative coordinates
      if (m_pad_data.touch1.id != m_prev_touch.id)
        m_prev_touch_valid = false;
      if (m_prev_touch_valid)
      {
        m_touch_x += m_pad_data.touch1.x - m_prev_touch.x;
        m_touch_y += m_pad_data.touch1.y - m_prev_touch.y;
        m_touch_x = std::clamp(m_touch_x, -TOUCH_X_AXIS_MAX, TOUCH_X_AXIS_MAX);
        m_touch_y = std::clamp(m_touch_y, -TOUCH_Y_AXIS_MAX, TOUCH_Y_AXIS_MAX);
      }
      m_prev_touch = m_pad_data.touch1;
      m_prev_touch_valid = true;
    }
  }
}

std::optional<int> Device::GetPreferredId() const
{
  return m_index;
}

}  // namespace ciface::DualShockUDPClient
