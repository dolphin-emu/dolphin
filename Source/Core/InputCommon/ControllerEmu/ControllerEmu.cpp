// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControllerEmu.h"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "Common/IniFile.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ControllerEmu
{
// This should theoretically be per EmulatedController instance,
// though no EmulatedController usually run in parallel, so it makes little difference
static std::recursive_mutex s_get_state_mutex;

std::string ControlGroupContainer::GetDisplayName() const
{
  return GetName();
}

EmulatedController::~EmulatedController() = default;

// This should be called before calling GetState() or State() on a control reference
// to prevent a race condition.
// This is a recursive mutex because UpdateReferences is recursive.
std::unique_lock<std::recursive_mutex> EmulatedController::GetStateLock()
{
  std::unique_lock<std::recursive_mutex> lock(s_get_state_mutex);
  return lock;
}

void EmulatedController::UpdateReferences(const ControllerInterface& devi)
{
  std::scoped_lock lk(s_get_state_mutex, devi.GetDevicesMutex());

  m_default_device_is_connected = devi.HasConnectedDevice(m_default_device);

  ciface::ExpressionParser::ControlEnvironment env(devi, GetDefaultDevice(), m_expression_vars);

  UpdateGroupsReferences(env);

  env.CleanUnusedVariables();
}

void ControlGroupContainer::UpdateGroupsReferences(
    ciface::ExpressionParser::ControlEnvironment& env)
{
  const auto lock = EmulatedController::GetStateLock();

  for (auto& group : groups)
    group->UpdateReferences(env);
}

void EmulatedController::UpdateSingleControlReference(const ControllerInterface& devi,
                                                      ControlReference* ref)
{
  ciface::ExpressionParser::ControlEnvironment env(devi, GetDefaultDevice(), m_expression_vars);

  const auto lock = GetStateLock();
  ref->UpdateReference(env);

  env.CleanUnusedVariables();
}

const ciface::ExpressionParser::ControlEnvironment::VariableContainer&
EmulatedController::GetExpressionVariables() const
{
  return m_expression_vars;
}

void EmulatedController::ResetExpressionVariables()
{
  for (auto& var : m_expression_vars)
  {
    if (var.second)
    {
      *var.second = 0;
    }
  }
}

bool EmulatedController::IsDefaultDeviceConnected() const
{
  return m_default_device_is_connected;
}

const ciface::Core::DeviceQualifier& EmulatedController::GetDefaultDevice() const
{
  return m_default_device;
}

void EmulatedController::SetDefaultDevice(const std::string& device)
{
  ciface::Core::DeviceQualifier devq;
  devq.FromString(device);
  SetDefaultDevice(std::move(devq));
}

void EmulatedController::SetDefaultDevice(ciface::Core::DeviceQualifier devq)
{
  m_default_device = std::move(devq);
}

ControlGroupContainer::~ControlGroupContainer() = default;

void EmulatedController::LoadConfig(Common::IniFile::Section* sec)
{
  const auto lock = EmulatedController::GetStateLock();

  std::string defdev;
  if (sec->Get("Device", &defdev, "") && !defdev.empty())
    SetDefaultDevice(defdev);

  LoadGroupsConfig(sec, "");
}

void ControlGroupContainer::LoadGroupsConfig(Common::IniFile::Section* sec, const std::string& base)
{
  for (auto& cg : groups)
    cg->LoadConfig(sec, base);
}

void EmulatedController::SaveConfig(Common::IniFile::Section* sec)
{
  const auto lock = EmulatedController::GetStateLock();

  sec->Set("Device", GetDefaultDevice().ToString(), "");

  SaveGroupsConfig(sec, "");
}

void ControlGroupContainer::SaveGroupsConfig(Common::IniFile::Section* sec, const std::string& base)
{
  for (auto& cg : groups)
    cg->SaveConfig(sec, base);
}

void EmulatedController::LoadDefaults(const ControllerInterface& ciface)
{
  const auto lock = GetStateLock();
  // load an empty inifile section, clears everything
  Common::IniFile::Section sec;
  LoadConfig(&sec);

  const std::string& default_device_string = ciface.GetDefaultDeviceString();
  if (!default_device_string.empty())
  {
    SetDefaultDevice(default_device_string);
  }
}

void ControlGroupContainer::SetInputOverrideFunction(InputOverrideFunction override_func)
{
  m_input_override_function = std::move(override_func);
}

void ControlGroupContainer::ClearInputOverrideFunction()
{
  m_input_override_function = {};
}

}  // namespace ControllerEmu
