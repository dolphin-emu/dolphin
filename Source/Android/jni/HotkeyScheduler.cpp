// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/HotkeyScheduler.h"

#include <chrono>
#include <cmath>
#include <jni.h>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include "AudioCommon/AudioCommon.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Thread.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HotkeyManager.h"
#include "Core/State.h"
#include "Core/System.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

namespace HotkeyScheduler
{
namespace
{
constexpr int POLL_INTERVAL_MS = 5;

std::thread s_thread;
Common::Flag s_stop_requested;
Common::Flag s_foreground{true};
Common::Flag s_any_hotkey_set;
Common::Event s_wakeup;

bool IsHotkey(int id, bool held = false)
{
  return HotkeyManagerEmu::IsPressed(id, held);
}

bool IsAnyHotkeySet()
{
  InputConfig* const config = HotkeyManagerEmu::GetConfig();
  if (config == nullptr || config->ControllersNeedToBeCreated())
    return false;

  auto* const hotkeys = static_cast<HotkeyManager*>(config->GetController(0));

  const auto lock = ControllerEmu::EmulatedController::GetStateLock();

  for (const int hotkey : GetSupportedHotkeys())
  {
    const int group = hotkeys->FindGroupByID(hotkey);
    const int index = hotkeys->GetIndexForGroup(group, hotkey);
    const auto* const control_group =
        HotkeyManagerEmu::GetHotkeyGroup(static_cast<HotkeyGroup>(group));

    if (!control_group->controls[index]->control_ref->GetExpression().empty())
      return true;
  }

  return false;
}

bool ShouldPoll()
{
  if (!s_foreground.IsSet() || !s_any_hotkey_set.IsSet())
    return false;

  if (!HotkeyManagerEmu::IsEnabled())
    return false;

  return Core::GetState(Core::System::GetInstance()) != Core::State::Stopping;
}

// Buttons already held when polling resumes must not count as new presses
void ConsumePendingPresses()
{
  for (const int hotkey : GetSupportedHotkeys())
    IsHotkey(hotkey);
}

void ShowVolumeOSD()
{
  OSD::AddMessage(
      fmt::format("Volume: {}", Config::Get(Config::MAIN_AUDIO_MUTED) ?
                                    "Muted" :
                                    fmt::format("{}%", Config::Get(Config::MAIN_AUDIO_VOLUME))));
}

void ShowEmulationSpeedOSD()
{
  const float emulation_speed = Config::Get(Config::MAIN_EMULATION_SPEED);
  OSD::AddMessage(emulation_speed <= 0 ?
                      "Speed Limit: Unlimited" :
                      fmt::format("Speed Limit: {}%", std::lround(emulation_speed * 100.f)));
}

void ShowInternalResolutionOSD(int new_efb_scale)
{
  switch (new_efb_scale)
  {
  case EFB_SCALE_AUTO_INTEGRAL:
    OSD::AddMessage("Internal Resolution: Auto (integral)");
    break;
  case 1:
    OSD::AddMessage("Internal Resolution: Native");
    break;
  default:
    OSD::AddMessage(fmt::format("Internal Resolution: {}x", new_efb_scale));
    break;
  }
}

void HandleGeneralHotkeys()
{
  // TODO: add support for more general hotkeys
  if (IsHotkey(HK_SCREENSHOT))
    Core::SaveScreenShot();
}

void HandleEmulationSpeedHotkeys()
{
  if (IsHotkey(HK_TOGGLE_THROTTLE))
    Core::SetIsThrottlerTempDisabled(!Core::GetIsThrottlerTempDisabled());

  if (IsHotkey(HK_DECREASE_EMULATION_SPEED))
  {
    auto speed = Config::Get(Config::MAIN_EMULATION_SPEED) - 0.1;
    if (speed > 0)
    {
      speed = (speed >= 0.95 && speed <= 1.05) ? 1.0 : speed;
      Config::SetCurrent(Config::MAIN_EMULATION_SPEED, speed);
    }
    ShowEmulationSpeedOSD();
  }

  if (IsHotkey(HK_INCREASE_EMULATION_SPEED))
  {
    auto speed = Config::Get(Config::MAIN_EMULATION_SPEED) + 0.1;
    speed = (speed >= 0.95 && speed <= 1.05) ? 1.0 : speed;
    Config::SetCurrent(Config::MAIN_EMULATION_SPEED, speed);
    ShowEmulationSpeedOSD();
  }
}

void HandleVolumeHotkeys(Core::System& system)
{
  if (IsHotkey(HK_VOLUME_DOWN))
  {
    AudioCommon::DecreaseVolume(system, 3);
    ShowVolumeOSD();
  }

  if (IsHotkey(HK_VOLUME_UP))
  {
    AudioCommon::IncreaseVolume(system, 3);
    ShowVolumeOSD();
  }

  if (IsHotkey(HK_VOLUME_TOGGLE_MUTE))
  {
    AudioCommon::ToggleMuteVolume(system);
    ShowVolumeOSD();
  }
}

void HandleGraphicsHotkeys()
{
  const auto efb_scale = Config::Get(Config::GFX_EFB_SCALE);

  if (IsHotkey(HK_INCREASE_IR))
  {
    Config::SetCurrent(Config::GFX_EFB_SCALE, efb_scale + 1);
    ShowInternalResolutionOSD(efb_scale + 1);
  }
  if (IsHotkey(HK_DECREASE_IR))
  {
    if (efb_scale > EFB_SCALE_AUTO_INTEGRAL)
    {
      Config::SetCurrent(Config::GFX_EFB_SCALE, efb_scale - 1);
      ShowInternalResolutionOSD(efb_scale - 1);
    }
  }
}

void HandleStateHotkeys(Core::System& system)
{
  for (u32 i = 0; i < State::NUM_STATES; ++i)
  {
    if (IsHotkey(HK_LOAD_STATE_SLOT_1 + i))
      State::Load(system, i + 1);

    if (IsHotkey(HK_SAVE_STATE_SLOT_1 + i))
      State::Save(system, i + 1);

    if (IsHotkey(HK_LOAD_LAST_STATE_1 + i))
      State::LoadLastSaved(system, i + 1);

    if (IsHotkey(HK_SELECT_STATE_SLOT_1 + i))
      State::SelectSlot(i + 1);
  }

  if (IsHotkey(HK_SAVE_STATE_SLOT_SELECTED))
    State::SaveSelected(system);

  if (IsHotkey(HK_LOAD_STATE_SLOT_SELECTED))
    State::LoadSelected(system);

  if (IsHotkey(HK_INCREMENT_SELECTED_STATE_SLOT))
    State::SelectSlot(State::GetNextSlot());

  if (IsHotkey(HK_DECREMENT_SELECTED_STATE_SLOT))
    State::SelectSlot(State::GetPreviousSlot());

  if (IsHotkey(HK_SAVE_FIRST_STATE))
    State::SaveFirstSaved(system);

  if (IsHotkey(HK_UNDO_LOAD_STATE))
    State::UndoLoadState(system);

  if (IsHotkey(HK_UNDO_SAVE_STATE))
    State::UndoSaveState(system);
}

void Run()
{
  Common::SetCurrentThreadName("HotkeyScheduler");

  bool was_polling = false;

  while (!s_stop_requested.IsSet())
  {
    if (!ShouldPoll())
    {
      was_polling = false;
      s_wakeup.Wait();
      continue;
    }

    s_wakeup.WaitFor(std::chrono::milliseconds(POLL_INTERVAL_MS));

    if (s_stop_requested.IsSet())
      break;

    ControllerInterface::SetCurrentInputChannel(ciface::InputChannel::Host);
    g_controller_interface.UpdateInput();

    ControlReference::SetInputGate(true);

    // These two cover disjoint sets of groups, see HotkeyManager::GetInput.
    // Skipping either one leaves half the groups reading as unpressed.
    HotkeyManagerEmu::GetStatus(false);
    HotkeyManagerEmu::GetStatus(true);

    if (!was_polling)
    {
      ConsumePendingPresses();
      was_polling = true;
      continue;
    }

    Core::System& system = Core::System::GetInstance();

    HandleGeneralHotkeys();
    HandleEmulationSpeedHotkeys();
    HandleVolumeHotkeys(system);
    HandleGraphicsHotkeys();
    HandleStateHotkeys(system);
  }
}
}  // namespace

void Start()
{
  if (s_thread.joinable())
    return;

  HotkeyManagerEmu::Enable(true);
  State::SetSelectedSlot(1);
  s_any_hotkey_set.Set(IsAnyHotkeySet());
  s_stop_requested.Clear();
  s_wakeup.Reset();
  s_thread = std::thread(Run);
}

void Stop()
{
  if (!s_thread.joinable())
    return;

  s_stop_requested.Set();
  s_wakeup.Set();
  s_thread.join();

  Core::SetIsThrottlerTempDisabled(false);
}

void SetBackgroundExecutionAllowed(bool allowed)
{
  s_foreground.Set(allowed);
  s_wakeup.Set();
}

void RefreshActiveHotkeys()
{
  s_any_hotkey_set.Set(IsAnyHotkeySet());
  s_wakeup.Set();
}

const std::vector<int>& GetSupportedHotkeys()
{
  static const std::vector<int> supported = [] {
    std::vector<int> hotkeys{
        // General - only the one entry, see HandleGeneralHotkeys
        HK_SCREENSHOT,
        // Emulation Speed
        HK_TOGGLE_THROTTLE,
        HK_DECREASE_EMULATION_SPEED,
        HK_INCREASE_EMULATION_SPEED,
        // Volume
        HK_VOLUME_DOWN,
        HK_VOLUME_UP,
        HK_VOLUME_TOGGLE_MUTE,
        // Internal Resolution
        HK_INCREASE_IR,
        HK_DECREASE_IR,
    };

    hotkeys.insert(hotkeys.end(),
                   {HK_SAVE_STATE_SLOT_SELECTED, HK_LOAD_STATE_SLOT_SELECTED,
                    HK_INCREMENT_SELECTED_STATE_SLOT, HK_DECREMENT_SELECTED_STATE_SLOT,
                    HK_SAVE_FIRST_STATE, HK_UNDO_LOAD_STATE, HK_UNDO_SAVE_STATE});

    for (u32 i = 0; i < State::NUM_STATES; ++i)
      hotkeys.push_back(HK_SAVE_STATE_SLOT_1 + i);
    for (u32 i = 0; i < State::NUM_STATES; ++i)
      hotkeys.push_back(HK_LOAD_STATE_SLOT_1 + i);
    for (u32 i = 0; i < State::NUM_STATES; ++i)
      hotkeys.push_back(HK_SELECT_STATE_SLOT_1 + i);
    for (u32 i = 0; i < State::NUM_STATES; ++i)
      hotkeys.push_back(HK_LOAD_LAST_STATE_1 + i);

    return hotkeys;
  }();

  return supported;
}
}  // namespace HotkeyScheduler

extern "C" {

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_input_model_Hotkeys_setEnabled(
    JNIEnv*, jclass, jboolean enabled)
{
  HotkeyManagerEmu::Enable(enabled == JNI_TRUE);
  HotkeyScheduler::RefreshActiveHotkeys();
}
}
