// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Wiimote.h"

#include <optional>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Core.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/WiimoteDevice.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/WiiUtils.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/InputConfig.h"

#include "Core/PrimeHack/HackConfig.h"

// Limit the amount of wiimote connect requests, when a button is pressed in disconnected state
static std::array<u8, MAX_BBMOTES> s_last_connect_request_counter;

namespace
{
static std::array<std::atomic<WiimoteSource>, MAX_BBMOTES> s_wiimote_sources;
static std::optional<size_t> s_config_callback_id = std::nullopt;

WiimoteSource GetSource(unsigned int index)
{
  return s_wiimote_sources[index];
}

void OnSourceChanged(unsigned int index, WiimoteSource source)
{
  const WiimoteSource previous_source = s_wiimote_sources[index].exchange(source);

  if (previous_source == source)
  {
    // No change. Do nothing.
    return;
  }

  if (index == 0) {
    Config::SetBaseOrCurrent(Config::PRIMEHACK_ENABLE, source == WiimoteSource::Real ? false : true);
  }

  WiimoteReal::HandleWiimoteSourceChange(index);

  Core::RunAsCPUThread([index] { WiimoteCommon::UpdateSource(index); });
}

void RefreshConfig()
{
  for (int i = 0; i < MAX_BBMOTES; ++i)
    OnSourceChanged(i, Config::Get(Config::GetInfoForWiimoteSource(i)));
}

}  // namespace

namespace WiimoteCommon
{
void UpdateSource(unsigned int index)
{
  const auto bluetooth = WiiUtils::GetBluetoothEmuDevice();
  if (bluetooth == nullptr)
    return;

  bluetooth->AccessWiimoteByIndex(index)->SetSource(GetHIDWiimoteSource(index));
}

HIDWiimote* GetHIDWiimoteSource(unsigned int index)
{
  HIDWiimote* hid_source = nullptr;

  WiimoteSource src = GetSource(index);
  switch (src)
  {
  case WiimoteSource::Emulated:
  case WiimoteSource::Metroid:
    hid_source = static_cast<WiimoteEmu::Wiimote*>(::Wiimote::GetConfig()->GetController(index));
    break;

  case WiimoteSource::Real:
    hid_source = WiimoteReal::g_wiimotes[index].get();
    break;

  default:
    break;
  }

  Wiimote::ChangeUIPrimeHack(index, src == WiimoteSource::Metroid);

  return hid_source;
}

}  // namespace WiimoteCommon

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

ControllerEmu::ControlGroup* GetUDrawTabletGroup(int number, WiimoteEmu::UDrawTabletGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))
      ->GetUDrawTabletGroup(group);
}

ControllerEmu::ControlGroup* GetDrawsomeTabletGroup(int number,
                                                    WiimoteEmu::DrawsomeTabletGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))
      ->GetDrawsomeTabletGroup(group);
}

ControllerEmu::ControlGroup* GetTaTaConGroup(int number, WiimoteEmu::TaTaConGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetTaTaConGroup(group);
}

void Shutdown()
{
  s_config.UnregisterHotplugCallback();

  s_config.ClearControllers();

  WiimoteReal::Stop();

  if (s_config_callback_id)
  {
    Config::RemoveConfigChangedCallback(*s_config_callback_id);
    s_config_callback_id = std::nullopt;
  }
}

void Initialize(InitializeMode init_mode)
{
  if (s_config.ControllersNeedToBeCreated())
  {
    for (unsigned int i = WIIMOTE_CHAN_0; i < MAX_BBMOTES; ++i)
      s_config.CreateController<WiimoteEmu::Wiimote>(i);
  }

  s_config.RegisterHotplugCallback();

  LoadConfig();

  if (!s_config_callback_id)
    s_config_callback_id = Config::AddConfigChangedCallback(RefreshConfig);
  RefreshConfig();

  WiimoteReal::Initialize(init_mode);

  // Reload Wiimotes with our settings
  if (Movie::IsMovieActive())
    Movie::ChangeWiiPads();
}

void ResetAllWiimotes()
{
  for (int i = WIIMOTE_CHAN_0; i < MAX_BBMOTES; ++i)
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(i))->Reset();
}

void LoadConfig()
{
  s_config.LoadConfig(InputConfig::InputClass::Wii);
  s_last_connect_request_counter.fill(0);

  prime::UpdateHackSettings();
}

void Resume()
{
  WiimoteReal::Resume();
}

void Pause()
{
  WiimoteReal::Pause();
}

void ChangeUIPrimeHack(int number, bool useMetroidUI)
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number));

  wiimote->ChangeUIPrimeHack(useMetroidUI);
  wiimote->GetNunchuk()->ChangeUIPrimeHack(useMetroidUI);
}

bool CheckVisor(int visorcount)
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->CheckVisorCtrl(visorcount);
}

bool CheckBeam(int beamcount)
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->CheckBeamCtrl(beamcount);
}

bool CheckForward() {
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->GetNunchukGroup(WiimoteEmu::NunchukGroup::Stick)->
      controls[0].get()->control_ref->State() > 0.5;
}

bool CheckBack() {
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->GetNunchukGroup(WiimoteEmu::NunchukGroup::Stick)->
      controls[1].get()->control_ref->State() > 0.5;
}

bool CheckLeft() {
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->GetNunchukGroup(WiimoteEmu::NunchukGroup::Stick)->
      controls[2].get()->control_ref->State() > 0.5;
}

bool CheckRight() {
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->GetNunchukGroup(WiimoteEmu::NunchukGroup::Stick)->
      controls[3].get()->control_ref->State() > 0.5;
}

bool CheckJump() {
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->groups[0].get()->controls[1]->control_ref->State() > 0.5;
}

bool CheckGrapple() {
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->CheckGrappleCtrl();
}

bool UseGrappleTapping() {
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->CheckUseGrappleTapping();
}

bool GrappleCtlBound() {
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->IsGrappleBinded();
}

bool CheckSpringBall()
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->CheckSpringBallCtrl();
}

std::tuple<bool, bool> GetBVMenuOptions()
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->GetBVMenuOptions();
}

bool CheckImprovedMotions()
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->CheckImprovedMotions();
}

bool CheckVisorScroll(bool direction)
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->CheckVisorScrollCtrl(direction);
}

bool CheckBeamScroll(bool direction)
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->CheckBeamScrollCtrl(direction);
}

bool PrimeUseController()
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->PrimeControllerMode();
}

std::tuple<double, double> GetPrimeStickXY()
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->GetPrimeStickXY();
}

std::tuple<double, double, bool, bool, bool, bool, bool> PrimeSettings()
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->GetPrimeSettings();
}

bool CheckPitchRecentre()
{
  WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(0));

  return wiimote->CheckPitchRecentre();
}

void DoState(PointerWrap& p)
{
  for (int i = 0; i < MAX_BBMOTES; ++i)
  {
    const WiimoteSource source = GetSource(i);
    auto state_wiimote_source = u8(source);
    p.Do(state_wiimote_source);

    if (WiimoteSource(state_wiimote_source) == WiimoteSource::Emulated || WiimoteSource(state_wiimote_source) == WiimoteSource::Metroid)
    {
      // Sync complete state of emulated wiimotes.
      static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(i))->DoState(p);
    }

    if (p.IsReadMode())
    {
      // If using a real wiimote or the save-state source does not match the current source,
      // then force a reconnection on load.
      if (source == WiimoteSource::Real || source != WiimoteSource(state_wiimote_source))
        WiimoteCommon::UpdateSource(i);
    }
  }
}
}  // namespace Wiimote
