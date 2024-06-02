// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/HotkeyScheduler.h"

#include <algorithm>
#include <cmath>
#include <thread>

#include <fmt/format.h>

#include <QApplication>
#include <QCoreApplication>

#include "AudioCommon/AudioCommon.h"

#include "Common/Config/Config.h"
#include "Common/Thread.h"

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/FreeLookSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/Core.h"
#include "Core/FreeLookManager.h"
#include "Core/Host.h"
#include "Core/HotkeyManager.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"
#include "Core/State.h"
#include "Core/System.h"
#include "Core/WiiUtils.h"

#ifdef HAS_LIBMGBA
#include "DolphinQt/GBAWidget.h"
#endif
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Settings.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

constexpr const char* DUBOIS_ALGORITHM_SHADER = "dubois";

HotkeyScheduler::HotkeyScheduler() : m_stop_requested(false)
{
  HotkeyManagerEmu::Enable(true);
}

HotkeyScheduler::~HotkeyScheduler()
{
  Stop();
}

void HotkeyScheduler::Start()
{
  m_stop_requested.Set(false);
  m_thread = std::thread(&HotkeyScheduler::Run, this);
}

void HotkeyScheduler::Stop()
{
  m_stop_requested.Set(true);

  if (m_thread.joinable())
    m_thread.join();
}

static bool IsHotkey(int id, bool held = false)
{
  return HotkeyManagerEmu::IsPressed(id, held);
}

static void HandleFrameStepHotkeys()
{
  constexpr int MAX_FRAME_STEP_DELAY = 60;
  constexpr int FRAME_STEP_DELAY = 30;

  static int frame_step_count = 0;
  static int frame_step_delay = 1;
  static int frame_step_delay_count = 0;
  static bool frame_step_hold = false;

  if (IsHotkey(HK_FRAME_ADVANCE_INCREASE_SPEED))
  {
    frame_step_delay = std::max(frame_step_delay - 1, 0);
    return;
  }

  if (IsHotkey(HK_FRAME_ADVANCE_DECREASE_SPEED))
  {
    frame_step_delay = std::min(frame_step_delay + 1, MAX_FRAME_STEP_DELAY);
    return;
  }

  if (IsHotkey(HK_FRAME_ADVANCE_RESET_SPEED))
  {
    frame_step_delay = 1;
    return;
  }

  if (IsHotkey(HK_FRAME_ADVANCE, true))
  {
    if (frame_step_delay_count < frame_step_delay && frame_step_hold)
      frame_step_delay_count++;

    if ((frame_step_count == 0 || frame_step_count == FRAME_STEP_DELAY) && !frame_step_hold)
    {
      Core::QueueHostJob([](auto& system) { Core::DoFrameStep(system); });
      frame_step_hold = true;
    }

    if (frame_step_count < FRAME_STEP_DELAY)
    {
      frame_step_count++;
      frame_step_hold = false;
    }

    if (frame_step_count == FRAME_STEP_DELAY && frame_step_hold &&
        frame_step_delay_count >= frame_step_delay)
    {
      frame_step_hold = false;
      frame_step_delay_count = 0;
    }

    return;
  }
  else if (frame_step_count > 0)
  {
    // Reset frame advance
    frame_step_count = 0;
    frame_step_hold = false;
    frame_step_delay_count = 0;
  }
}

