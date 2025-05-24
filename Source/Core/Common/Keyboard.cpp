// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Keyboard.h"

#include <array>
#include <map>
#include <mutex>
#include <utility>

#ifdef HAVE_SDL2
#include <SDL_events.h>
#include <SDL_keyboard.h>

// Will be overridden by Dolphin's SDL InputBackend
u32 Common::KeyboardContext::s_sdl_init_event_type(-1);
u32 Common::KeyboardContext::s_sdl_update_event_type(-1);
u32 Common::KeyboardContext::s_sdl_quit_event_type(-1);
#endif

#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "DiscIO/Enums.h"

namespace
{
// Translate HID usage ID based on the host and the game keyboard layout:
//  - we need to take into account the host layout as we receive raw scan codes
//  - we need to consider the game layout as it might be different from the host one
u8 TranslateUsageID(u8 usage_id, int host_layout, int game_layout)
{
  if (host_layout == game_layout)
    return usage_id;

  // Currently, the translation is partial (i.e. alpha only)
  if (usage_id != Common::HIDUsageID::M_AZERTY &&
      (usage_id < Common::HIDUsageID::A || usage_id > Common::HIDUsageID::Z))
  {
    return usage_id;
  }

  switch (host_layout | game_layout)
  {
  case Common::KeyboardLayout::AZERTY_QWERTZ:
  {
    static const std::map<u8, u8> TO_QWERTZ{
        {Common::HIDUsageID::A_AZERTY, Common::HIDUsageID::A},
        {Common::HIDUsageID::Z_AZERTY, Common::HIDUsageID::Z_QWERTZ},
        {Common::HIDUsageID::Y, Common::HIDUsageID::Y_QWERTZ},
        {Common::HIDUsageID::Q_AZERTY, Common::HIDUsageID::Q},
        {Common::HIDUsageID::M_AZERTY, Common::HIDUsageID::M},
        {Common::HIDUsageID::W_AZERTY, Common::HIDUsageID::W},
        {Common::HIDUsageID::M, Common::HIDUsageID::M_AZERTY},
    };
    static const std::map<u8, u8> TO_AZERTY{
        {Common::HIDUsageID::Q, Common::HIDUsageID::Q_AZERTY},
        {Common::HIDUsageID::W, Common::HIDUsageID::W_AZERTY},
        {Common::HIDUsageID::Z_QWERTZ, Common::HIDUsageID::Z_AZERTY},
        {Common::HIDUsageID::A, Common::HIDUsageID::A_AZERTY},
        {Common::HIDUsageID::M_AZERTY, Common::HIDUsageID::M},
        {Common::HIDUsageID::Y_QWERTZ, Common::HIDUsageID::Y},
        {Common::HIDUsageID::M, Common::HIDUsageID::M_AZERTY},
    };
    const auto& map = game_layout == Common::KeyboardLayout::QWERTZ ? TO_QWERTZ : TO_AZERTY;
    if (const auto it{map.find(usage_id)}; it != map.end())
      return it->second;
    break;
  }
  case Common::KeyboardLayout::QWERTY_AZERTY:
  {
    static constexpr std::array<std::pair<u8, u8>, 3> BI_MAP{
        {{Common::HIDUsageID::Q, Common::HIDUsageID::A},
         {Common::HIDUsageID::W, Common::HIDUsageID::Z},
         {Common::HIDUsageID::M, Common::HIDUsageID::M_AZERTY}}};
    for (const auto& [a, b] : BI_MAP)
    {
      if (usage_id == a)
        return b;
      else if (usage_id == b)
        return a;
    }
    break;
  }
  case Common::KeyboardLayout::QWERTY_QWERTZ:
  {
    if (usage_id == Common::HIDUsageID::Y)
      return Common::HIDUsageID::Z;
    else if (usage_id == Common::HIDUsageID::Z)
      return Common::HIDUsageID::Y;
    break;
  }
  default:
    // Shouldn't happen
    break;
  }
  return usage_id;
}

int GetHostLayout()
{
  const int layout = Config::Get(Config::MAIN_WII_KEYBOARD_HOST_LAYOUT);
  if (layout != Common::KeyboardLayout::AUTO)
    return layout;

#ifdef HAVE_SDL2
  if (const SDL_Keycode key_code = SDL_GetKeyFromScancode(SDL_SCANCODE_Y); key_code == SDLK_z)
  {
    return Common::KeyboardLayout::QWERTZ;
  }
  if (const SDL_Keycode key_code = SDL_GetKeyFromScancode(SDL_SCANCODE_Q); key_code == SDLK_a)
  {
    return Common::KeyboardLayout::AZERTY;
  }
#endif

  return Common::KeyboardLayout::QWERTY;
}

int GetGameLayout()
{
  const int layout = Config::Get(Config::MAIN_WII_KEYBOARD_GAME_LAYOUT);
  if (layout != Common::KeyboardLayout::AUTO)
    return layout;

  const DiscIO::Language language =
      static_cast<DiscIO::Language>(Config::Get(Config::SYSCONF_LANGUAGE));
  switch (language)
  {
  case DiscIO::Language::French:
    return Common::KeyboardLayout::AZERTY;
  case DiscIO::Language::German:
    return Common::KeyboardLayout::QWERTZ;
  default:
    return Common::KeyboardLayout::QWERTY;
  }
}

u8 MapVirtualKeyToHID(u8 virtual_key, int host_layout, int game_layout)
{
  // SDL2 keyboard state uses scan codes already based on HID usage id
  u8 usage_id = virtual_key;
  if (Config::Get(Config::MAIN_WII_KEYBOARD_TRANSLATION))
    usage_id = TranslateUsageID(usage_id, host_layout, game_layout);
  return usage_id;
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
#ifdef HAVE_SDL2
  SDL_Event event{s_sdl_init_event_type};
  SDL_PushEvent(&event);
  m_keyboard_state = SDL_GetKeyboardState(nullptr);
#endif
  UpdateLayout();
  m_is_ready = true;
}

void KeyboardContext::Quit()
{
  m_is_ready = false;
#ifdef HAVE_SDL2
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
#ifdef HAVE_SDL2
  SDL_Event event{s_sdl_update_event_type};
  SDL_PushEvent(&event);
#endif
}

void KeyboardContext::NotifyQuit()
{
  if (auto self = s_keyboard_context.lock())
    self->Quit();
}

void KeyboardContext::UpdateLayout()
{
  if (auto self = s_keyboard_context.lock())
  {
    self->m_host_layout = GetHostLayout();
    self->m_game_layout = GetGameLayout();
  }
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

HIDPressedState KeyboardContext::GetPressedState() const
{
  return m_is_ready ? HIDPressedState{.modifiers = PollHIDModifiers(),
                                      .pressed_keys = PollHIDPressedKeys()} :
                      HIDPressedState{};
}

bool KeyboardContext::IsVirtualKeyPressed(int virtual_key) const
{
#ifdef HAVE_SDL2
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
#ifdef HAVE_SDL2
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

HIDPressedKeys KeyboardContext::PollHIDPressedKeys() const
{
  HIDPressedKeys pressed_keys{};
  auto it = pressed_keys.begin();

#ifdef HAVE_SDL2
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

    *it = MapVirtualKeyToHID(static_cast<u8>(virtual_key), m_host_layout, m_game_layout);
    if (++it == pressed_keys.end())
      break;
  }
  return pressed_keys;
}
}  // namespace Common
