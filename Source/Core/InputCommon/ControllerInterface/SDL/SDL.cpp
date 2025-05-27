// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/SDL/SDL.h"

#include <optional>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <SDL.h>

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

std::optional<const char*> UpdateKeyboardHandle(UniqueSDLWindow* unique_window)
{
  std::optional<const char*> error;

  // Doesn't seem to work with X11 until SDL 3.10:
  //  - https://github.com/libsdl-org/SDL/pull/10467
  const void* keyboard_handle = Common::KeyboardContext::GetWindowHandle();
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
    SDL_SetWindowBordered(keyboard_window, SDL_TRUE);
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
  ~InputBackend();
  void PopulateDevices() override;
  void UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>& devices_to_remove) override;

private:
  void OpenAndAddDevice(int index);

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
  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
  SDL_LogSetOutputFunction(
      [](void*, int category, SDL_LogPriority priority, const char* message) {
        std::string category_name;
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
        default:
          category_name = fmt::format("unknown({})", category);
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

        GENERIC_LOG_FMT(Common::Log::LogType::CONTROLLERINTERFACE, log_level, "{}: {}",
                        category_name, message);
      },
      nullptr);
}

InputBackend::InputBackend(ControllerInterface* controller_interface)
    : ciface::InputBackend(controller_interface)
{
  EnableSDLLogging();

  // This is required on windows so that SDL's joystick code properly pumps window messages
  SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");

  SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
  // We want buttons to come in as positions, not labels
  SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
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

      if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER) != 0)
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
      while (SDL_PollEvent(&e) != 0)
      {
        if (!HandleEventAndContinue(e))
          return;
      }
    }

#ifdef _WIN32
    // This is a hack to workaround SDL_hidapi using window messages to detect device
    // removal/arrival, yet no part of SDL pumps messages for it. It can hopefully be removed in the
    // future when SDL fixes the issue. Note this is a separate issue from SDL_HINT_JOYSTICK_THREAD.
    // Also note that SDL_WaitEvent may block while device detection window messages get queued up,
    // causing some noticible stutter. This is just another reason it should be fixed properly by
    // SDL...
    const auto window_handle =
        FindWindowEx(HWND_MESSAGE, nullptr, TEXT("SDL_HIDAPI_DEVICE_DETECTION"), nullptr);
#endif

    SDL_Event e;
    while (SDL_WaitEvent(&e) != 0)
    {
      if (!HandleEventAndContinue(e))
        return;

#ifdef _WIN32
      MSG msg;
      while (window_handle && PeekMessage(&msg, window_handle, 0, 0, PM_NOREMOVE))
      {
        if (GetMessageA(&msg, window_handle, 0, 0) != 0)
        {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
      }
#endif
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

void InputBackend::UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>& devices_to_remove)
{
  SDL_GameControllerUpdate();
}

void InputBackend::OpenAndAddDevice(int index)
{
  SDL_GameController* gc = SDL_GameControllerOpen(index);
  SDL_Joystick* js = SDL_JoystickOpen(index);

  if (js)
  {
    if (SDL_JoystickNumButtons(js) > 255 || SDL_JoystickNumAxes(js) > 255 ||
        SDL_JoystickNumHats(js) > 255 || SDL_JoystickNumBalls(js) > 255)
    {
      // This device is invalid, don't use it
      // Some crazy devices (HP webcam 2100) end up as HID devices
      // SDL tries parsing these as Joysticks
      return;
    }
    auto gamecontroller = std::make_shared<GameController>(gc, js);
    if (!gamecontroller->Inputs().empty() || !gamecontroller->Outputs().empty())
      GetControllerInterface().AddDevice(std::move(gamecontroller));
  }
}

bool InputBackend::HandleEventAndContinue(const SDL_Event& e)
{
  if (e.type == SDL_JOYDEVICEADDED)
  {
    // NOTE: SDL_JOYDEVICEADDED's `jdevice.which` is a device index in SDL2.
    // It will change to an "instance ID" in SDL3.
    // OpenAndAddDevice impl and calls will need refactoring when changing to SDL3.
    static_assert(!SDL_VERSION_ATLEAST(3, 0, 0), "Refactoring is needed for SDL3.");
    OpenAndAddDevice(e.jdevice.which);
  }
  else if (e.type == SDL_JOYDEVICEREMOVED)
  {
    // NOTE: SDL_JOYDEVICEREMOVED's `jdevice.which` is an "instance ID".
    GetControllerInterface().RemoveDevice([&e](const auto* device) {
      return device->GetSource() == "SDL" &&
             static_cast<const GameController*>(device)->GetSDLInstanceID() == e.jdevice.which;
    });
  }
  else if (e.type == m_populate_event_type)
  {
    GetControllerInterface().PlatformPopulateDevices([this] {
      for (int i = 0; i < SDL_NumJoysticks(); ++i)
        OpenAndAddDevice(i);
    });
  }
  else if (e.type == m_stop_event_type)
  {
    return false;
  }
  else if (e.type == Common::KeyboardContext::s_sdl_init_event_type)
  {
    if (const int error = SDL_InitSubSystem(SDL_INIT_VIDEO); error != 0)
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
  else if (e.type == SDL_KEYMAPCHANGED)
  {
    Common::KeyboardContext::UpdateLayout();
  }

  return true;
}

}  // namespace ciface::SDL