void HotkeyScheduler::Run()
{
  Common::SetCurrentThreadName("HotkeyScheduler");

  while (!m_stop_requested.IsSet())
  {
    Common::SleepCurrentThread(5);

    g_controller_interface.SetCurrentInputChannel(ciface::InputChannel::FreeLook);
    g_controller_interface.UpdateInput();
    FreeLook::UpdateInput();

    g_controller_interface.SetCurrentInputChannel(ciface::InputChannel::Host);
    g_controller_interface.UpdateInput();

    if (!HotkeyManagerEmu::IsEnabled())
      continue;

    Core::System& system = Core::System::GetInstance();
    if (Core::GetState(system) != Core::State::Stopping)
    {
      // Obey window focus (config permitting) before checking hotkeys.
      Core::UpdateInputGate(Config::Get(Config::MAIN_FOCUSED_HOTKEYS));

      HotkeyManagerEmu::GetStatus(false);

      // Everything else on the host thread (controller config dialog) should always get input.
      ControlReference::SetInputGate(true);

      HotkeyManagerEmu::GetStatus(true);

      // Open
      if (IsHotkey(HK_OPEN))
        emit Open();

      // Refresh Game List
      if (IsHotkey(HK_REFRESH_LIST))
        emit RefreshGameListHotkey();

      // Recording
      if (IsHotkey(HK_START_RECORDING))
        emit StartRecording();

      // Exit
      if (IsHotkey(HK_EXIT))
        emit ExitHotkey();

      if (Core::IsUninitialized(system))
      {
        // Only check for Play Recording hotkey when no game is running
        if (IsHotkey(HK_PLAY_RECORDING))
          emit PlayRecording();

        continue;
      }

      // Disc

      if (IsHotkey(HK_EJECT_DISC))
        emit EjectDisc();

      if (IsHotkey(HK_CHANGE_DISC))
        emit ChangeDisc();

      // Fullscreen
      if (IsHotkey(HK_FULLSCREEN))
      {
        emit FullScreenHotkey();

        // Prevent fullscreen from getting toggled too often
        Common::SleepCurrentThread(100);
      }

      // Pause and Unpause
      if (IsHotkey(HK_PLAY_PAUSE))
        emit TogglePauseHotkey();

      // Stop
      if (IsHotkey(HK_STOP))
        emit StopHotkey();

      // Reset
      if (IsHotkey(HK_RESET))
        emit ResetHotkey();

      // Frame advance
      HandleFrameStepHotkeys();

      // Screenshot
      if (IsHotkey(HK_SCREENSHOT))
        emit ScreenShotHotkey();

      // Unlock Cursor
      if (IsHotkey(HK_UNLOCK_CURSOR))
        emit UnlockCursor();

      if (IsHotkey(HK_CENTER_MOUSE, true))
        g_controller_interface.SetMouseCenteringRequested(true);

      auto& settings = Settings::Instance();

      // Toggle Chat
      if (IsHotkey(HK_ACTIVATE_CHAT))
        emit ActivateChat();

      if (IsHotkey(HK_REQUEST_GOLF_CONTROL))
        emit RequestGolfControl();

      if (IsHotkey(HK_EXPORT_RECORDING))
        emit ExportRecording();

      if (IsHotkey(HK_READ_ONLY_MODE))
        emit ToggleReadOnlyMode();

      // Wiimote
      if (auto bt = WiiUtils::GetBluetoothRealDevice())
        bt->UpdateSyncButtonState(IsHotkey(HK_TRIGGER_SYNC_BUTTON, true));

      if (Config::IsDebuggingEnabled())
      {
        CheckDebuggingHotkeys();
      }

      // TODO: HK_MBP_ADD

      if (Core::System::GetInstance().IsWii())
      {
        int wiimote_id = -1;
        if (IsHotkey(HK_WIIMOTE1_CONNECT))
          wiimote_id = 0;
        if (IsHotkey(HK_WIIMOTE2_CONNECT))
          wiimote_id = 1;
        if (IsHotkey(HK_WIIMOTE3_CONNECT))
          wiimote_id = 2;
        if (IsHotkey(HK_WIIMOTE4_CONNECT))
          wiimote_id = 3;
        if (IsHotkey(HK_BALANCEBOARD_CONNECT))
          wiimote_id = 4;

        if (wiimote_id > -1)
          emit ConnectWiiRemote(wiimote_id);

        if (IsHotkey(HK_TOGGLE_SD_CARD))
          Settings::Instance().SetSDCardInserted(!Settings::Instance().IsSDCardInserted());

        if (IsHotkey(HK_TOGGLE_USB_KEYBOARD))
        {
          Settings::Instance().SetUSBKeyboardConnected(
              !Settings::Instance().IsUSBKeyboardConnected());
        }
      }

      if (IsHotkey(HK_PREV_WIIMOTE_PROFILE_1))
        m_profile_cycler.PreviousWiimoteProfile(0);
      else if (IsHotkey(HK_NEXT_WIIMOTE_PROFILE_1))
        m_profile_cycler.NextWiimoteProfile(0);

      if (IsHotkey(HK_PREV_WIIMOTE_PROFILE_2))
        m_profile_cycler.PreviousWiimoteProfile(1);
      else if (IsHotkey(HK_NEXT_WIIMOTE_PROFILE_2))
        m_profile_cycler.NextWiimoteProfile(1);

      if (IsHotkey(HK_PREV_WIIMOTE_PROFILE_3))
        m_profile_cycler.PreviousWiimoteProfile(2);
      else if (IsHotkey(HK_NEXT_WIIMOTE_PROFILE_3))
        m_profile_cycler.NextWiimoteProfile(2);

      if (IsHotkey(HK_PREV_WIIMOTE_PROFILE_4))
        m_profile_cycler.PreviousWiimoteProfile(3);
      else if (IsHotkey(HK_NEXT_WIIMOTE_PROFILE_4))
        m_profile_cycler.NextWiimoteProfile(3);

      if (IsHotkey(HK_PREV_GAME_WIIMOTE_PROFILE_1))
        m_profile_cycler.PreviousWiimoteProfileForGame(0);
      else if (IsHotkey(HK_NEXT_GAME_WIIMOTE_PROFILE_1))
        m_profile_cycler.NextWiimoteProfileForGame(0);

      if (IsHotkey(HK_PREV_GAME_WIIMOTE_PROFILE_2))
        m_profile_cycler.PreviousWiimoteProfileForGame(1);
      else if (IsHotkey(HK_NEXT_GAME_WIIMOTE_PROFILE_2))
        m_profile_cycler.NextWiimoteProfileForGame(1);

      if (IsHotkey(HK_PREV_GAME_WIIMOTE_PROFILE_3))
        m_profile_cycler.PreviousWiimoteProfileForGame(2);
      else if (IsHotkey(HK_NEXT_GAME_WIIMOTE_PROFILE_3))
        m_profile_cycler.NextWiimoteProfileForGame(2);

      if (IsHotkey(HK_PREV_GAME_WIIMOTE_PROFILE_4))
        m_profile_cycler.PreviousWiimoteProfileForGame(3);
      else if (IsHotkey(HK_NEXT_GAME_WIIMOTE_PROFILE_4))
        m_profile_cycler.NextWiimoteProfileForGame(3);

      auto ShowVolume = []() {
        OSD::AddMessage(std::string("Volume: ") +
                        (Config::Get(Config::MAIN_AUDIO_MUTED) ?
                             "Muted" :
                             std::to_string(Config::Get(Config::MAIN_AUDIO_VOLUME)) + "%"));
      };

      // Volume
      if (IsHotkey(HK_VOLUME_DOWN))
      {
        settings.DecreaseVolume(3);
        ShowVolume();
      }

      if (IsHotkey(HK_VOLUME_UP))
      {
        settings.IncreaseVolume(3);
        ShowVolume();
      }

      if (IsHotkey(HK_VOLUME_TOGGLE_MUTE))
      {
        AudioCommon::ToggleMuteVolume(Core::System::GetInstance());
        ShowVolume();
      }

      // Graphics
      const auto efb_scale = Config::Get(Config::GFX_EFB_SCALE);
      const auto ShowEFBScale = [](int new_efb_scale) {
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
      };

      if (IsHotkey(HK_INCREASE_IR))
      {
        Config::SetCurrent(Config::GFX_EFB_SCALE, efb_scale + 1);
        ShowEFBScale(efb_scale + 1);
      }
      if (IsHotkey(HK_DECREASE_IR))
      {
        if (efb_scale > EFB_SCALE_AUTO_INTEGRAL)
        {
          Config::SetCurrent(Config::GFX_EFB_SCALE, efb_scale - 1);
          ShowEFBScale(efb_scale - 1);
        }
      }

      if (IsHotkey(HK_TOGGLE_CROP))
        Config::SetCurrent(Config::GFX_CROP, !Config::Get(Config::GFX_CROP));

      if (IsHotkey(HK_TOGGLE_AR))
      {
        const int aspect_ratio = (static_cast<int>(Config::Get(Config::GFX_ASPECT_RATIO)) + 1) & 3;
        Config::SetCurrent(Config::GFX_ASPECT_RATIO, static_cast<AspectMode>(aspect_ratio));
        switch (static_cast<AspectMode>(aspect_ratio))
        {
        case AspectMode::Stretch:
          OSD::AddMessage("Stretch");
          break;
        case AspectMode::ForceStandard:
          OSD::AddMessage("Force 4:3");
          break;
        case AspectMode::ForceWide:
          OSD::AddMessage("Force 16:9");
          break;
        case AspectMode::Custom:
          OSD::AddMessage("Custom");
          break;
        case AspectMode::CustomStretch:
          OSD::AddMessage("Custom (Stretch)");
          break;
        case AspectMode::Raw:
          OSD::AddMessage("Raw (Square Pixels)");
          break;
        case AspectMode::Auto:
        default:
          OSD::AddMessage("Auto");
          break;
        }
      }

      if (IsHotkey(HK_TOGGLE_SKIP_EFB_ACCESS))
      {
        const bool new_value = !Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE);
        Config::SetCurrent(Config::GFX_HACK_EFB_ACCESS_ENABLE, new_value);
        OSD::AddMessage(fmt::format("{} EFB Access from CPU", new_value ? "Skip" : "Don't skip"));
      }

      if (IsHotkey(HK_TOGGLE_EFBCOPIES))
      {
        const bool new_value = !Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
        Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, new_value);
        OSD::AddMessage(fmt::format("Copy EFB: {}", new_value ? "to Texture" : "to RAM"));
      }

      auto ShowXFBCopies = []() {
        OSD::AddMessage(fmt::format(
            "Copy XFB: {}{}", Config::Get(Config::GFX_HACK_IMMEDIATE_XFB) ? " (Immediate)" : "",
            Config::Get(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM) ? "to Texture" : "to RAM"));
      };

      if (IsHotkey(HK_TOGGLE_XFBCOPIES))
      {
        Config::SetCurrent(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM,
                           !Config::Get(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM));
        ShowXFBCopies();
      }
      if (IsHotkey(HK_TOGGLE_IMMEDIATE_XFB))
      {
        Config::SetCurrent(Config::GFX_HACK_IMMEDIATE_XFB,
                           !Config::Get(Config::GFX_HACK_IMMEDIATE_XFB));
        ShowXFBCopies();
      }
      if (IsHotkey(HK_TOGGLE_FOG))
      {
        const bool new_value = !Config::Get(Config::GFX_DISABLE_FOG);
        Config::SetCurrent(Config::GFX_DISABLE_FOG, new_value);
        OSD::AddMessage(fmt::format("Fog: {}", new_value ? "Enabled" : "Disabled"));
      }

      if (IsHotkey(HK_TOGGLE_DUMPTEXTURES))
        Config::SetCurrent(Config::GFX_DUMP_TEXTURES, !Config::Get(Config::GFX_DUMP_TEXTURES));

      if (IsHotkey(HK_TOGGLE_TEXTURES))
        Config::SetCurrent(Config::GFX_HIRES_TEXTURES, !Config::Get(Config::GFX_HIRES_TEXTURES));

      Core::SetIsThrottlerTempDisabled(IsHotkey(HK_TOGGLE_THROTTLE, true));

      auto ShowEmulationSpeed = []() {
        const float emulation_speed = Config::Get(Config::MAIN_EMULATION_SPEED);
        if (!AchievementManager::GetInstance().IsHardcoreModeActive() ||
            Config::Get(Config::MAIN_EMULATION_SPEED) >= 1.0f ||
            Config::Get(Config::MAIN_EMULATION_SPEED) <= 0.0f)
        {
          OSD::AddMessage(emulation_speed <= 0 ? "Speed Limit: Unlimited" :
                                                 fmt::format("Speed Limit: {}%",
                                                             std::lround(emulation_speed * 100.f)));
        }
      };

      if (IsHotkey(HK_DECREASE_EMULATION_SPEED))
      {
        auto speed = Config::Get(Config::MAIN_EMULATION_SPEED) - 0.1;
        if (speed > 0)
        {
          speed = (speed >= 0.95 && speed <= 1.05) ? 1.0 : speed;
          Config::SetCurrent(Config::MAIN_EMULATION_SPEED, speed);
        }
        ShowEmulationSpeed();
      }

      if (IsHotkey(HK_INCREASE_EMULATION_SPEED))
      {
        auto speed = Config::Get(Config::MAIN_EMULATION_SPEED) + 0.1;
        speed = (speed >= 0.95 && speed <= 1.05) ? 1.0 : speed;
        Config::SetCurrent(Config::MAIN_EMULATION_SPEED, speed);
        ShowEmulationSpeed();
      }

      // USB Device Emulation
      if (IsHotkey(HK_SKYLANDERS_PORTAL))
        emit SkylandersPortalHotkey();

      if (IsHotkey(HK_INFINITY_BASE))
        emit InfinityBaseHotkey();

      // Slot Saving / Loading
      if (IsHotkey(HK_SAVE_STATE_SLOT_SELECTED))
        emit StateSaveSlotHotkey();

      if (IsHotkey(HK_LOAD_STATE_SLOT_SELECTED))
        emit StateLoadSlotHotkey();

      if (IsHotkey(HK_INCREMENT_SELECTED_STATE_SLOT))
        emit IncrementSelectedStateSlotHotkey();

      if (IsHotkey(HK_DECREMENT_SELECTED_STATE_SLOT))
        emit DecrementSelectedStateSlotHotkey();

      // Stereoscopy
      if (IsHotkey(HK_TOGGLE_STEREO_SBS))
      {
        if (Config::Get(Config::GFX_STEREO_MODE) != StereoMode::SBS)
        {
          // Disable post-processing shader, as stereoscopy itself is currently a shader
          if (Config::Get(Config::GFX_ENHANCE_POST_SHADER) == DUBOIS_ALGORITHM_SHADER)
            Config::SetCurrent(Config::GFX_ENHANCE_POST_SHADER, "");

          Config::SetCurrent(Config::GFX_STEREO_MODE, StereoMode::SBS);
        }
        else
        {
          Config::SetCurrent(Config::GFX_STEREO_MODE, StereoMode::Off);
        }
      }

      if (IsHotkey(HK_TOGGLE_STEREO_TAB))
      {
        if (Config::Get(Config::GFX_STEREO_MODE) != StereoMode::TAB)
        {
          // Disable post-processing shader, as stereoscopy itself is currently a shader
          if (Config::Get(Config::GFX_ENHANCE_POST_SHADER) == DUBOIS_ALGORITHM_SHADER)
            Config::SetCurrent(Config::GFX_ENHANCE_POST_SHADER, "");

          Config::SetCurrent(Config::GFX_STEREO_MODE, StereoMode::TAB);
        }
        else
        {
          Config::SetCurrent(Config::GFX_STEREO_MODE, StereoMode::Off);
        }
      }

      if (IsHotkey(HK_TOGGLE_STEREO_ANAGLYPH))
      {
        if (Config::Get(Config::GFX_STEREO_MODE) != StereoMode::Anaglyph)
        {
          Config::SetCurrent(Config::GFX_STEREO_MODE, StereoMode::Anaglyph);
          Config::SetCurrent(Config::GFX_ENHANCE_POST_SHADER, DUBOIS_ALGORITHM_SHADER);
        }
        else
        {
          Config::SetCurrent(Config::GFX_STEREO_MODE, StereoMode::Off);
          Config::SetCurrent(Config::GFX_ENHANCE_POST_SHADER, "");
        }
      }

      CheckGBAHotkeys();
    }

    const auto stereo_depth = Config::Get(Config::GFX_STEREO_DEPTH);

    if (IsHotkey(HK_DECREASE_DEPTH, true))
      Config::SetCurrent(Config::GFX_STEREO_DEPTH, std::max(stereo_depth - 1, 0));

    if (IsHotkey(HK_INCREASE_DEPTH, true))
      Config::SetCurrent(Config::GFX_STEREO_DEPTH,
                         std::min(stereo_depth + 1, Config::GFX_STEREO_DEPTH_MAXIMUM));

    const auto stereo_convergence = Config::Get(Config::GFX_STEREO_CONVERGENCE);

    if (IsHotkey(HK_DECREASE_CONVERGENCE, true))
      Config::SetCurrent(Config::GFX_STEREO_CONVERGENCE, std::max(stereo_convergence - 5, 0));

    if (IsHotkey(HK_INCREASE_CONVERGENCE, true))
      Config::SetCurrent(Config::GFX_STEREO_CONVERGENCE,
                         std::min(stereo_convergence + 5, Config::GFX_STEREO_CONVERGENCE_MAXIMUM));

    // Free Look
    if (IsHotkey(HK_FREELOOK_TOGGLE))
    {
      const bool new_value = !Config::Get(Config::FREE_LOOK_ENABLED);
      Config::SetCurrent(Config::FREE_LOOK_ENABLED, new_value);

      const bool hardcore = AchievementManager::GetInstance().IsHardcoreModeActive();
      if (hardcore)
        OSD::AddMessage("Free Look is Disabled in Hardcore Mode");
      else
        OSD::AddMessage(fmt::format("Free Look: {}", new_value ? "Enabled" : "Disabled"));
    }

    // Savestates
    for (u32 i = 0; i < State::NUM_STATES; i++)
    {
      if (IsHotkey(HK_LOAD_STATE_SLOT_1 + i))
        emit StateLoadSlot(i + 1);

      if (IsHotkey(HK_SAVE_STATE_SLOT_1 + i))
        emit StateSaveSlot(i + 1);

      if (IsHotkey(HK_LOAD_LAST_STATE_1 + i))
        emit StateLoadLastSaved(i + 1);

      if (IsHotkey(HK_SELECT_STATE_SLOT_1 + i))
        emit SetStateSlotHotkey(i + 1);
    }

    if (IsHotkey(HK_SAVE_FIRST_STATE))
      emit StateSaveOldest();

    if (IsHotkey(HK_UNDO_LOAD_STATE))
      emit StateLoadUndo();

    if (IsHotkey(HK_UNDO_SAVE_STATE))
      emit StateSaveUndo();

    if (IsHotkey(HK_LOAD_STATE_FILE))
      emit StateLoadFile();

    if (IsHotkey(HK_SAVE_STATE_FILE))
      emit StateSaveFile();
  }
}

