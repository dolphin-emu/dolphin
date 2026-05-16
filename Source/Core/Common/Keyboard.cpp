// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Keyboard.h"

#include <array>
#include <map>
#include <mutex>
#include <ranges>
#include <utility>

#ifdef __APPLE__
#include <Carbon/Carbon.h>

#include "Common/ScopeGuard.h"
#endif

#ifdef HAVE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>

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

#ifdef __APPLE__
  auto current_layout = TISCopyCurrentKeyboardLayoutInputSource();
  Common::ScopeGuard guard([&current_layout] { CFRelease(current_layout); });

  auto layout_data = static_cast<CFDataRef>(
      TISGetInputSourceProperty(current_layout, kTISPropertyUnicodeKeyLayoutData));
  if (!layout_data)
    return Common::KeyboardLayout::QWERTY;

  auto unicode_layout = reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(layout_data));

  using VkToLayout = std::tuple<CGKeyCode, char, Common::KeyboardLayout::LayoutEnum>;
  for (auto& [key_code, c, l] :
       std::array{VkToLayout(kVK_ANSI_Y, 'z', Common::KeyboardLayout::QWERTZ),
                  VkToLayout(kVK_ANSI_Q, 'a', Common::KeyboardLayout::AZERTY)})
  {
    UInt32 dead_key_state = 0;
    std::array<UniChar, 4> str{};
    UniCharCount real_length = 0;
    UCKeyTranslate(unicode_layout, key_code, kUCKeyActionDown, 0, LMGetKbdType(),
                   kUCKeyTranslateNoDeadKeysMask, &dead_key_state, str.size(), &real_length,
                   str.data());
    if (real_length && (str[0] == c || str[0] == (c - 0x20)))
      return l;
  }
#elif defined(HAVE_SDL3)
  if (const SDL_Keycode key_code = SDL_GetKeyFromScancode(SDL_SCANCODE_Y, SDL_KMOD_NONE, false);
      key_code == SDLK_Z)
  {
    return Common::KeyboardLayout::QWERTZ;
  }
  if (const SDL_Keycode key_code = SDL_GetKeyFromScancode(SDL_SCANCODE_Q, SDL_KMOD_NONE, false);
      key_code == SDLK_A)
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
#ifdef __APPLE__
  static const std::map<u8, u8> VK_TO_HID{{kVK_ANSI_A, 0x04},
                                          {kVK_ANSI_B, 0x05},
                                          {kVK_ANSI_C, 0x06},
                                          {kVK_ANSI_D, 0x07},
                                          {kVK_ANSI_E, 0x08},
                                          {kVK_ANSI_F, 0x09},
                                          {kVK_ANSI_G, 0x0a},
                                          {kVK_ANSI_H, 0x0b},
                                          {kVK_ANSI_I, 0x0c},
                                          {kVK_ANSI_J, 0x0d},
                                          {kVK_ANSI_K, 0x0e},
                                          {kVK_ANSI_L, 0x0f},
                                          {kVK_ANSI_M, 0x10},
                                          {kVK_ANSI_N, 0x11},
                                          {kVK_ANSI_O, 0x12},
                                          {kVK_ANSI_P, 0x13},
                                          {kVK_ANSI_Q, 0x14},
                                          {kVK_ANSI_R, 0x15},
                                          {kVK_ANSI_S, 0x16},
                                          {kVK_ANSI_T, 0x17},
                                          {kVK_ANSI_U, 0x18},
                                          {kVK_ANSI_V, 0x19},
                                          {kVK_ANSI_W, 0x1a},
                                          {kVK_ANSI_X, 0x1b},
                                          {kVK_ANSI_Y, 0x1c},
                                          {kVK_ANSI_Z, 0x1d},
                                          {kVK_ANSI_1, 0x1e},
                                          {kVK_ANSI_2, 0x1f},
                                          {kVK_ANSI_3, 0x20},
                                          {kVK_ANSI_4, 0x21},
                                          {kVK_ANSI_5, 0x22},
                                          {kVK_ANSI_6, 0x23},
                                          {kVK_ANSI_7, 0x24},
                                          {kVK_ANSI_8, 0x25},
                                          {kVK_ANSI_9, 0x26},
                                          {kVK_ANSI_0, 0x27},
                                          {kVK_Return, 0x28},
                                          {kVK_Escape, 0x29},
                                          {kVK_Delete, 0x2a},
                                          {kVK_Tab, 0x2b},
                                          {kVK_Space, 0x2c},
                                          {kVK_ANSI_Minus, 0x2d},
                                          {kVK_ANSI_Equal, 0x2e},
                                          {kVK_ANSI_LeftBracket, 0x2f},
                                          {kVK_ANSI_RightBracket, 0x30},
                                          {kVK_ANSI_Backslash, 0x31},
                                          // Missing: Non-US #
                                          {kVK_ANSI_Semicolon, 0x33},
                                          {kVK_ANSI_Quote, 0x34},
                                          {kVK_ANSI_Grave, 0x35},
                                          {kVK_ANSI_Comma, 0x36},
                                          {kVK_ANSI_Period, 0x37},
                                          {kVK_ANSI_Slash, 0x38},
                                          {kVK_CapsLock, 0x39},
                                          {kVK_F1, 0x3a},
                                          {kVK_F2, 0x3b},
                                          {kVK_F3, 0x3c},
                                          {kVK_F4, 0x3d},
                                          {kVK_F5, 0x3e},
                                          {kVK_F6, 0x3f},
                                          {kVK_F7, 0x40},
                                          {kVK_F8, 0x41},
                                          {kVK_F9, 0x42},
                                          {kVK_F10, 0x43},
                                          {kVK_F11, 0x44},
                                          {kVK_F12, 0x45},
                                          // Missing: PrintScreen, Scroll Lock, Pause
                                          {kVK_Help, 0x49},
                                          {kVK_Home, 0x4a},
                                          {kVK_PageUp, 0x4b},
                                          {kVK_ForwardDelete, 0x4c},
                                          {kVK_End, 0x4d},
                                          {kVK_PageDown, 0x4e},
                                          {kVK_RightArrow, 0x4f},
                                          {kVK_LeftArrow, 0x50},
                                          {kVK_DownArrow, 0x51},
                                          {kVK_UpArrow, 0x52},
                                          {kVK_ANSI_KeypadClear, 0x53},
                                          {kVK_ANSI_KeypadDivide, 0x54},
                                          {kVK_ANSI_KeypadMultiply, 0x55},
                                          {kVK_ANSI_KeypadMinus, 0x56},
                                          {kVK_ANSI_KeypadPlus, 0x57},
                                          {kVK_ANSI_KeypadEnter, 0x58},
                                          {kVK_ANSI_Keypad1, 0x59},
                                          {kVK_ANSI_Keypad2, 0x5a},
                                          {kVK_ANSI_Keypad3, 0x5b},
                                          {kVK_ANSI_Keypad4, 0x5c},
                                          {kVK_ANSI_Keypad5, 0x5d},
                                          {kVK_ANSI_Keypad6, 0x5e},
                                          {kVK_ANSI_Keypad7, 0x5f},
                                          {kVK_ANSI_Keypad8, 0x60},
                                          {kVK_ANSI_Keypad9, 0x61},
                                          {kVK_ANSI_Keypad0, 0x62},
                                          {kVK_ANSI_KeypadDecimal, 0x63},
                                          // Missing: Non-US |, Application, Power
                                          {kVK_ANSI_KeypadEquals, 0x67},
                                          {kVK_Control, 0xe0},
                                          {kVK_Shift, 0xe1},
                                          {kVK_Option, 0xe2},
                                          {kVK_Command, 0xe3},
                                          {kVK_RightControl, 0xe4},
                                          {kVK_RightShift, 0xe5},
                                          {kVK_RightOption, 0xe6},
                                          {kVK_RightCommand, 0xe7}};
  const auto it{VK_TO_HID.find(virtual_key)};
  u8 usage_id = (it == VK_TO_HID.end()) ? 0 : it->second;
