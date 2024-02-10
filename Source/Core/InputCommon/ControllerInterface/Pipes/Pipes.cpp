// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Pipes/Pipes.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <Core/Config/MainSettings.h>
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

// SLIPPITODO: Do we need to make this extern?
bool g_need_input_for_frame;

namespace ciface::Pipes
{
static const std::array<std::string, 12> s_button_tokens{
    {"A", "B", "X", "Y", "Z", "START", "L", "R", "D_UP", "D_DOWN", "D_LEFT", "D_RIGHT"}};

static const std::array<std::string, 2> s_shoulder_tokens{{"L", "R"}};

static const std::array<std::string, 2> s_axis_tokens{{"MAIN", "C"}};

static double StringToDouble(const std::string& text)
{
  std::istringstream is(text);
  // ignore current locale
  is.imbue(std::locale::classic());
  double result;
  is >> result;
  return result;
}

void PopulateDevices()
{
#ifdef _WIN32
  PIPE_FD pipes[4];
  // Windows has named pipes, but they're different. They don't exist on the
  //  local filesystem and are transient. So rather than searching the /Pipes
  //  directory for pipes, we just always assume there's 4 and then make them
  for (uint32_t i = 0; i < 4; i++)
  {
    std::string pipename = "\\\\.\\pipe\\slippibot" + std::to_string(i + 1);
    pipes[i] = CreateNamedPipeA(pipename.data(),               // pipe name
                                PIPE_ACCESS_INBOUND,           // read access, inward only
                                PIPE_TYPE_BYTE | PIPE_NOWAIT,  // byte mode, nonblocking
                                1,                             // number of clients
                                256,                           // output buffer size
                                256,                           // input buffer size
                                0,                             // timeout value
                                NULL                           // security attributes
    );

    // We're in nonblocking mode, so this won't wait for clients
    ConnectNamedPipe(pipes[i], NULL);
    std::string ui_pipe_name = "slippibot" + std::to_string(i + 1);
    g_controller_interface.AddDevice(std::make_shared<PipeDevice>(pipes[i], ui_pipe_name));
  }
#else
  // Search the Pipes directory for files that we can open in read-only,
  // non-blocking mode. The device name is the virtual name of the file.
  File::FSTEntry fst;
  std::string dir_path = File::GetUserPath(D_PIPES_IDX);
  if (!File::Exists(dir_path))
    return;
  fst = File::ScanDirectoryTree(dir_path, false);
  if (!fst.isDirectory)
    return;
  for (unsigned int i = 0; i < fst.size; ++i)
  {
    const File::FSTEntry& child = fst.children[i];
    if (child.isDirectory)
      continue;
    PIPE_FD fd = open(child.physicalName.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
      continue;
    g_controller_interface.AddDevice(std::make_shared<PipeDevice>(fd, child.virtualName));
  }
#endif
}

PipeDevice::PipeDevice(PIPE_FD fd, const std::string& name) : m_fd(fd), m_name(name)
{
  for (const auto& tok : s_button_tokens)
  {
    PipeInput* btn = new PipeInput("Button " + tok);
    AddInput(btn);
    m_buttons[tok] = btn;
  }
  for (const auto& tok : s_shoulder_tokens)
  {
    AddAxis(tok, 0.0);
  }
  for (const auto& tok : s_axis_tokens)
  {
    AddAxis(tok + " X", 0.5);
    AddAxis(tok + " Y", 0.5);
  }
}

PipeDevice::~PipeDevice()
{
#ifdef _WIN32
  CloseHandle(m_fd);
#else
  close(m_fd);
#endif
}

s32 PipeDevice::readFromPipe(PIPE_FD file_descriptor, char* in_buffer, size_t size)
{
#ifdef _WIN32

  u32 bytes_available = 0;
  DWORD bytesread = 0;
  bool peek_success =
      PeekNamedPipe(file_descriptor, NULL, 0, NULL, (LPDWORD)&bytes_available, NULL);

  if (!peek_success && (GetLastError() == ERROR_BROKEN_PIPE))
  {
    DisconnectNamedPipe(file_descriptor);
    ConnectNamedPipe(file_descriptor, NULL);
    return -1;
  }

  if (peek_success && (bytes_available > 0))
  {
    bool success = ReadFile(file_descriptor,                              // pipe handle
                            in_buffer,                                    // buffer to receive reply
                            (DWORD)std::min(bytes_available, (u32)size),  // size of buffer
                            &bytesread,                                   // number of bytes read
                            NULL);                                        // not overlapped
    if (!success)
    {
      return -1;
    }
  }
  return (s32)bytesread;
#ifndef _WIN32
  if (SConfig::GetInstance().m_blockingPipes)
  {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(file_descriptor, &set);

    // Wait for activity on the socket
    select(1, &set, NULL, NULL, NULL);
  }
#endif
#else
  return read(file_descriptor, in_buffer, size);
#endif
}

Core::DeviceRemoval PipeDevice::UpdateInput()
{
  bool finished = false;
  do
  {
    // Read any pending characters off the pipe. If we hit a newline,
    // then dequeue a command off the front of m_buf and parse it.
    char buf[32];
    s32 bytes_read = readFromPipe(m_fd, buf, sizeof buf);
    if (bytes_read == 0)
    {
      // Pipe died, so just quit out
      return;
    }
    while (bytes_read > 0)
    {
      m_buf.append(buf, bytes_read);
      bytes_read = readFromPipe(m_fd, buf, sizeof buf);
    }
    std::size_t newline = m_buf.find("\n");
    while (newline != std::string::npos)
    {
      std::string command = m_buf.substr(0, newline);
      ParseCommand(command);
      m_buf.erase(0, newline + 1);
      newline = m_buf.find("\n");
    }
  } while (!finished && Config::Get(Config::SLIPPI_BLOCKING_PIPES));
}

void PipeDevice::AddAxis(const std::string& name, double value)
{
  // Dolphin uses separate axes for left/right, which complicates things.
  PipeInput* ax_hi = new PipeInput("Axis " + name + " +");
  ax_hi->SetState(value);
  PipeInput* ax_lo = new PipeInput("Axis " + name + " -");
  ax_lo->SetState(value);
  m_axes[name + " +"] = ax_hi;
  m_axes[name + " -"] = ax_lo;
  AddAnalogInputs(ax_lo, ax_hi);
}

void PipeDevice::SetAxis(const std::string& entry, double value)
{
  value = std::clamp(value, 0.0, 1.0);
  double hi = std::max(0.0, value - 0.5) * 2.0;
  double lo = (0.5 - std::min(0.5, value)) * 2.0;
  auto search_hi = m_axes.find(entry + " +");
  if (search_hi != m_axes.end())
    search_hi->second->SetState(hi);
  auto search_lo = m_axes.find(entry + " -");
  if (search_lo != m_axes.end())
    search_lo->second->SetState(lo);
}

bool PipeDevice::ParseCommand(const std::string& command)
{
  const std::vector<std::string> tokens = SplitString(command, ' ');
  if (tokens.size() < 2 || tokens.size() > 4)
    return false;
  if (tokens[0] == "FLUSH")
  {
    g_need_input_for_frame = false;
    return true;
  }
  if (tokens[0] == "PRESS" || tokens[0] == "RELEASE")
  {
    auto search = m_buttons.find(tokens[1]);
    if (search != m_buttons.end())
      search->second->SetState(tokens[0] == "PRESS" ? 1.0 : 0.0);
  }
  else if (tokens[0] == "SET")
  {
    if (tokens.size() == 3)
    {
      double value = StringToDouble(tokens[2]);
      SetAxis(tokens[1], (value / 2.0) + 0.5);
    }
    else if (tokens.size() == 4)
    {
      double x = StringToDouble(tokens[2]);
      double y = StringToDouble(tokens[3]);
      SetAxis(tokens[1] + " X", x);
      SetAxis(tokens[1] + " Y", y);
    }
  }
  return false;
}
}  // namespace ciface::Pipes
