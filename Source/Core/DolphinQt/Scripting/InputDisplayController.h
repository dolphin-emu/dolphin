// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace Scripting
{
class ScriptingBackend;
}

// Drives the bundled input-display script through its own backend, decoupled from the Scripts window.
class InputDisplayController
{
public:
  InputDisplayController();
  ~InputDisplayController();

  void Show();
  void Hide();
  bool IsShown() const;

  void StartDump();
  void StopDump();
  bool IsDumping() const;

private:
  std::unique_ptr<Scripting::ScriptingBackend> m_backend;
  bool m_dumping = false;
};
