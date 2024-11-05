// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::Pipes
{
#ifdef _WIN32
  typedef HANDLE PIPE_FD;
#else
  typedef int PIPE_FD;
#endif
// To create a piped controller input, create a named pipe in the
// Pipes directory and write commands out to it. Commands are separated
// by a newline character, with spaces separating command tokens.
// Command syntax is as follows, where curly brackets are one-of and square
// brackets are inclusive numeric ranges. Cases are sensitive. Numeric inputs
// are clamped to [0, 1] and otherwise invalid commands are discarded.
// {PRESS, RELEASE} {A, B, X, Y, Z, START, L, R, D_UP, D_DOWN, D_LEFT, D_RIGHT}
// SET {L, R} [0, 1]
// SET {MAIN, C} [0, 1] [0, 1]

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface);

class PipeDevice : public Core::Device
{
public:
  PipeDevice(PIPE_FD fd, const std::string& name);
  ~PipeDevice();

  Core::DeviceRemoval UpdateInput() override;
  std::string GetName() const override { return m_name; }
  std::string GetSource() const override { return "Pipe"; }

private:
  class PipeInput : public Input
  {
  public:
    PipeInput(const std::string& name) : m_name(name), m_state(0.0) {}
    std::string GetName() const override { return m_name; }
    ControlState GetState() const override { return m_state; }
    void SetState(ControlState state) { m_state = state; }

  private:
    const std::string m_name;
    ControlState m_state;
  };

  void AddAxis(const std::string& name, double value);
  bool ParseCommand(const std::string& command);
  void SetAxis(const std::string& entry, double value);
  s32 readFromPipe(PIPE_FD file_descriptor, char* in_buffer, size_t size);

  const PIPE_FD  m_fd;
  const std::string m_name;
  std::string m_buf;
  std::map<std::string, PipeInput*> m_buttons;
  std::map<std::string, PipeInput*> m_axes;
};
}  // namespace ciface::Pipes