void HotkeyScheduler::CheckDebuggingHotkeys()
{
  if (IsHotkey(HK_STEP))
    emit Step();

  if (IsHotkey(HK_STEP_OVER))
    emit StepOver();

  if (IsHotkey(HK_STEP_OUT))
    emit StepOut();

  if (IsHotkey(HK_SKIP))
    emit Skip();

  if (IsHotkey(HK_SHOW_PC))
    emit ShowPC();

  if (IsHotkey(HK_SET_PC))
    emit Skip();

  if (IsHotkey(HK_BP_TOGGLE))
    emit ToggleBreakpoint();

  if (IsHotkey(HK_BP_ADD))
    emit AddBreakpoint();
}

void HotkeyScheduler::CheckGBAHotkeys()
{
#ifdef HAS_LIBMGBA
  GBAWidget* gba_widget = qobject_cast<GBAWidget*>(QApplication::activeWindow());
  if (!gba_widget)
    return;

  if (IsHotkey(HK_GBA_LOAD))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->LoadROM(); });

  if (IsHotkey(HK_GBA_UNLOAD))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->UnloadROM(); });

  if (IsHotkey(HK_GBA_RESET))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->ResetCore(); });

  if (IsHotkey(HK_GBA_VOLUME_DOWN))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->VolumeDown(); });

  if (IsHotkey(HK_GBA_VOLUME_UP))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->VolumeUp(); });

  if (IsHotkey(HK_GBA_TOGGLE_MUTE))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->ToggleMute(); });

  if (IsHotkey(HK_GBA_1X))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->Resize(1); });

  if (IsHotkey(HK_GBA_2X))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->Resize(2); });

  if (IsHotkey(HK_GBA_3X))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->Resize(3); });

  if (IsHotkey(HK_GBA_4X))
    QueueOnObject(gba_widget, [gba_widget] { gba_widget->Resize(4); });
#endif
}
