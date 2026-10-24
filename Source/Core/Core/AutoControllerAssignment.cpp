// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/AutoControllerAssignment.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/InputConfig.h"

namespace
{
// Source and device name of the internal Bluetooth Wii Remote devices exposed by
// Dolphin's controller interface. Multiple connected Wiimotes are disambiguated by
// an incrementing id, producing qualified names like:
//   "Bluetooth/0/Wii Remote", "Bluetooth/1/Wii Remote", ...
constexpr char WIIMOTE_BLUETOOTH_SOURCE[] = "Bluetooth";
constexpr char WIIMOTE_BLUETOOTH_NAME[] = "Wii Remote";

// Profile loaded for an emulated Wii Remote slot that is driven by an actual Wiimote.
// This is the basename of the .ini file (the UI shows sys profiles with a "(Stock)"
// suffix, but the file itself does not contain it).
constexpr char WIIMOTE_STOCK_PROFILE[] = "Wii Remote with MotionPlus Pointing";

// Stock profile loaded for a regular (non-Wiimote) SDL controller when no
// controller-specific profile is found.
constexpr char SDL_STOCK_PROFILE[] = "SDL Auto Assignment Gamepad";

// Prefix used to look up a controller-specific profile, e.g.
// "SDL Auto Assignment Steam Controller.ini".
constexpr char SDL_PROFILE_PREFIX[] = "SDL Auto Assignment ";
}  // namespace

AutoControllerAssignment::AutoControllerAssignment() = default;

AutoControllerAssignment::~AutoControllerAssignment()
{
  Shutdown();
}

void AutoControllerAssignment::Initialize()
{
  // Register for device change notifications. The work is gated on the current
  // config value inside the callback so toggling the setting at runtime is honored.
  m_devices_changed_hook = g_controller_interface.RegisterDevicesChangedCallback([this] {
    if (Config::Get(Config::MAIN_AUTO_CONTROLLER_ASSIGNMENT))
      PerformAssignment();
  });

  // Register for config changes so enabling the setting at runtime immediately
  // normalizes all slots (and so an initial check happens when it flips on).
  m_config_changed_callback_id = Config::AddConfigChangedCallback([this] {
    const bool enabled = Config::Get(Config::MAIN_AUTO_CONTROLLER_ASSIGNMENT);
    const bool just_enabled = enabled && !m_was_enabled;

    // Update state first to avoid re-entering PerformAssignment(): that function
    // writes config values which re-trigger this callback.
    m_was_enabled = enabled;

    if (just_enabled)
      PerformAssignment();
  });

  m_was_enabled = Config::Get(Config::MAIN_AUTO_CONTROLLER_ASSIGNMENT);

  // Perform initial assignment with currently connected devices (normalizes any
  // pre-existing configuration, e.g. clearing slots when nothing is connected).
  if (m_was_enabled)
    PerformAssignment();
}

void AutoControllerAssignment::Shutdown()
{
  m_devices_changed_hook.reset();
  if (m_config_changed_callback_id != Config::ConfigChangedCallbackID{})
  {
    Config::RemoveConfigChangedCallback(m_config_changed_callback_id);
    m_config_changed_callback_id = {};
  }
}

void AutoControllerAssignment::PerformAssignment()
{
  const auto sdl_devices = GetConnectedSDLDevices();

  INFO_LOG_FMT(CONTROLLERINTERFACE,
               "AutoControllerAssignment: {} SDL device(s) connected, performing assignment",
               sdl_devices.size());

  // Batch all config writes so change callbacks fire once at the end rather than after
  // every SetBaseOrCurrent call.
  Config::ConfigChangeCallbackGuard config_guard;

  AssignGCPadSlots(sdl_devices);

  // Always normalize Wiimote emulation slots too. The Wii Remote source/device config
  // is not game-specific, so this must run regardless of whether a Wii game is active;
  // otherwise pre-configured slots would keep pointing at disconnected SDL devices.
  AssignWiimoteSlots(sdl_devices);

  // Persist the input configs to disk. The mapping/config UI reloads the input config
  // from the .ini when opened (e.g. Pad::LoadConfig()), so without saving, our runtime
  // SetDefaultDevice() changes would be discarded and the UI would show stale devices
  // (including "[disconnected]" entries from previously saved sessions).
  if (auto* const gc_config = Pad::GetConfig())
    gc_config->SaveConfig();
  if (auto* const wii_config = Wiimote::GetConfig())
    wii_config->SaveConfig();
}

