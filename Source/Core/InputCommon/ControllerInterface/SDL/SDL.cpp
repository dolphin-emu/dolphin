// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/SDL/SDL.h"

#include <map>
#include <span>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include <SDL3/SDL.h>

#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/Thread.h"

#include "Core/Config/MainSettings.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/SDL/SDLGamepad.h"

namespace ciface::SDL
{

class InputBackend final : public ciface::InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);
  ~InputBackend() override;
  void PopulateDevices() override;
  void UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>& devices_to_remove) override;

private:
  void OpenAndAddDevice(SDL_JoystickID instance_id);

  bool HandleEventAndContinue(const SDL_Event& e);

  // Removes all SDL devices and re-adds them from the current enumeration.
  void ReenumerateDevices();

  // True if the given joystick is currently Steam Input's generic virtual-gamepad
  // placeholder rather than a real controller.
  static bool IsSteamVirtualPlaceholder(SDL_JoystickID instance_id);

  // Closes and forgets a placeholder joystick we were keeping open, if any.
  void StopWatchingPlaceholder(SDL_JoystickID instance_id);

  Common::Event m_init_event;
  Uint32 m_stop_event_type;
  Uint32 m_populate_event_type;
  std::thread m_hotplug_thread;

  // Tracks, per joystick, whether it was last seen as Steam's placeholder. Steam swaps a
  // controller's identity in place (same instance id) without emitting an add/remove
  // event, so we watch for that bit flipping. See the UPDATE_COMPLETE handler.
  std::map<SDL_JoystickID, bool> m_placeholder_state;

  // Placeholder joysticks we keep open (but unregistered) purely so SDL keeps polling them
  // and emitting per-device events, letting us notice when they become real again.
  std::map<SDL_JoystickID, SDL_Joystick*> m_watched_placeholders;
};

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

static void EnableSDLLogging()
{
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
  SDL_SetLogOutputFunction(
      [](void*, int category, SDL_LogPriority priority, const char* message) {
        std::string_view category_name{};
        switch (category)
        {
        case SDL_LOG_CATEGORY_APPLICATION:
          category_name = "app";
          break;
        case SDL_LOG_CATEGORY_ERROR:
          category_name = "error";
          break;
        case SDL_LOG_CATEGORY_ASSERT:
          category_name = "assert";
          break;
        case SDL_LOG_CATEGORY_SYSTEM:
          category_name = "system";
          break;
        case SDL_LOG_CATEGORY_AUDIO:
          category_name = "audio";
          break;
        case SDL_LOG_CATEGORY_VIDEO:
          category_name = "video";
          break;
        case SDL_LOG_CATEGORY_RENDER:
          category_name = "render";
          break;
        case SDL_LOG_CATEGORY_INPUT:
          category_name = "input";
          break;
        case SDL_LOG_CATEGORY_TEST:
          category_name = "test";
          break;
        case SDL_LOG_CATEGORY_GPU:
          category_name = "gpu";
          break;
        default:
          break;
        }

        auto log_level = Common::Log::LogLevel::LNOTICE;
        switch (priority)
        {
        case SDL_LOG_PRIORITY_VERBOSE:
        case SDL_LOG_PRIORITY_DEBUG:
          log_level = Common::Log::LogLevel::LDEBUG;
          break;
        case SDL_LOG_PRIORITY_INFO:
          log_level = Common::Log::LogLevel::LINFO;
          break;
        case SDL_LOG_PRIORITY_WARN:
          log_level = Common::Log::LogLevel::LWARNING;
          break;
        case SDL_LOG_PRIORITY_ERROR:
          log_level = Common::Log::LogLevel::LERROR;
          break;
        case SDL_LOG_PRIORITY_CRITICAL:
        default:
          log_level = Common::Log::LogLevel::LNOTICE;
          break;
        }

        if (category_name.empty())
        {
          GENERIC_LOG_FMT(Common::Log::LogType::CONTROLLERINTERFACE, log_level, "unknown({}): {}",
                          category, message);
        }
        else
        {
          GENERIC_LOG_FMT(Common::Log::LogType::CONTROLLERINTERFACE, log_level, "{}: {}",
                          category_name, message);
        }
      },
      nullptr);
}

