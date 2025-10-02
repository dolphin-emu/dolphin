// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/SDL/SDL.h"

#include <optional>
#include <span>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <SDL3/SDL.h>

#include "Common/Event.h"
#include "Common/Keyboard.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Core/Host.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/SDL/SDLGamepad.h"

namespace
{
using UniqueSDLWindow = std::unique_ptr<SDL_Window, void (*)(SDL_Window*)>;

// Based on sdl2-compat 76eb981a4c376bcaf615c0af37d46512ba45cfb8
SDL_Window* SDL_CreateWindowFrom(void* handle)
{
  SDL_PropertiesID props = SDL_CreateProperties();
  if (!props)
    return nullptr;

  if (const char* hint = SDL_GetHint("SDL_VIDEO_WINDOW_SHARE_PIXEL_FORMAT"); hint)
  {
    // This hint is a pointer (in string form) of the address of
    // the window to share a pixel format with
    SDL_Window* other_window = nullptr;
    if (SDL_sscanf(hint, "%p", &other_window))
    {
      void* ptr = SDL_GetPointerProperty(SDL_GetWindowProperties(other_window),
                                         SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
      SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_PIXEL_FORMAT_HWND_POINTER, ptr);
      SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    }
  }

  if (SDL_GetHintBoolean("SDL_VIDEO_FOREIGN_WINDOW_OPENGL", false))
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);

  if (SDL_GetHintBoolean("SDL_VIDEO_FOREIGN_WINDOW_VULKAN", false))
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);

  SDL_SetPointerProperty(props, "sdl2-compat.external_window", handle);
  SDL_Window* window = SDL_CreateWindowWithProperties(props);
  SDL_DestroyProperties(props);

  // SDL3 has per-window text input, so we must enable on this window if it's active
  if (SDL_EventEnabled(SDL_EVENT_TEXT_INPUT))
  {
    props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, SDL_CAPITALIZE_NONE);
    SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, false);
    SDL_StartTextInputWithProperties(window, props);
    SDL_DestroyProperties(props);
  }

  return window;
}

std::optional<const char*> UpdateKeyboardHandle(UniqueSDLWindow* unique_window)
{
  std::optional<const char*> error;

  void* keyboard_handle = Common::KeyboardContext::GetWindowHandle();
  SDL_Window* keyboard_window = SDL_CreateWindowFrom(keyboard_handle);
  if (keyboard_window == nullptr)
    error = SDL_GetError();

  unique_window->reset(keyboard_window);
  if (error.has_value())
    return error;

  // SDL aggressive hooking might make the window borderless sometimes
  if (!Host_RendererIsFullscreen())
  {
    SDL_SetWindowFullscreen(keyboard_window, 0);
    SDL_SetWindowBordered(keyboard_window, true);
  }

  Common::KeyboardContext::UpdateLayout();

  return error;
}
}  // namespace

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

  Common::Event m_init_event;
  Uint32 m_stop_event_type;
  Uint32 m_populate_event_type;
  std::thread m_hotplug_thread;
  UniqueSDLWindow m_keyboard_window{nullptr, SDL_DestroyWindow};
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

  SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");

  // We have our own WGI backend. Enabling SDL's WGI handling creates even more redundant devices.
  SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");

  // Disable DualSense Player LEDs; We already colorize the Primary LED
  SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, "0");

  m_hotplug_thread = std::thread([this] {
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

      const Uint32 custom_events_start = SDL_RegisterEvents(5);
      if (custom_events_start == static_cast<Uint32>(-1))
      {
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "SDL failed to register custom events");
        return;
      }
      m_stop_event_type = custom_events_start;
      m_populate_event_type = custom_events_start + 1;
      Common::KeyboardContext::s_sdl_init_event_type = custom_events_start + 2;
      Common::KeyboardContext::s_sdl_update_event_type = custom_events_start + 3;
      Common::KeyboardContext::s_sdl_quit_event_type = custom_events_start + 4;

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

void InputBackend::OpenAndAddDevice(SDL_JoystickID instance_id)
{
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
    OpenAndAddDevice(e.jdevice.which);
  }
  else if (e.type == SDL_EVENT_JOYSTICK_REMOVED)
  {
    GetControllerInterface().RemoveDevice([&e](const auto* device) {
      return device->GetSource() == "SDL" &&
             static_cast<const Gamepad*>(device)->GetSDLInstanceID() == e.jdevice.which;
    });
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
  else if (e.type == m_stop_event_type)
  {
    return false;
  }
  else if (e.type == Common::KeyboardContext::s_sdl_init_event_type)
  {
    const auto wsi = GetControllerInterface().GetWindowSystemInfo();
    if (wsi.type == WindowSystemType::X11)
    {
      // Avoid a crash with Xwayland when the wrong driver is picked
      if (!SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11"))
      {
        WARN_LOG_FMT(IOS_USB, "SDL failed to pick driver to capture keyboard input: {}",
                     SDL_GetError());
      }
    }

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
      ERROR_LOG_FMT(IOS_USB, "SDL failed to init subsystem to capture keyboard input: {}",
                    SDL_GetError());
      return true;
    }

    if (const auto error = UpdateKeyboardHandle(&m_keyboard_window); error.has_value())
    {
      ERROR_LOG_FMT(IOS_USB, "SDL failed to attach window to capture keyboard input: {}", *error);
      return true;
    }
  }
  else if (e.type == Common::KeyboardContext::s_sdl_update_event_type)
  {
    if (!SDL_WasInit(SDL_INIT_VIDEO))
      return true;

    // Release previous SDLWindow
    m_keyboard_window.reset();

    if (const auto error = UpdateKeyboardHandle(&m_keyboard_window); error.has_value())
    {
      ERROR_LOG_FMT(IOS_USB, "SDL failed to switch window to capture keyboard input: {}", *error);
      return true;
    }
  }
  else if (e.type == Common::KeyboardContext::s_sdl_quit_event_type)
  {
    m_keyboard_window.reset();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
  else if (e.type == SDL_EVENT_KEYMAP_CHANGED)
  {
    Common::KeyboardContext::UpdateLayout();
  }

  return true;
}

}  // namespace ciface::SDL
