// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Wiimote.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/WiimoteDevice.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"

// Limit the amount of wiimote connect requests, when a button is pressed in disconnected state
static std::array<u8, MAX_BBMOTES> s_last_connect_request_counter;

namespace Wiimote
{
static InputConfig s_config(WIIMOTE_INI_NAME, _trans("Wii Remote"), "Wiimote");

InputConfig* GetConfig()
{
  return &s_config;
}

ControllerEmu::ControlGroup* GetWiimoteGroup(int number, WiimoteEmu::WiimoteGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetWiimoteGroup(group);
}

ControllerEmu::ControlGroup* GetNunchukGroup(int number, WiimoteEmu::NunchukGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetNunchukGroup(group);
}

ControllerEmu::ControlGroup* GetClassicGroup(int number, WiimoteEmu::ClassicGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetClassicGroup(group);
}

ControllerEmu::ControlGroup* GetGuitarGroup(int number, WiimoteEmu::GuitarGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetGuitarGroup(group);
}

ControllerEmu::ControlGroup* GetDrumsGroup(int number, WiimoteEmu::DrumsGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetDrumsGroup(group);
}

ControllerEmu::ControlGroup* GetTurntableGroup(int number, WiimoteEmu::TurntableGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))
      ->GetTurntableGroup(group);
}

void Shutdown()
{
  s_config.ClearControllers();

  WiimoteReal::Stop();
}

void Initialize(InitializeMode init_mode)
{
  if (s_config.ControllersNeedToBeCreated())
  {
    for (unsigned int i = WIIMOTE_CHAN_0; i < MAX_BBMOTES; ++i)
      s_config.CreateController<WiimoteEmu::Wiimote>(i);
  }

  g_controller_interface.RegisterHotplugCallback(LoadConfig);

  LoadConfig();

  WiimoteReal::Initialize(init_mode);

  // Reload Wiimotes with our settings
  if (Movie::IsMovieActive())
    Movie::ChangeWiiPads();
}

void Connect(unsigned int index, bool connect)
{
  if (SConfig::GetInstance().m_bt_passthrough_enabled || index >= MAX_BBMOTES)
    return;

  const auto ios = IOS::HLE::GetIOS();
  if (!ios)
    return;

  const auto bluetooth = std::static_pointer_cast<IOS::HLE::Device::BluetoothEmu>(
      ios->GetDeviceByName("/dev/usb/oh1/57e/305"));

  if (bluetooth)
    bluetooth->AccessWiiMote(index | 0x100)->Activate(connect);

  const char* message = connect ? "Wii Remote %i connected" : "Wii Remote %i disconnected";
  Core::DisplayMessage(StringFromFormat(message, index + 1), 3000);
}

void ResetAllWiimotes()
{
  for (int i = WIIMOTE_CHAN_0; i < MAX_BBMOTES; ++i)
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(i))->Reset();
}

void LoadConfig()
{
  s_config.LoadConfig(false);
  s_last_connect_request_counter.fill(0);
}

void Resume()
{
  WiimoteReal::Resume();
}

void Pause()
{
  WiimoteReal::Pause();
}

// An L2CAP packet is passed from the Core to the Wiimote on the HID CONTROL channel.
void ControlChannel(int number, u16 channel_id, const void* data, u32 size)
{
  if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[number])
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))
        ->ControlChannel(channel_id, data, size);
}

// An L2CAP packet is passed from the Core to the Wiimote on the HID INTERRUPT channel.
void InterruptChannel(int number, u16 channel_id, const void* data, u32 size)
{
  if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[number])
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))
        ->InterruptChannel(channel_id, data, size);
}

bool ButtonPressed(int number)
{
  if (s_last_connect_request_counter[number] > 0)
  {
    --s_last_connect_request_counter[number];
    if (g_wiimote_sources[number] && NetPlay::IsNetPlayRunning())
      Wiimote::NetPlay_GetButtonPress(number, false);
    return false;
  }

  bool button_pressed = false;

  if (WIIMOTE_SRC_EMU & g_wiimote_sources[number])
    button_pressed =
        static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->CheckForButtonPress();

  if (WIIMOTE_SRC_REAL & g_wiimote_sources[number])
    button_pressed = WiimoteReal::CheckForButtonPress(number);

  if (g_wiimote_sources[number] && NetPlay::IsNetPlayRunning())
    button_pressed = Wiimote::NetPlay_GetButtonPress(number, button_pressed);

  return button_pressed;
}

// This function is called periodically by the Core to update Wiimote state.
void Update(int number, bool connected)
{
  if (connected)
  {
    if (WIIMOTE_SRC_EMU & g_wiimote_sources[number])
      static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->Update();
    else
      WiimoteReal::Update(number);
  }
  else
  {
    if (ButtonPressed(number))
    {
      Connect(number, true);
      // arbitrary value so it doesn't try to send multiple requests before Dolphin can react
      // if Wii Remotes are polled at 200Hz then this results in one request being sent per 500ms
      s_last_connect_request_counter[number] = 100;
    }
  }
}

// Get a mask of attached the pads (eg: controller 1 & 4 -> 0x9)
unsigned int GetAttached()
{
  unsigned int attached = 0;
  for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
    if (g_wiimote_sources[i])
      attached |= (1 << i);
  return attached;
}

// Save/Load state
void DoState(PointerWrap& p)
{
  for (int i = 0; i < MAX_BBMOTES; ++i)
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(i))->DoState(p);
}
}
