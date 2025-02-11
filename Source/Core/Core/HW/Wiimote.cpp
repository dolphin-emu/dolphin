// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Wiimote.h"

#include <optional>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/WiimoteDevice.h"
#include "Core/Movie.h"
#include "Core/System.h"
#include "Core/WiiUtils.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/InputConfig.h"

namespace
{
static std::array<std::atomic<WiimoteSource>, MAX_BBMOTES> s_wiimote_sources;
static std::optional<Config::ConfigChangedCallbackID> s_config_callback_id = std::nullopt;

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

  WiimoteReal::HandleWiimoteSourceChange(index);

  const Core::CPUThreadGuard guard(Core::System::GetInstance());
  WiimoteCommon::UpdateSource(index);
}

void RefreshConfig()
{
  for (int i = 0; i < MAX_BBMOTES; ++i)
    OnSourceChanged(i, Config::Get(Config::GetInfoForWiimoteSource(i)));
}

void DoWiimoteSlotState(PointerWrap& p, int slot, ControllerEmu::EmulatedController* controller)
{
  const WiimoteSource source = GetSource(slot);
  auto state_wiimote_source = u8(source);
  p.Do(state_wiimote_source);

  if (WiimoteSource(state_wiimote_source) == WiimoteSource::Emulated)
  {
    // Sync complete state of emulated wiimotes.
    static_cast<WiimoteEmu::WiimoteBase*>(controller)->DoState(p);
  }

  if (p.IsReadMode())
  {
    // If using a real wiimote or the save-state source does not match the current source,
    // then force a reconnection on load.
    if (source == WiimoteSource::Real || source != WiimoteSource(state_wiimote_source))
      WiimoteCommon::UpdateSource(slot);
  }
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

  switch (GetSource(index))
  {
  case WiimoteSource::Emulated:
    hid_source = static_cast<WiimoteEmu::WiimoteBase*>(
        (index == WIIMOTE_BALANCE_BOARD) ? ::BalanceBoard::GetConfig()->GetController(0) :
                                           ::Wiimote::GetConfig()->GetController(index));
    break;

  case WiimoteSource::Real:
    hid_source = WiimoteReal::g_wiimotes[index].get();
    break;

  default:
    break;
  }

  return hid_source;
}

}  // namespace WiimoteCommon

namespace
{
static InputConfig s_config(WIIMOTE_INI_NAME, _trans("Wii Remote"), "Wiimote", "Wiimote");
static InputConfig s_bb_config(WIIMOTE_INI_NAME, _trans("Balance Board"), "BalanceBoard",
                               "BalanceBoard");
}  // namespace

namespace Wiimote
{
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

ControllerEmu::ControlGroup* GetShinkansenGroup(int number, WiimoteEmu::ShinkansenGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))
      ->GetShinkansenGroup(group);
}

void Shutdown()
{
  s_bb_config.UnregisterHotplugCallback();
  s_config.UnregisterHotplugCallback();

  s_bb_config.ClearControllers();
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
    for (unsigned int i = WIIMOTE_CHAN_0; i < MAX_WIIMOTES; ++i)
      s_config.CreateController<WiimoteEmu::Wiimote>(i);

    s_bb_config.CreateController<WiimoteEmu::BalanceBoard>(WIIMOTE_BALANCE_BOARD);
  }

  s_config.RegisterHotplugCallback();
  s_bb_config.RegisterHotplugCallback();

  LoadConfig();
  BalanceBoard::LoadConfig();

  if (!s_config_callback_id)
    s_config_callback_id = Config::AddConfigChangedCallback(RefreshConfig);
  RefreshConfig();

  WiimoteReal::Initialize(init_mode);

  // Reload Wiimotes with our settings
  auto& movie = Core::System::GetInstance().GetMovie();
  if (movie.IsMovieActive())
    movie.ChangeWiiPads();
}

void ResetAllWiimotes()
{
  for (int i = WIIMOTE_CHAN_0; i < MAX_WIIMOTES; ++i)
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(i))->Reset();

  static_cast<WiimoteEmu::BalanceBoard*>(s_bb_config.GetController(0))->Reset();
}

void LoadConfig()
{
  s_config.LoadConfig();
}

void GenerateDynamicInputTextures()
{
  s_config.GenerateControllerTextures();
}

void DoState(PointerWrap& p)
{
  for (int slot = 0; slot < MAX_WIIMOTES; ++slot)
    DoWiimoteSlotState(p, slot, s_config.GetController(slot));

  DoWiimoteSlotState(p, WIIMOTE_BALANCE_BOARD, s_bb_config.GetController(0));
}
}  // namespace Wiimote

namespace BalanceBoard
{
InputConfig* GetConfig()
{
  return &s_bb_config;
}

void LoadConfig()
{
  s_bb_config.LoadConfig();
}

ControllerEmu::ControlGroup* GetBalanceBoardGroup(int number, WiimoteEmu::BalanceBoardGroup group)
{
  return static_cast<WiimoteEmu::BalanceBoard*>(s_bb_config.GetController(number))
      ->GetBalanceBoardGroup(group);
}
}  // namespace BalanceBoard