#else
  // SDL3 keyboard state uses scan codes already based on HID usage id
  u8 usage_id = virtual_key;
#endif
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
#ifdef __APPLE__
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(), this,
      [](CFNotificationCenterRef /* center */, void* /* observer */, CFStringRef /* name */,
         const void* /* object */,
         CFDictionaryRef /* userInfo */) { KeyboardContext::UpdateLayout(); },
      kTISNotifySelectedKeyboardInputSourceChanged, nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);
#elif defined(HAVE_SDL3)
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
#ifdef __APPLE__
  CFNotificationCenterRemoveEveryObserver(CFNotificationCenterGetDistributedCenter(), this);
#elif defined(HAVE_SDL3)
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
#ifdef __APPLE__
  // Quartz doesn't seem impacted
#elif defined(HAVE_SDL3)
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

HIDPressedState KeyboardContext::GetPressedState() const
{
  return m_is_ready ? HIDPressedState{.modifiers = PollHIDModifiers(),
                                      .pressed_keys = PollHIDPressedKeys()} :
                      HIDPressedState{};
}

bool KeyboardContext::IsVirtualKeyPressed(int virtual_key) const
{
#ifdef __APPLE__
  return CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, CGKeyCode(virtual_key));
#elif defined(HAVE_SDL3)
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
  // https://stackoverflow.com/a/16125341
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
#ifdef __APPLE__
      {kVK_Control, 0x01},     {kVK_Shift, 0x02},        {kVK_Option, 0x04},
      {kVK_Command, 0x08},     {kVK_RightControl, 0x10}, {kVK_RightShift, 0x20},
      {kVK_RightOption, 0x40}, {kVK_RightCommand, 0x80}
#elif defined(HAVE_SDL3)
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

#ifdef __APPLE__
  // See comment about <HIToolbox/Events.h> keycodes in QuartzKeyboardAndMouse.mm
  static auto KEYCODES = std::ranges::filter_view(std::ranges::iota_view{0, 0x7f}, [](int keycode) {
    switch (keycode)
    {
    case kVK_Command:
    case kVK_RightCommand:
    case kVK_Shift:
    case kVK_RightShift:
    case kVK_Option:
    case kVK_RightOption:
    case kVK_Control:
    case kVK_RightControl:
      return false;
    default:
      return true;
    }
  });
  static const std::vector<int> VIRTUAL_KEYS{KEYCODES.begin(), KEYCODES.end()};
#elif defined(HAVE_SDL3)
  static constexpr std::ranges::iota_view SCANCODES{int(SDL_SCANCODE_A), int(SDL_SCANCODE_LCTRL)};
  static const std::vector<int> VIRTUAL_KEYS{SCANCODES.begin(), SCANCODES.end()};
#else
  static constexpr std::vector<int> VIRTUAL_KEYS{};
#endif

  for (int virtual_key : VIRTUAL_KEYS)
  {
    if (!IsVirtualKeyPressed(virtual_key))
      continue;

    *it = MapVirtualKeyToHID(static_cast<u8>(virtual_key), m_host_layout, m_game_layout);
    if (++it == pressed_keys.end())
      break;
  }
  return pressed_keys;
}
}  // namespace Common