InputBackend::InputBackend(ControllerInterface* controller_interface)
    : ciface::InputBackend(controller_interface)
{
  EnableSDLLogging();

  if (Config::Get(Config::MAIN_SDL_HINT_JOYSTICK_ENHANCED_REPORTS) == "")
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");

  // We have our own WGI backend. Enabling SDL's WGI handling creates even more redundant devices.
  if (Config::Get(Config::MAIN_SDL_HINT_JOYSTICK_WGI) == "")
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_WGI, "0");

  // Disable DualSense Player LEDs; We already colorize the Primary LED
  if (Config::Get(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED) == "")
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, "0");

  // Disabling DirectInput support apparently solves hangs on shutdown for users with
  //  "8BitDo Ultimate 2" controllers, however, it also breaks hotplug support for Dual Sense
  //  and DS4 Controllers, so we leave it enabled for now.
  // It also works around a possibly related random hang on a IDirectInputDevice8_Acquire
  //  call within SDL.
  if (Config::Get(Config::MAIN_SDL_HINT_JOYSTICK_DIRECTINPUT) == "")
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_DIRECTINPUT, "1");

  // Pre-populate the default Joy-Con hints so they can be easily changed
  if (Config::Get(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS) == "")
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS, "1");

  if (Config::Get(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS) == "")
    Config::SetBase(Config::MAIN_SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS, "0");

  // Disable SDL's GC Adapter handling when we want to handle it ourselves.
  bool is_gc_adapter_configured = false;
  for (int i = 0; i != SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    if (Config::Get(Config::GetInfoForSIDevice(i)) == SerialInterface::SIDEVICE_WIIU_ADAPTER)
    {
      is_gc_adapter_configured = true;
      break;
    }
  }
  // TODO: This hint should be adjusted when the config changes,
  //  but SDL requires it be set before joystick initialization,
  //  and ControllerInterface isn't prepared for SDL to spontaneously re-initialize itself.
  SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE, is_gc_adapter_configured ? "0" : "1");

  // Load all the hints from the config file
  std::shared_ptr<Config::Layer> layer = Config::GetLayer(Config::LayerType::Base);
  const Config::Section& section = layer->GetSection(Config::System::Main, "SDL_Hints");
  for (auto& row_data : section)
  {
    const Config::Location& location = row_data.first;
    const std::optional<std::string>& value = row_data.second;

    if (value)
      SDL_SetHint(location.key.c_str(), value->c_str());
  }

  m_hotplug_thread = std::thread([this] {
    Common::SetCurrentThreadName("SDL Hotplug Thread");

    Common::ScopeGuard quit_guard([] {
      // TODO: there seems to be some sort of memory leak with SDL, quit isn't freeing everything up
      SDL_Quit();
    });
    {
      Common::ScopeGuard init_guard([this] { m_init_event.Set(); });

      if (!SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD))
      {
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "SDL failed to initialize");
        return;
      }

      const Uint32 custom_events_start = SDL_RegisterEvents(2);
      if (custom_events_start == static_cast<Uint32>(-1))
      {
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "SDL failed to register custom events");
        return;
      }
      m_stop_event_type = custom_events_start;
      m_populate_event_type = custom_events_start + 1;

      // Drain all of the events and add the initial joysticks before returning. Otherwise, the
      // individual joystick events as well as the custom populate event will be handled _after_
      // ControllerInterface::Init/RefreshDevices has cleared its list of devices, resulting in
      // duplicate devices. Adding devices will actually "fail" here, as the ControllerInterface
      // hasn't finished initializing yet.
      SDL_Event e;
      while (SDL_PollEvent(&e))
      {
        if (!HandleEventAndContinue(e))
          return;
      }
    }

    SDL_Event e;
    while (SDL_WaitEvent(&e))
    {
      if (!HandleEventAndContinue(e))
        return;
    }
  });

  m_init_event.Wait();
}

InputBackend::~InputBackend()
{
  if (!m_hotplug_thread.joinable())
    return;

  SDL_Event stop_event{m_stop_event_type};
  SDL_PushEvent(&stop_event);

  m_hotplug_thread.join();
}

void InputBackend::PopulateDevices()
{
  if (!m_hotplug_thread.joinable())
    return;

  SDL_Event populate_event{m_populate_event_type};
  SDL_PushEvent(&populate_event);
}

void InputBackend::UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>&)
{
  SDL_UpdateGamepads();
}

bool InputBackend::IsSteamVirtualPlaceholder(SDL_JoystickID instance_id)
{
  // When Steam Input assigns a controller to a slot it does not forward to the game, it
  // replaces the real device with this generic placeholder: the entry stays enumerated
  // (keeping the same instance id and XInput path) but reports Valve's generic VID/PID and
  // produces no usable input.
  constexpr Uint16 STEAM_VIRTUAL_GAMEPAD_VID = 0x28de;
  constexpr Uint16 STEAM_VIRTUAL_GAMEPAD_PID = 0x11ff;
  return SDL_GetJoystickVendorForID(instance_id) == STEAM_VIRTUAL_GAMEPAD_VID &&
         SDL_GetJoystickProductForID(instance_id) == STEAM_VIRTUAL_GAMEPAD_PID;
}

void InputBackend::StopWatchingPlaceholder(SDL_JoystickID instance_id)
{
  const auto it = m_watched_placeholders.find(instance_id);
  if (it == m_watched_placeholders.end())
    return;

  SDL_CloseJoystick(it->second);
  m_watched_placeholders.erase(it);
}