bool AutoControllerAssignment::IsNintendoController(const std::string& device_name)
{
  // Known Nintendo controller name patterns that indicate a Wiimote or Pro Controller
  // connected via Bluetooth (these would typically be handled via BT passthrough).
  static const std::vector<std::string> nintendo_patterns = {
      "Nintendo RVL-CNT",  // Wiimote
      "Nintendo RVL-WBC",  // Balance Board
      "Wii Remote",        // Generic Wiimote name in some SDL versions
      "Nintendo Wii",      // Wii controller variants
  };

  for (const auto& pattern : nintendo_patterns)
  {
    if (device_name.find(pattern) != std::string::npos)
      return true;
  }

  return false;
}

std::string AutoControllerAssignment::ExtractDeviceName(const std::string& qualified_name)
{
  // Qualified names look like "SDL/{id}/{name}".
  const auto last_slash = qualified_name.rfind('/');
  return (last_slash != std::string::npos) ? qualified_name.substr(last_slash + 1) : qualified_name;
}

void AutoControllerAssignment::LoadProfileForController(
    InputConfig* config, ControllerEmu::EmulatedController* controller,
    const std::string& device_name, const std::string& force_device,
    const std::string& stock_profile_name)
{
  // Look for a controller-specific profile first, then fall back to the stock profile.
  // Profiles are searched in both the user and system profile directories.
  const std::string specific_profile_name = std::string(SDL_PROFILE_PREFIX) + device_name;

  const std::array<std::string, 2> profile_dirs = {config->GetUserProfileDirectoryPath(),
                                                    config->GetSysProfileDirectoryPath()};

  std::string profile_path;
  std::string loaded_profile_name;

  // Search order: controller-specific (user, then sys), then stock (user, then sys).
  for (const auto& candidate : {specific_profile_name, stock_profile_name})
  {
    for (const auto& dir : profile_dirs)
    {
      const std::string path = dir + candidate + ".ini";
      if (File::Exists(path))
      {
        profile_path = path;
        loaded_profile_name = candidate;
        break;
      }
    }
    if (!profile_path.empty())
      break;
  }

  if (profile_path.empty())
  {
    WARN_LOG_FMT(CONTROLLERINTERFACE,
                 "AutoControllerAssignment: No profile found for '{}' (tried '{}' and '{}')",
                 device_name, specific_profile_name, stock_profile_name);
    // Still point the controller at the correct device even without a profile.
    controller->SetDefaultDevice(force_device);
    controller->UpdateReferences(g_controller_interface);
    return;
  }

  Common::IniFile ini;
  if (!ini.Load(profile_path))
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "AutoControllerAssignment: Failed to load profile '{}'",
                  profile_path);
    controller->SetDefaultDevice(force_device);
    controller->UpdateReferences(g_controller_interface);
    return;
  }

  INFO_LOG_FMT(CONTROLLERINTERFACE,
               "AutoControllerAssignment: Loading profile '{}' for device '{}', forcing device '{}'",
               loaded_profile_name, device_name, force_device);

  controller->LoadConfig(ini.GetOrCreateSection("Profile"));

  // Override whatever "Device" the profile may contain so the mapping always
  // points at the auto-assigned device. This is what makes the stock profile
  // usable for any controller regardless of the Device line saved inside it.
  controller->SetDefaultDevice(force_device);
  controller->UpdateReferences(g_controller_interface);
  config->GenerateControllerTextures(ini);
}

