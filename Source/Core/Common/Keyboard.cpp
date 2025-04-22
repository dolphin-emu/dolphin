// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Keyboard.h"

#include <array>
#include <mutex>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#elif defined(HAVE_SDL2)
#include <SDL_events.h>
#include <SDL_keyboard.h>
#endif

#ifdef HAVE_SDL2
// Will be overridden by Dolphin's SDL InputBackend
u32 Common::KeyboardContext::s_sdl_init_event_type(-1);
u32 Common::KeyboardContext::s_sdl_update_event_type(-1);
u32 Common::KeyboardContext::s_sdl_quit_event_type(-1);
#endif

#include "Core/Config/MainSettings.h"

namespace
{
// Crazy ugly
#ifdef _WIN32
constexpr std::size_t KEYBOARD_STATE_SIZE = 256;
constexpr std::array<u8, KEYBOARD_STATE_SIZE> VK_HID_QWERTY{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x2A,  // Backspace
    0x2B,  // Tab
    0x00, 0x00,
    0x00,  // Clear
    0x28,  // Return
    0x00, 0x00,
    0x00,  // Shift
    0x00,  // Control
    0x00,  // ALT
    0x48,  // Pause
    0x39,  // Capital
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x29,  // Escape
    0x00, 0x00, 0x00, 0x00,
    0x2C,  // Space
    0x4B,  // Prior
    0x4E,  // Next
    0x4D,  // End
    0x4A,  // Home
    0x50,  // Left
    0x52,  // Up
    0x4F,  // Right
    0x51,  // Down
    0x00, 0x00, 0x00,
    0x46,  // Print screen
    0x49,  // Insert
    0x4C,  // Delete
    0x00,
    // 0 -> 9
    0x27, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
    // A -> Z
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Numpad 0 -> 9
    0x62, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61,
    0x55,  // Multiply
    0x57,  // Add
    0x00,  // Separator
    0x56,  // Subtract
    0x63,  // Decimal
    0x54,  // Divide
    // F1 -> F12
    0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    // F13 -> F24
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x53,  // Numlock
    0x47,  // Scroll lock
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Modifier keys
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x33,  // ';'
    0x2E,  // Plus
    0x36,  // Comma
    0x2D,  // Minus
    0x37,  // Period
    0x38,  // '/'
    0x35,  // '~'
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x2F,  // '['
    0x32,  // '\'
    0x30,  // ']'
    0x34,  // '''
    0x00,  //
    0x00,  // Nothing interesting past this point.
};

constexpr std::array<u8, KEYBOARD_STATE_SIZE> VK_HID_AZERTY{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x2A,  // Backspace
    0x2B,  // Tab
    0x00, 0x00,
    0x00,  // Clear
    0x28,  // Return
    0x00, 0x00,
    0x00,  // Shift
    0x00,  // Control
    0x00,  // ALT
    0x48,  // Pause
    0x39,  // Capital
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x29,  // Escape
    0x00, 0x00, 0x00, 0x00,
    0x2C,  // Space
    0x4B,  // Prior
    0x4E,  // Next
    0x4D,  // End
    0x4A,  // Home
    0x50,  // Left
    0x52,  // Up
    0x4F,  // Right
    0x51,  // Down
    0x00, 0x00, 0x00,
    0x46,  // Print screen
    0x49,  // Insert
    0x4C,  // Delete
    0x00,
    // 0 -> 9
    0x27, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
    // A -> Z
    0x14, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x33, 0x11, 0x12, 0x13,
    0x04, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1D, 0x1B, 0x1C, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Numpad 0 -> 9
    0x62, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61,
    0x55,  // Multiply
    0x57,  // Add
    0x00,  // Separator
    0x56,  // Substract
    0x63,  // Decimal
    0x54,  // Divide
    // F1 -> F12
    0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    // F13 -> F24
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x53,  // Numlock
    0x47,  // Scroll lock
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Modifier keys
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x30,  // '$'
    0x2E,  // Plus
    0x10,  // Comma
    0x00,  // Minus
    0x36,  // Period
    0x37,  // '/'
    0x34,  // ' '
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x2D,  // ')'
    0x32,  // '\'
    0x2F,  // '^'
    0x00,  // ' '
    0x38,  // '!'
    0x00,  // Nothing interesting past this point.
};

u8 MapVirtualKeyToHID(u8 virtual_key, int keyboard_layout)
{
  switch (keyboard_layout)
  {
  case Common::KBD_LAYOUT_AZERTY:
    return VK_HID_AZERTY[virtual_key];
  case Common::KBD_LAYOUT_QWERTY:
  default:
    return VK_HID_QWERTY[virtual_key];
  }
}
#else
u8 MapVirtualKeyToHID(u8 virtual_key, int keyboard_layout)
{
  // SDL2 keyboard state uses scan codes already based on HID usage id
  return virtual_key;
}
#endif

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
#if !defined(_WIN32) && defined(HAVE_SDL2)
  SDL_Event event{s_sdl_init_event_type};
  SDL_PushEvent(&event);
  m_keyboard_state = SDL_GetKeyboardState(nullptr);
#endif
  m_is_ready = true;
}

void KeyboardContext::Quit()
{
  m_is_ready = false;
#if !defined(_WIN32) && defined(HAVE_SDL2)
  SDL_Event event{s_sdl_quit_event_type};
  SDL_PushEvent(&event);
#endif
}

const void* KeyboardContext::HandlerState::GetHandle() const
{
  if (is_rendering_to_main && !is_fullscreen)
    return main_handle;
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
#if !defined(_WIN32) && defined(HAVE_SDL2)
  SDL_Event event{s_sdl_update_event_type};
  SDL_PushEvent(&event);
#endif
}

void KeyboardContext::NotifyQuit()
{
  if (auto self = s_keyboard_context.lock())
    self->Quit();
}

const void* KeyboardContext::GetWindowHandle()
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
#ifdef _WIN32
  return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
#elif defined(HAVE_SDL2)
  if (virtual_key >= SDL_NUM_SCANCODES)
    return false;
  return m_keyboard_state[virtual_key] == 1;
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
  // https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes
  // https://wiki.libsdl.org/SDL2/SDL_Scancode
  // https://www.usb.org/document-library/device-class-definition-hid-111
  //
  // HID modifier: Bit 0 - LEFT CTRL
  // HID modifier: Bit 1 - LEFT SHIFT
  // HID modifier: Bit 2 - LEFT ALT
  // HID modifier: Bit 3 - LEFT GUI
  // HID modifier: Bit 4 - RIGHT CTRL
  // HID modifier: Bit 5 - RIGHT SHIFT
  // HID modifier: Bit 6 - RIGHT ALT
  // HID modifier: Bit 7 - RIGHT GUI
  static const std::vector<VkHidPair> MODIFIERS_MAP{
#ifdef _WIN32
      {VK_LCONTROL, 0x01}, {VK_LSHIFT, 0x02},   {VK_LMENU, 0x04},
      {VK_LWIN, 0x08},     {VK_RCONTROL, 0x10}, {VK_RSHIFT, 0x20},
      {VK_RMENU, 0x40},    {VK_RWIN, 0x80}
#elif defined(HAVE_SDL2)
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

#ifdef _WIN32
  const std::size_t begin = 0;
  const std::size_t end = KEYBOARD_STATE_SIZE;
#elif defined(HAVE_SDL2)
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