void InputBackend::OpenAndAddDevice(SDL_JoystickID instance_id)
{
  if (IsSteamVirtualPlaceholder(instance_id))
  {
    // Don't expose Steam's placeholder as a controller; it would leave a dead device mapped
    // to a player slot. We do however keep the joystick *open* (without registering it):
    // SDL only polls opened joysticks, and we rely on its per-device
    // SDL_EVENT_JOYSTICK_UPDATE_COMPLETE to notice when Steam swaps the identity back to a
    // real controller (that transition is otherwise silent).
    if (!m_watched_placeholders.contains(instance_id))
    {
      if (SDL_Joystick* const js = SDL_OpenJoystick(instance_id))
      {
        m_watched_placeholders.emplace(instance_id, js);
        INFO_LOG_FMT(CONTROLLERINTERFACE,
                     "SDL: Ignoring Steam Input virtual gamepad placeholder (watching id {})",
                     static_cast<int>(instance_id));
      }
    }
    return;
  }

  // A real controller: stop watching it as a placeholder, if we were.
  StopWatchingPlaceholder(instance_id);

  SDL_Gamepad* gc = SDL_OpenGamepad(instance_id);
  SDL_Joystick* js = SDL_OpenJoystick(instance_id);

  if (js != nullptr)
  {
    if (SDL_GetNumJoystickButtons(js) > 255 || SDL_GetNumJoystickAxes(js) > 255 ||
        SDL_GetNumJoystickHats(js) > 255 || SDL_GetNumJoystickBalls(js) > 255)
    {
      // This device is invalid, don't use it
      // Some crazy devices (HP webcam 2100) end up as HID devices
      // SDL tries parsing these as Joysticks
      return;
    }
    auto gamepad = std::make_shared<Gamepad>(gc, js);
    if (!gamepad->Inputs().empty() || !gamepad->Outputs().empty())
      GetControllerInterface().AddDevice(std::move(gamepad));
  }
}

bool InputBackend::HandleEventAndContinue(const SDL_Event& e)
{
  if (e.type == SDL_EVENT_JOYSTICK_ADDED)
  {
    m_placeholder_state[e.jdevice.which] = IsSteamVirtualPlaceholder(e.jdevice.which);
    OpenAndAddDevice(e.jdevice.which);
  }
  else if (e.type == SDL_EVENT_JOYSTICK_REMOVED)
  {
    m_placeholder_state.erase(e.jdevice.which);
    StopWatchingPlaceholder(e.jdevice.which);
    GetControllerInterface().RemoveDevice([&e](const auto* device) {
      return device->GetSource() == "SDL" &&
             static_cast<const Gamepad*>(device)->GetSDLInstanceID() == e.jdevice.which;
    });
  }
  else if (e.type == SDL_EVENT_JOYSTICK_UPDATE_COMPLETE)
  {
    // Steam Input swaps a controller's identity in place when it moves the controller to or
    // from a slot it doesn't forward: the instance id stays the same while the device becomes
    // (or stops being) Steam's placeholder gamepad. SDL emits no add/remove event for that,
    // and STEAM_HANDLE_UPDATED fires before the identity settles, so we watch for the
    // placeholder bit flipping on this per-device update event and re-enumerate then.
    const auto instance_id = e.jdevice.which;
    const bool is_placeholder = IsSteamVirtualPlaceholder(instance_id);
    const auto it = m_placeholder_state.find(instance_id);

    if (it == m_placeholder_state.end())
    {
      m_placeholder_state.emplace(instance_id, is_placeholder);
    }
    else if (it->second != is_placeholder)
    {
      it->second = is_placeholder;
      INFO_LOG_FMT(CONTROLLERINTERFACE,
                   "SDL: Joystick {} identity changed (now {}); re-enumerating",
                   static_cast<int>(instance_id), is_placeholder ? "placeholder" : "real");
      ReenumerateDevices();
    }
  }
  else if (e.type == m_populate_event_type)
  {
    GetControllerInterface().PlatformPopulateDevices([this] {
      int joystick_count = 0;
      auto* const joystick_ids = SDL_GetJoysticks(&joystick_count);
      for (auto instance_id : std::span(joystick_ids, joystick_count))
        OpenAndAddDevice(instance_id);

      SDL_free(joystick_ids);
    });
  }
  else if (e.type == SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED)
  {
    // Steam Input fires this when the user reorders controllers via Steam (it does not fire
    // ADDED/REMOVED). Re-enumerate so the new order is picked up promptly. A pure reorder is
    // reflected in SDL's enumeration immediately; slot changes that swap a device's identity
    // are handled by the UPDATE_COMPLETE case above, since this event fires before that
    // identity settles.
    ReenumerateDevices();
  }
  else if (e.type == m_stop_event_type)
  {
    // Release any placeholder joysticks we were keeping open.
    for (auto& [instance_id, js] : m_watched_placeholders)
      SDL_CloseJoystick(js);
    m_watched_placeholders.clear();

    return false;
  }

  return true;
}

void InputBackend::ReenumerateDevices()
{
  // Remove the existing SDL devices and re-add them from the current enumeration within a
  // single populate transaction, so the DevicesChanged callback fires once with the updated
  // set and ordering.
  GetControllerInterface().PlatformPopulateDevices([this] {
    GetControllerInterface().RemoveDevice(
        [](const auto* device) { return device->GetSource() == "SDL"; });

    int joystick_count = 0;
    auto* const joystick_ids = SDL_GetJoysticks(&joystick_count);
    for (const auto instance_id : std::span(joystick_ids, joystick_count))
      OpenAndAddDevice(instance_id);

    SDL_free(joystick_ids);
  });
}

}  // namespace ciface::SDL
