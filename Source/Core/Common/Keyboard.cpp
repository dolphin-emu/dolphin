// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Keyboard.h"

#include <array>
#include <mutex>
#include <utility>

#ifdef HAVE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>

// Will be overridden by Dolphin's SDL InputBackend
u32 Common::KeyboardContext::s_sdl_init_event_type(-1);
u32 Common::KeyboardContext::s_sdl_update_event_type(-1);
u32 Common::KeyboardContext::s_sdl_quit_event_type(-1);
#endif

#include "Core/Config/MainSettings.h"

namespace
{
u8 MapVirtualKeyToHID(u8 virtual_key, int keyboard_layout)
{
  // SDL3 keyboard state uses scan codes already based on HID usage id
  return virtual_key;
}

std::weak_ptr<Common::KeyboardContext> s_keyboard_context;
std::mutex s_keyboard_context_mutex;

// Will be updated by DolphinQt's Host:
//  - SetRenderHandle
//  - SetFullscreen
Common::KeyboardContext::HandlerState s_handler_state{};
}  // Anonymous namespace

namespace Common
{
KeyboardContext::KeyboardContext()
{
  if (Config::Get(Config::MAIN_WII_KEYBOARD))
    Init();
}

KeyboardContext::~KeyboardContext()
{
  if (Config::Get(Config::MAIN_WII_KEYBOARD))
    Quit();
}

void KeyboardContext::Init()
{
#ifdef HAVE_SDL3
  SDL_Event event{s_sdl_init_event_type};
  SDL_PushEvent(&event);
  m_keyboard_state = SDL_GetKeyboardState(nullptr);
#endif
  m_is_ready = true;
}

void KeyboardContext::Quit()
{
  m_is_ready = false;
#ifdef HAVE_SDL3
  SDL_Event event{s_sdl_quit_event_type};
  SDL_PushEvent(&event);
#endif
}

void* KeyboardContext::HandlerState::GetHandle() const
{
#ifdef _WIN32
  if (is_rendering_to_main && !is_fullscreen)
    return main_handle;
#endif
  return renderer_handle;
}

void KeyboardContext::NotifyInit()
{
  if (auto self = s_keyboard_context.lock())
    self->Init();
}

void KeyboardContext::NotifyHandlerChanged(const KeyboardContext::HandlerState& state)
{
  s_handler_state = state;
  if (s_keyboard_context.expired())
    return;
#ifdef HAVE_SDL3
  SDL_Event event{s_sdl_update_event_type};
  SDL_PushEvent(&event);
#endif
}

void KeyboardContext::NotifyQuit()
{
  if (auto self = s_keyboard_context.lock())
    self->Quit();
}

void* KeyboardContext::GetWindowHandle()
{
  return s_handler_state.GetHandle();
}

std::shared_ptr<KeyboardContext> KeyboardContext::GetInstance()
{
  const std::lock_guard guard(s_keyboard_context_mutex);
  std::shared_ptr<KeyboardContext> ptr = s_keyboard_context.lock();
  if (!ptr)
  {
    ptr = std::shared_ptr<KeyboardContext>(new KeyboardContext);
    s_keyboard_context = ptr;
  }
  return ptr;
}

HIDPressedState KeyboardContext::GetPressedState(int keyboard_layout) const
{
  return m_is_ready ? HIDPressedState{.modifiers = PollHIDModifiers(),
                                      .pressed_keys = PollHIDPressedKeys(keyboard_layout)} :
                      HIDPressedState{};
}

bool KeyboardContext::IsVirtualKeyPressed(int virtual_key) const
{
#ifdef HAVE_SDL3
  if (virtual_key >= SDL_SCANCODE_COUNT)
    return false;
  return m_keyboard_state[virtual_key];
#else
  // TODO: Android implementation
  return false;
#endif
}

u8 KeyboardContext::PollHIDModifiers() const
{
  u8 modifiers = 0;

  using VkHidPair = std::pair<int, u8>;

  // References:
  // https://wiki.libsdl.org/SDL3/SDL_Scancode
  // https://www.usb.org/document-library/device-class-definition-hid-111
  //
  // HID modifiers:
  // Bit 0 - LEFT CTRL
  // Bit 1 - LEFT SHIFT
  // Bit 2 - LEFT ALT
  // Bit 3 - LEFT GUI
  // Bit 4 - RIGHT CTRL
  // Bit 5 - RIGHT SHIFT
  // Bit 6 - RIGHT ALT
  // Bit 7 - RIGHT GUI
  static const std::vector<VkHidPair> MODIFIERS_MAP{
#ifdef HAVE_SDL3
      {SDL_SCANCODE_LCTRL, 0x01}, {SDL_SCANCODE_LSHIFT, 0x02}, {SDL_SCANCODE_LALT, 0x04},
      {SDL_SCANCODE_LGUI, 0x08},  {SDL_SCANCODE_RCTRL, 0x10},  {SDL_SCANCODE_RSHIFT, 0x20},
      {SDL_SCANCODE_RALT, 0x40},  {SDL_SCANCODE_RGUI, 0x80}
#else
  // TODO: Android implementation
#endif
  };

  for (const auto& [virtual_key, hid_modifier] : MODIFIERS_MAP)
  {
    if (IsVirtualKeyPressed(virtual_key))
      modifiers |= hid_modifier;
  }

  return modifiers;
}

HIDPressedKeys KeyboardContext::PollHIDPressedKeys(int keyboard_layout) const
{
  HIDPressedKeys pressed_keys{};
  auto it = pressed_keys.begin();

#ifdef HAVE_SDL3
  const std::size_t begin = SDL_SCANCODE_A;
  const std::size_t end = SDL_SCANCODE_LCTRL;
#else
  const std::size_t begin = 0;
  const std::size_t end = 0;
#endif

  for (std::size_t virtual_key = begin; virtual_key < end; ++virtual_key)
  {
    if (!IsVirtualKeyPressed(static_cast<int>(virtual_key)))
      continue;

    *it = MapVirtualKeyToHID(static_cast<u8>(virtual_key), keyboard_layout);
    if (++it == pressed_keys.end())
      break;
  }
  return pressed_keys;
}
}  // namespace Common
