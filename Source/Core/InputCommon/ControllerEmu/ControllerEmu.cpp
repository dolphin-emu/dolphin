// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControllerEmu.h"

#include "Common/Logging/Log.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ControllerEmu
{
static std::recursive_mutex s_state_mutex;
#ifdef _DEBUG
static std::atomic<int> s_state_mutex_count;
#endif
static std::recursive_mutex s_devices_input_mutex;

std::string EmulatedController::GetDisplayName() const
{
  return GetName();
}

EmulatedController::~EmulatedController() = default;

RecursiveMutexCountedLockGuard EmulatedController::GetStateLock()
{
#ifdef _DEBUG
  return std::move(RecursiveMutexCountedLockGuard(&s_state_mutex, &s_state_mutex_count));
#else
  return std::move(RecursiveMutexCountedLockGuard(&s_state_mutex, nullptr));
#endif
}
void EmulatedController::EnsureStateLock()
{
#ifdef _DEBUG
  if (s_state_mutex_count <= 0)
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "EmulatedController state mutex is not locked");
#endif
}
std::unique_lock<std::recursive_mutex> EmulatedController::GetDevicesInputLock()
{
  // Needs to be recursive because controllers cache their own attached controllers
  std::unique_lock<std::recursive_mutex> lock(s_devices_input_mutex);
  return lock;
}

void EmulatedController::UpdateReferences(const ControllerInterface& devi)
{
  m_default_device_is_connected = devi.HasConnectedDevice(m_default_device);

  ciface::ExpressionParser::ControlEnvironment env(devi, GetDefaultDevice(), m_expression_vars);

  UpdateReferences(env);

  env.CleanUnusedVariables();
}

void EmulatedController::UpdateReferences(ciface::ExpressionParser::ControlEnvironment& env)
{
  const auto lock = GetStateLock();

  for (auto& ctrlGroup : groups)
  {
    for (auto& control : ctrlGroup->controls)
      control->control_ref->UpdateReference(env);

    for (auto& setting : ctrlGroup->numeric_settings)
      setting->GetInputReference().UpdateReference(env);

    // Attachments:
    if (ctrlGroup->type == GroupType::Attachments)
    {
      auto* const attachments = static_cast<Attachments*>(ctrlGroup.get());

      attachments->GetSelectionSetting().GetInputReference().UpdateReference(env);

      for (auto& attachment : attachments->GetAttachmentList())
        attachment->UpdateReferences(env);
    }
  }
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

void EmulatedController::CacheInputAndRefreshOutput()
{
  const auto state_lock = GetStateLock();
  const auto devices_input_lock = GetDevicesInputLock();

  for (auto& ctrlGroup : groups)
  {
    for (auto& control : ctrlGroup->controls)
    {
      // Cache inputs and refresh/apply outputs here.
      // This guarantees accurate timings in our control expressions,
      // and allows us to use all kinds of functions in outputs.
      // The only slight downside is that outputs will be applied a max of one input frame late.
      // Unfortunately outputs are set at random times by the emulation, so we don't have a reliable
      // place where to update them instead of here, especially because their functions also follow
      // the ControllerInterface timings.
      control->control_ref->UpdateState();
    }

    for (auto& setting : ctrlGroup->numeric_settings)
      setting->Update();

    // Attachments:
    if (ctrlGroup->type == GroupType::Attachments)
    {
      auto* const attachments = static_cast<Attachments*>(ctrlGroup.get());

      // This is an extra numeric setting not included in the list of numeric_settings
      attachments->GetSelectionSetting().Update();

      // Only cache the currently selected attachment
      attachments->GetAttachmentList()[attachments->GetSelectedAttachment()]
          ->CacheInputAndRefreshOutput();
    }
  }
}

bool EmulatedController::IsDefaultDeviceConnected() const
{
  return m_default_device_is_connected;
}

bool EmulatedController::HasDefaultDevice() const
{
  return !m_default_device.ToString().empty();
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

  for (auto& ctrlGroup : groups)
  {
    // Attachments:
    if (ctrlGroup->type == GroupType::Attachments)
    {
      for (auto& ai : static_cast<Attachments*>(ctrlGroup.get())->GetAttachmentList())
      {
        ai->SetDefaultDevice(m_default_device);
      }
    }
  }
}

void EmulatedController::LoadConfig(IniFile::Section* sec, const std::string& base)
{
  const auto lock = GetStateLock();

  std::string defdev = GetDefaultDevice().ToString();
  if (base.empty())
  {
    sec->Get(base + "Device", &defdev, "");
    SetDefaultDevice(defdev);
  }

  for (auto& cg : groups)
    cg->LoadConfig(sec, defdev, base);
}

void EmulatedController::SaveConfig(IniFile::Section* sec, const std::string& base)
{
  const auto lock = GetStateLock();

  const std::string defdev = GetDefaultDevice().ToString();
  if (base.empty())
    sec->Set(/*std::string(" ") +*/ base + "Device", defdev, "");

  for (auto& ctrlGroup : groups)
    ctrlGroup->SaveConfig(sec, defdev, base);
}

void EmulatedController::LoadDefaults(const ControllerInterface& ciface)
{
  const auto lock = GetStateLock();

  // load an empty inifile section, clears everything
  IniFile::Section sec;
  LoadConfig(&sec);

  const std::string& default_device_string = ciface.GetDefaultDeviceString();
  if (!default_device_string.empty())
  {
    SetDefaultDevice(default_device_string);
  }
}
}  // namespace ControllerEmu
