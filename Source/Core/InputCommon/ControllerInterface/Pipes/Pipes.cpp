// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Pipes/Pipes.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::Pipes
{
static constexpr std::array s_button_tokens = {"A", "B", "X",    "Y",      "Z",      "START",
                                               "L", "R", "D_UP", "D_DOWN", "D_LEFT", "D_RIGHT"};

static constexpr std::array s_shoulder_tokens{"L", "R"};

static constexpr std::array s_axis_tokens{"MAIN", "C"};

static double StringToDouble(std::string text)
{
  double result = 0;
  TryParse(text, &result);
  return result;
}

class PipeDevice : public Core::Device
{
public:
  PipeDevice(int fd, std::string name);
  ~PipeDevice();

  Core::DeviceRemoval UpdateInput() override;
  std::string GetName() const override { return m_name; }
  std::string GetSource() const override { return "Pipe"; }

private:
  class ButtonInput : public Input
  {
  public:
    ButtonInput(std::string_view name, const bool* value) : m_name(std::move(name)), m_state(*value)
    {
    }
    std::string GetName() const override { return "Button " + std::string{m_name}; }
    ControlState GetState() const override { return m_state; }

  private:
    const std::string_view m_name;
    const bool& m_state;
  };

  template <int Range>
  class AnalogInput : public Input
  {
  public:
    AnalogInput(std::string name, const ControlState* value)
        : m_name(std::move(name)), m_state(*value)
    {
    }
    std::string GetName() const override { return "Axis " + m_name + (Range < 0 ? " -" : " +"); }
    ControlState GetState() const override { return m_state / Range; }

  private:
    const std::string m_name;
    const ControlState& m_state;
  };

  void AddAxis(std::string name);
  void ParseCommand(const std::string& command);
  void SetAxis(const std::string& entry, double value);

  const int m_fd;
  const std::string m_name;
  std::string m_buf;
  std::map<std::string, bool> m_buttons;
  std::map<std::string, ControlState> m_axes;
};

class InputBackend final : public ciface::InputBackend
{
public:
  using ciface::InputBackend::InputBackend;
  void PopulateDevices() override;
};

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

void InputBackend::PopulateDevices()
{
  // Search the "Pipes" directory for fifo files that we can open in read-only,
  // non-blocking mode. The filename becomes the Device name.
  const std::string& dir_path = File::GetUserPath(D_PIPES_IDX);
  std::error_code error;
  for (const auto& dir_entry : std::filesystem::directory_iterator{dir_path, error})
  {
    if (!dir_entry.is_fifo())
      continue;

    const int fd = open(dir_entry.path().c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
      continue;

    GetControllerInterface().AddDevice(
        std::make_shared<PipeDevice>(fd, dir_entry.path().filename()));
  }

  if (error)
    NOTICE_LOG_FMT(CONTROLLERINTERFACE, "Pipes/PopulateDevices: {}", error.message());
}

PipeDevice::PipeDevice(int fd, std::string name) : m_fd(fd), m_name(std::move(name))
{
  for (const auto& tok : s_button_tokens)
  {
    auto [iter, inserted] = m_buttons.emplace(tok, false);
    AddInput(new ButtonInput(tok, &iter->second));
  }
  for (const auto& tok : s_shoulder_tokens)
  {
    AddAxis(tok);
  }
  for (const auto& tok : s_axis_tokens)
  {
    AddAxis(tok + std::string{" X"});
    AddAxis(tok + std::string{" Y"});
  }
}

PipeDevice::~PipeDevice()
{
  close(m_fd);
}

Core::DeviceRemoval PipeDevice::UpdateInput()
{
  // Read any pending characters off the pipe. If we hit a newline,
  // then dequeue a command off the front of m_buf and parse it.
  char buf[32];
  ssize_t bytes_read = read(m_fd, buf, sizeof(buf));
  while (bytes_read > 0)
  {
    m_buf.append(buf, bytes_read);
    bytes_read = read(m_fd, buf, sizeof buf);
  }
  std::size_t newline = m_buf.find("\n");
  while (newline != std::string::npos)
  {
    std::string command = m_buf.substr(0, newline);
    ParseCommand(command);
    m_buf.erase(0, newline + 1);
    newline = m_buf.find("\n");
  }
  return Core::DeviceRemoval::Keep;
}

void PipeDevice::AddAxis(std::string name)
{
  auto [iter, inserted] = m_axes.emplace(name, 0.0);
  AddInput(new AnalogInput<-1>(name, &iter->second));
  AddInput(new AnalogInput<+1>(std::move(name), &iter->second));
}

void PipeDevice::SetAxis(const std::string& entry, double value)
{
  auto search = m_axes.find(entry);
  if (search != m_axes.end())
    search->second = std::clamp(value, 0.0, 1.0) * 2.0 - 1.0;
}

void PipeDevice::ParseCommand(const std::string& command)
{
  const std::vector<std::string> tokens = SplitString(command, ' ');
  if (tokens.size() < 2 || tokens.size() > 4)
    return;
  if (tokens[0] == "PRESS" || tokens[0] == "RELEASE")
  {
    auto search = m_buttons.find(tokens[1]);
    if (search != m_buttons.end())
      search->second = tokens[0] == "PRESS";
  }
  else if (tokens[0] == "SET")
  {
    if (tokens.size() == 3)
    {
      double value = StringToDouble(tokens[2]);
      SetAxis(tokens[1], (value + 1.0) / 2.0);
    }
    else if (tokens.size() == 4)
    {
      double x = StringToDouble(tokens[2]);
      double y = StringToDouble(tokens[3]);
      SetAxis(tokens[1] + " X", x);
      SetAxis(tokens[1] + " Y", y);
    }
  }
}
}  // namespace ciface::Pipes