std::vector<std::string> AutoControllerAssignment::GetConnectedSDLDevices() const
{
  // Collect SDL devices in the controller interface's own list order. SDL devices all
  // share the same sort priority, so the interface keeps them in add order, which is
  // the connection order (a reconnected controller is appended at the end). Note the
  // SDL id in the qualified name is only a per-name disambiguator (e.g. two Xbox pads
  // are "SDL/0/Xbox..." and "SDL/1/Xbox..."), so it must NOT be used for ordering.
  std::vector<std::string> sdl_devices;

  const auto all_devices = g_controller_interface.GetAllDevices();
  for (const auto& device : all_devices)
  {
    if (device->GetSource() == "SDL")
      sdl_devices.push_back(device->GetQualifiedName());
  }

  return sdl_devices;
}

std::vector<std::string> AutoControllerAssignment::GetConnectedBluetoothWiimotes() const
{
  // Qualified names of the live internal Bluetooth Wii Remote devices, ordered by id.
  // These ids are NOT kept in sync with the SDL device ids: the SDL devices get renumbered
  // whenever the SDL backend re-enumerates (ids are reassigned from zero), while the
  // Bluetooth devices keep theirs. So the Bluetooth device must be looked up from the live
  // list rather than derived from an SDL id.
  std::vector<std::pair<int, std::string>> wiimotes;

  for (const auto& device : g_controller_interface.GetAllDevices())
  {
    if (device->GetSource() == WIIMOTE_BLUETOOTH_SOURCE &&
        device->GetName() == WIIMOTE_BLUETOOTH_NAME)
    {
      wiimotes.emplace_back(device->GetId(), device->GetQualifiedName());
    }
  }

  std::ranges::sort(wiimotes, {}, &std::pair<int, std::string>::first);

  std::vector<std::string> result;
  result.reserve(wiimotes.size());
  for (auto& [id, name] : wiimotes)
    result.push_back(std::move(name));

  return result;
}

void AutoControllerAssignment::AssignGCPadSlots(const std::vector<std::string>& sdl_devices)
{
  InputConfig* gc_config = Pad::GetConfig();
  if (!gc_config)
    return;

  const int num_slots = gc_config->GetControllerCount();
  const int num_devices = static_cast<int>(sdl_devices.size());

  // GCPad ports use a 1:1 positional mapping with the connection order (matching the
  // Wiimote track). A non-Wiimote SDL controller at connection position N becomes a
  // Standard Controller on GCPad port N. Wiimote positions (and empty positions) are
  // set to None so port indices stay aligned between the GCPad and Wiimote tracks.
  //
  // Every slot is set unconditionally on every pass so the result depends only on the
  // current device list, never on prior state. A single physical hotplug fires several
  // device-change events with inconsistent intermediate lists; making each pass fully
  // idempotent guarantees the final (settled) pass produces the correct configuration.
  for (int slot = 0; slot < num_slots; ++slot)
  {
    auto* controller = gc_config->GetController(slot);
    if (!controller)
      continue;

    const bool has_device = slot < num_devices;
    const std::string device_string = has_device ? sdl_devices[slot] : std::string{};
    const std::string device_name = has_device ? ExtractDeviceName(device_string) : std::string{};
    const bool is_wiimote = has_device && IsNintendoController(device_name);

    if (has_device && !is_wiimote)
    {
      // Non-Wiimote SDL controller — activate the port as a Standard Controller and
      // (re)assign the device + profile.
      INFO_LOG_FMT(CONTROLLERINTERFACE,
                   "AutoControllerAssignment: GCPad slot {} -> '{}' (Standard Controller)",
                   slot + 1, device_string);
      Config::SetBaseOrCurrent(Config::GetInfoForSIDevice(slot),
                               SerialInterface::SIDEVICE_GC_CONTROLLER);
      LoadProfileForController(gc_config, controller, device_name, device_string, "SDL Gamepad");
    }
    else
    {
      // Wiimote position or no device — port inactive (None), device cleared. Always
      // clear so stale device strings never linger and show as "[disconnected]".
      Config::SetBaseOrCurrent(Config::GetInfoForSIDevice(slot), SerialInterface::SIDEVICE_NONE);
      controller->SetDefaultDevice("");
      controller->UpdateReferences(g_controller_interface);
    }
  }
}

