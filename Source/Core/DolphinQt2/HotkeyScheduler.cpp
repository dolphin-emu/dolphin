// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/HotkeyScheduler.h"

#include <algorithm>
#include <thread>

#include <QCoreApplication>

#include "AudioCommon/AudioCommon.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HotkeyManager.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"
#include "Core/State.h"
#include "DolphinQt2/MainWindow.h"
#include "DolphinQt2/Settings.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

constexpr const char* DUBOIS_ALGORITHM_SHADER = "dubois";

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

static void HandleFrameskipHotkeys()
{
  constexpr int MAX_FRAME_SKIP_DELAY = 60;
  constexpr int FRAME_STEP_DELAY = 30;

  static int frame_step_count = 0;
  static int frame_step_delay = 1;
  static int frame_step_delay_count = 0;
  static bool frame_step_hold = false;

  if (IsHotkey(HK_FRAME_ADVANCE_INCREASE_SPEED))
  {
    frame_step_delay = std::min(frame_step_delay + 1, MAX_FRAME_SKIP_DELAY);
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

    // TODO GUI Update (Depends on an unimplemented feature)
    // if ((frame_step_count == 0 || frame_step_count == FRAME_STEP_DELAY) && !frame_step_hold)

    if (frame_step_count < FRAME_STEP_DELAY)
    {
      ++frame_step_count;
      if (frame_step_hold)
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

  if (frame_step_count > 0)
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

      // Fullscreen
      if (IsHotkey(HK_FULLSCREEN))
        emit FullScreenHotkey();

      // Pause and Unpause
      if (IsHotkey(HK_PLAY_PAUSE))
        emit PauseHotkey();

      // Stop
      if (IsHotkey(HK_STOP))
        emit StopHotkey();

      // Frameskipping
      HandleFrameskipHotkeys();

      // Screenshot
      if (IsHotkey(HK_SCREENSHOT))
        emit ScreenShotHotkey();

      // Exit
      if (IsHotkey(HK_EXIT))
        emit ExitHotkey();

      auto& settings = Settings::Instance();

      // Volume
      if (IsHotkey(HK_VOLUME_DOWN))
        settings.DecreaseVolume(3);

      if (IsHotkey(HK_VOLUME_UP))
        settings.IncreaseVolume(3);

      if (IsHotkey(HK_VOLUME_TOGGLE_MUTE))
        AudioCommon::ToggleMuteVolume();

      // Wiimote
      if (SConfig::GetInstance().m_bt_passthrough_enabled)
      {
        const auto ios = IOS::HLE::GetIOS();
        auto device = ios ? ios->GetDeviceByName("/dev/usb/oh1/57e/305") : nullptr;

        if (device != nullptr)
          std::static_pointer_cast<IOS::HLE::Device::BluetoothBase>(device)->UpdateSyncButtonState(
              IsHotkey(HK_TRIGGER_SYNC_BUTTON, true));
      }

      // TODO Debugging shortcuts (Separate PR)

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

        // TODO Implement Wiimote connecting / disconnecting (Separate PR)
        // if (wiimote_id > -1)
      }

      // Graphics
      if (IsHotkey(HK_INCREASE_IR))
        ++g_Config.iEFBScale;
      if (IsHotkey(HK_DECREASE_IR))
        g_Config.iEFBScale = std::max(g_Config.iEFBScale - 1, EFB_SCALE_AUTO_INTEGRAL);
      if (IsHotkey(HK_TOGGLE_CROP))
        g_Config.bCrop = !g_Config.bCrop;
      if (IsHotkey(HK_TOGGLE_AR))
        g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
      if (IsHotkey(HK_TOGGLE_EFBCOPIES))
        g_Config.bSkipEFBCopyToRam = !g_Config.bSkipEFBCopyToRam;
      if (IsHotkey(HK_TOGGLE_FOG))
        g_Config.bDisableFog = !g_Config.bDisableFog;
      if (IsHotkey(HK_TOGGLE_DUMPTEXTURES))
        g_Config.bDumpTextures = !g_Config.bDumpTextures;
      if (IsHotkey(HK_TOGGLE_TEXTURES))
        g_Config.bHiresTextures = !g_Config.bHiresTextures;

      Core::SetIsThrottlerTempDisabled(IsHotkey(HK_TOGGLE_THROTTLE, true));

      if (IsHotkey(HK_DECREASE_EMULATION_SPEED))
      {
        auto speed = SConfig::GetInstance().m_EmulationSpeed - 0.1;
        speed = (speed <= 0 || (speed >= 0.95 && speed <= 1.05)) ? 1.0 : speed;
        SConfig::GetInstance().m_EmulationSpeed = speed;
      }

      if (IsHotkey(HK_INCREASE_EMULATION_SPEED))
      {
        auto speed = SConfig::GetInstance().m_EmulationSpeed + 0.1;
        speed = (speed >= 0.95 && speed <= 1.05) ? 1.0 : speed;
        SConfig::GetInstance().m_EmulationSpeed = speed;
      }

      // Slot Saving / Loading
      if (IsHotkey(HK_SAVE_STATE_SLOT_SELECTED))
        emit StateSaveSlotHotkey();

      if (IsHotkey(HK_LOAD_STATE_SLOT_SELECTED))
        emit StateLoadSlotHotkey();

      // Stereoscopy
      if (IsHotkey(HK_TOGGLE_STEREO_SBS) || IsHotkey(HK_TOGGLE_STEREO_TAB))
      {
        if (g_Config.iStereoMode != STEREO_SBS)
        {
          // Disable post-processing shader, as stereoscopy itself is currently a shader
          if (g_Config.sPostProcessingShader == DUBOIS_ALGORITHM_SHADER)
            g_Config.sPostProcessingShader = "";

          g_Config.iStereoMode = IsHotkey(HK_TOGGLE_STEREO_SBS) ? STEREO_SBS : STEREO_TAB;
        }
        else
        {
          g_Config.iStereoMode = STEREO_OFF;
        }
      }

      if (IsHotkey(HK_TOGGLE_STEREO_ANAGLYPH))
      {
        if (g_Config.iStereoMode != STEREO_ANAGLYPH)
        {
          g_Config.iStereoMode = STEREO_ANAGLYPH;
          g_Config.sPostProcessingShader = DUBOIS_ALGORITHM_SHADER;
        }
        else
        {
          g_Config.iStereoMode = STEREO_OFF;
          g_Config.sPostProcessingShader = "";
        }
      }

      if (IsHotkey(HK_TOGGLE_STEREO_3DVISION))
      {
        if (g_Config.iStereoMode != STEREO_3DVISION)
        {
          if (g_Config.sPostProcessingShader == DUBOIS_ALGORITHM_SHADER)
            g_Config.sPostProcessingShader = "";

          g_Config.iStereoMode = STEREO_3DVISION;
        }
        else
        {
          g_Config.iStereoMode = STEREO_OFF;
        }
      }
    }

    if (IsHotkey(HK_DECREASE_DEPTH, true))
      g_Config.iStereoDepth = std::max(g_Config.iStereoDepth - 1, 0);

    if (IsHotkey(HK_INCREASE_DEPTH, true))
      g_Config.iStereoDepth = std::min(g_Config.iStereoDepth + 1, 100);

    if (IsHotkey(HK_DECREASE_CONVERGENCE, true))
      g_Config.iStereoConvergence = std::max(g_Config.iStereoConvergence - 5, 0);

    if (IsHotkey(HK_INCREASE_CONVERGENCE, true))
      g_Config.iStereoConvergence = std::min(g_Config.iStereoConvergence + 5, 500);

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
        State::Load(i + 1);

      if (IsHotkey(HK_SAVE_STATE_SLOT_1 + i))
        State::Save(i + 1);

      if (IsHotkey(HK_LOAD_LAST_STATE_1 + i))
        State::LoadLastSaved(i + 1);

      if (IsHotkey(HK_SELECT_STATE_SLOT_1 + i))
        emit SetStateSlotHotkey(i + 1);
    }

    if (IsHotkey(HK_SAVE_FIRST_STATE))
      State::SaveFirstSaved();

    if (IsHotkey(HK_UNDO_LOAD_STATE))
      State::UndoLoadState();

    if (IsHotkey(HK_UNDO_SAVE_STATE))
      State::UndoSaveState();
  }
}
