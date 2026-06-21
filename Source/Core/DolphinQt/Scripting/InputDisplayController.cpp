// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/InputDisplayController.h"

#include "Common/FileUtil.h"
#include "Scripting/ScriptingEngine.h"

namespace
{
std::string ScriptPath()
{
  return File::GetSysDirectory() + "InputDisplay/input_display_gc.py";
}
}  // namespace

InputDisplayController::InputDisplayController() = default;
InputDisplayController::~InputDisplayController() = default;

void InputDisplayController::Show()
{
  if (m_backend)
    return;
  m_backend = std::make_unique<Scripting::ScriptingBackend>(ScriptPath());
}

void InputDisplayController::Hide()
{
  m_backend.reset();
  m_dumping = false;
}

bool InputDisplayController::IsShown() const
{
  return m_backend != nullptr;
}

void InputDisplayController::StartDump()
{
  // Dumping needs a live overlay to capture.
  Show();
  m_dumping = true;
}

void InputDisplayController::StopDump()
{
  m_dumping = false;
}

bool InputDisplayController::IsDumping() const
{
  return m_dumping;
}
