// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/HotkeyScheduler.h"

#include <algorithm>
#include <cmath>
#include <thread>

#include <QCoreApplication>

#include "AudioCommon/AudioCommon.h"

#include "Common/Config/Config.h"
#include "Common/Thread.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HotkeyManager.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"
#include "Core/State.h"

#include "DolphinQt/Settings.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

HotkeyScheduler::HotkeyScheduler() : m_stop_requested(false)
{
  HotkeyManagerEmu::Initialize();
  HotkeyManagerEmu::LoadConfig();
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
    frame_step_delay = std::min(frame_step_delay + 1, MAX_FRAME_STEP_DELAY);
    return;
  }

  if (IsHotkey(HK_FRAME_ADVANCE_DECREASE_SPEED))
  {
    frame_step_delay = std::max(frame_step_delay - 1, 0);
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
      Core::DoFrameStep();
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
  while (!m_stop_requested.IsSet())
  {
    Common::SleepCurrentThread(1000 / 60);

    if (!HotkeyManagerEmu::IsEnabled())
      continue;

    if (Core::GetState() == Core::State::Uninitialized || Core::GetState() == Core::State::Paused)
      g_controller_interface.UpdateInput();

    if (Core::GetState() != Core::State::Stopping)
    {
      HotkeyManagerEmu::GetStatus();

      if (!Core::IsRunningAndStarted())
        continue;

      if (IsHotkey(HK_OPEN))
        emit Open();

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

      // Refresh Game List
      if (IsHotkey(HK_REFRESH_LIST))
        emit RefreshGameListHotkey();

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

      // Exit
      if (IsHotkey(HK_EXIT))
        emit ExitHotkey();

      auto& settings = Settings::Instance();

      // Toggle Chat
      if (IsHotkey(HK_ACTIVATE_CHAT))
        emit ActivateChat();

      if (IsHotkey(HK_REQUEST_GOLF_CONTROL))
        emit RequestGolfControl();

      // Recording
      if (IsHotkey(HK_START_RECORDING))
        emit StartRecording();

      if (IsHotkey(HK_EXPORT_RECORDING))
        emit ExportRecording();

      if (IsHotkey(HK_READ_ONLY_MODE))
        emit ToggleReadOnlyMode();

      // Wiimote
      if (SConfig::GetInstance().m_bt_passthrough_enabled)
      {
        const auto ios = IOS::HLE::GetIOS();
        auto device = ios ? ios->GetDeviceByName("/dev/usb/oh1/57e/305") : nullptr;

        if (device != nullptr)
          std::static_pointer_cast<IOS::HLE::Device::BluetoothBase>(device)->UpdateSyncButtonState(
              IsHotkey(HK_TRIGGER_SYNC_BUTTON, true));
      }

      if (SConfig::GetInstance().bEnableDebugging)
      {
        CheckDebuggingHotkeys();
      }

      // TODO: HK_MBP_ADD

      if (SConfig::GetInstance().bWii)
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
                        (SConfig::GetInstance().m_IsMuted ?
                             "Muted" :
                             std::to_string(SConfig::GetInstance().m_Volume)) +
                        "%");
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
        AudioCommon::ToggleMuteVolume();
        ShowVolume();
      }

      // Graphics
      const auto efb_scale = Config::Get(Config::GFX_EFB_SCALE);
      auto ShowEFBScale = []() {
        switch (Config::Get(Config::GFX_EFB_SCALE))
        {
        case EFB_SCALE_AUTO_INTEGRAL:
          OSD::AddMessage("Internal Resolution: Auto (integral)");
          break;
        case 1:
          OSD::AddMessage("Internal Resolution: Native");
          break;
        default:
          OSD::AddMessage("Internal Resolution: %dx", g_Config.iEFBScale);
          break;
        }
      };

      if (IsHotkey(HK_INCREASE_IR))
      {
        Config::SetCurrent(Config::GFX_EFB_SCALE, efb_scale + 1);
        ShowEFBScale();
      }
      if (IsHotkey(HK_DECREASE_IR))
      {
        if (efb_scale > EFB_SCALE_AUTO_INTEGRAL)
        {
          Config::SetCurrent(Config::GFX_EFB_SCALE, efb_scale - 1);
          ShowEFBScale();
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
        case AspectMode::Analog:
          OSD::AddMessage("Force 4:3");
          break;
        case AspectMode::AnalogWide:
          OSD::AddMessage("Force 16:9");
          break;
        case AspectMode::Auto:
        default:
          OSD::AddMessage("Auto");
          break;
        }
      }
      if (IsHotkey(HK_TOGGLE_EFBCOPIES))
      {
        const bool new_value = !Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
        Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, new_value);
        OSD::AddMessage(StringFromFormat("Copy EFB: %s", new_value ? "to Texture" : "to RAM"));
      }

      auto ShowXFBCopies = []() {
        OSD::AddMessage(StringFromFormat(
            "Copy XFB: %s%s", Config::Get(Config::GFX_HACK_IMMEDIATE_XFB) ? " (Immediate)" : "",
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
        OSD::AddMessage(StringFromFormat("Fog: %s", new_value ? "Enabled" : "Disabled"));
      }

      if (IsHotkey(HK_TOGGLE_DUMPTEXTURES))
        Config::SetCurrent(Config::GFX_DUMP_TEXTURES, !Config::Get(Config::GFX_DUMP_TEXTURES));

      if (IsHotkey(HK_TOGGLE_TEXTURES))
        Config::SetCurrent(Config::GFX_HIRES_TEXTURES, !Config::Get(Config::GFX_HIRES_TEXTURES));

      Core::SetIsThrottlerTempDisabled(IsHotkey(HK_TOGGLE_THROTTLE, true));

      auto ShowEmulationSpeed = []() {
        OSD::AddMessage(
            SConfig::GetInstance().m_EmulationSpeed <= 0 ?
                "Speed Limit: Unlimited" :
                StringFromFormat("Speed Limit: %li%%",
                                 std::lround(SConfig::GetInstance().m_EmulationSpeed * 100.f)));
      };

      if (IsHotkey(HK_DECREASE_EMULATION_SPEED))
      {
        auto speed = SConfig::GetInstance().m_EmulationSpeed - 0.1;
        speed = (speed <= 0 || (speed >= 0.95 && speed <= 1.05)) ? 1.0 : speed;
        SConfig::GetInstance().m_EmulationSpeed = speed;
        ShowEmulationSpeed();
      }

      if (IsHotkey(HK_INCREASE_EMULATION_SPEED))
      {
        auto speed = SConfig::GetInstance().m_EmulationSpeed + 0.1;
        speed = (speed >= 0.95 && speed <= 1.05) ? 1.0 : speed;
        SConfig::GetInstance().m_EmulationSpeed = speed;
        ShowEmulationSpeed();
      }

      // Slot Saving / Loading
      if (IsHotkey(HK_SAVE_STATE_SLOT_SELECTED))
        emit StateSaveSlotHotkey();

      if (IsHotkey(HK_LOAD_STATE_SLOT_SELECTED))
        emit StateLoadSlotHotkey();
    }

    // Freelook
    static float fl_speed = 1.0;

    if (IsHotkey(HK_FREELOOK_DECREASE_SPEED, true))
      fl_speed /= 1.1f;

    if (IsHotkey(HK_FREELOOK_INCREASE_SPEED, true))
      fl_speed *= 1.1f;

    if (IsHotkey(HK_FREELOOK_RESET_SPEED, true))
      fl_speed = 1.0;

    if (IsHotkey(HK_FREELOOK_UP, true))
      VertexShaderManager::TranslateView(0.0, 0.0, -fl_speed);

    if (IsHotkey(HK_FREELOOK_DOWN, true))
      VertexShaderManager::TranslateView(0.0, 0.0, fl_speed);

    if (IsHotkey(HK_FREELOOK_LEFT, true))
      VertexShaderManager::TranslateView(fl_speed, 0.0);

    if (IsHotkey(HK_FREELOOK_RIGHT, true))
      VertexShaderManager::TranslateView(-fl_speed, 0.0);

    if (IsHotkey(HK_FREELOOK_ZOOM_IN, true))
      VertexShaderManager::TranslateView(0.0, fl_speed);

    if (IsHotkey(HK_FREELOOK_ZOOM_OUT, true))
      VertexShaderManager::TranslateView(0.0, -fl_speed);

    if (IsHotkey(HK_FREELOOK_RESET, true))
      VertexShaderManager::ResetView();

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