void AutoControllerAssignment::AssignWiimoteSlots(const std::vector<std::string>& sdl_devices)
{
  InputConfig* wii_config = Wiimote::GetConfig();
  if (!wii_config)
    return;

  // Bluetooth passthrough routes everything to the real adapter and bypasses per-slot
  // emulated Wii Remote config, so there is nothing for us to manage in that mode.
  // Leave the user's setting untouched and skip.
  if (Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED))
    return;

  const int num_slots = std::min(wii_config->GetControllerCount(), static_cast<int>(MAX_WIIMOTES));
  const int num_devices = static_cast<int>(sdl_devices.size());

  // Every slot is set unconditionally on every pass (idempotent) — see the note in
  // AssignGCPadSlots for why.
  //
  // Live Bluetooth Wii Remote devices, in id order. The Nth detected Wiimote is mapped to
  // the Nth entry here.
  const std::vector<std::string> bluetooth_wiimotes = GetConnectedBluetoothWiimotes();
  int wiimote_ordinal = 0;

  for (int slot = 0; slot < num_slots; ++slot)
  {
    auto* controller = wii_config->GetController(slot);
    if (!controller)
      continue;

    if (slot < num_devices)
    {
      const auto& device_string = sdl_devices[slot];
      const std::string device_name = ExtractDeviceName(device_string);

      if (IsNintendoController(device_name))
      {
        // A real Wiimote was detected through SDL. Set the slot to an Emulated Wii
        // Remote pointing at Dolphin's internal Bluetooth device (not "Real Wii
        // Remote") and load the stock Wiimote profile, keeping native gyro/pointer.
        //
        // Map the Nth detected Wiimote to the Nth live Bluetooth Wii Remote device. The
        // Bluetooth name must NOT be derived from the SDL device's id: SDL devices are
        // renumbered when the SDL backend re-enumerates, while Bluetooth ids are not, so
        // the two drift apart (e.g. the survivor of a two-remote disconnect becomes
        // SDL/0 while still being Bluetooth/1).
        std::string bluetooth_device;
        if (wiimote_ordinal < static_cast<int>(bluetooth_wiimotes.size()))
        {
          bluetooth_device = bluetooth_wiimotes[wiimote_ordinal];
        }
        else
        {
          // The Bluetooth device hasn't been enumerated yet (can happen transiently right
          // after a remote connects). Fall back to the ordinal-derived name; a later pass
          // corrects it once the device appears.
          bluetooth_device = fmt::format("{}/{}/{}", WIIMOTE_BLUETOOTH_SOURCE, wiimote_ordinal,
                                         WIIMOTE_BLUETOOTH_NAME);
        }
        ++wiimote_ordinal;

        INFO_LOG_FMT(CONTROLLERINTERFACE,
                     "AutoControllerAssignment: Wiimote slot {} -> '{}' (Wiimote '{}')", slot + 1,
                     bluetooth_device, device_name);
        Config::SetBaseOrCurrent(Config::GetInfoForWiimoteSource(slot), WiimoteSource::Emulated);
        LoadProfileForController(wii_config, controller, device_name, bluetooth_device,
                                 WIIMOTE_STOCK_PROFILE);
      }
      else
      {
        // Regular SDL controller — Emulated Wii Remote driven by the SDL device,
        // enabling gyro-capable controllers to use SDL's motion sensors.
        INFO_LOG_FMT(CONTROLLERINTERFACE,
                     "AutoControllerAssignment: Wiimote slot {} -> '{}'", slot + 1, device_string);
        Config::SetBaseOrCurrent(Config::GetInfoForWiimoteSource(slot), WiimoteSource::Emulated);
        LoadProfileForController(wii_config, controller, device_name, device_string,
                                 SDL_STOCK_PROFILE);
      }
    }
    else
    {
      // No device for this slot — set to None and clear the device unconditionally so
      // stale device strings never linger and show as "[disconnected]".
      Config::SetBaseOrCurrent(Config::GetInfoForWiimoteSource(slot), WiimoteSource::None);
      controller->SetDefaultDevice("");
      controller->UpdateReferences(g_controller_interface);
    }
  }
}
