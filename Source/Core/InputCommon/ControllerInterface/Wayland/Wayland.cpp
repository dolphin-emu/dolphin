// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Keyboard and mouse input for native Wayland windows.
//
// Qt owns the Wayland connection and the window, so we talk to the compositor through a private
// event queue on the same connection (see Common::Wayland::Connection). We bind our own wl_seat,
// which gets the same focus-based keyboard and pointer events Qt gets. Our events are only
// dispatched from UpdateInput(), which is called from Dolphin's input polling.
//
// Each wl_seat is exposed as one device with the controls of the XInput2 devices:
// *    Key controls named after the XKB keysyms, like the XInput2 ones.
// *    Mouse button controls: 32 of them, numbered like X11 buttons.
// *    Mouse cursor controls: one for each cardinal direction, calculated from the pointer position
//      in the render surface.
// *    Mouse axis controls: a smoothed running average of the relative pointer motion.
// *    Relative mouse controls: the relative pointer motion since the last update.
//
// Wayland doesn't let clients warp the pointer, except through wp_pointer_warp_v1 where the
// compositor supports it. Without it, "Center Mouse" moves the origin of the cursor controls to
// the current pointer position instead.

#include "InputCommon/ControllerInterface/Wayland/Wayland.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

#include <pointer-warp-v1-client-protocol.h>
#include <relative-pointer-unstable-v1-client-protocol.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Matrix.h"
#include "Common/Wayland/Connection.h"

#include "Core/Host.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/InputBackend.h"

namespace ciface::Wayland
{
namespace
{
constexpr std::string_view SOURCE_NAME = "Wayland";

// The seat name is something like "seat0", which says nothing to the user. This matches the name of
// the keyboard and mouse device on Windows.
constexpr std::string_view DEVICE_NAME = "Keyboard Mouse";

// These are tuned the same as in the XInput2 backend, see there for the reasoning.
constexpr float MOUSE_AXIS_SENSITIVITY = 8.0f;
constexpr float MOUSE_AXIS_SMOOTHING = 1.5f;
constexpr float SCROLL_AXIS_DECAY = 1.1f;

// One scroll wheel click produces this many units of scrolling on the Z axis.
constexpr float SCROLL_UNITS_PER_CLICK = 8.0f;
// A continuous scrolling device (like a touchpad) sends about this many surface units per click.
constexpr double CONTINUOUS_SCROLL_UNITS_PER_CLICK = 10.0;

// Linux evdev button codes
constexpr u32 BTN_LEFT = 0x110;
constexpr u32 BTN_RIGHT = 0x111;
constexpr u32 BTN_MIDDLE = 0x112;
constexpr u32 BTN_SIDE = 0x113;

// The XKB keycodes are the evdev codes + 8.
constexpr u32 MAX_KEYCODE = 1024;
constexpr u32 NUM_KEY_WORDS = MAX_KEYCODE / 64;

using KeyState = std::array<std::atomic<u64>, NUM_KEY_WORDS>;

void SetKey(KeyState& state, u32 keycode, bool pressed)
{
  if (keycode >= MAX_KEYCODE)
    return;
  const u64 mask = u64(1) << (keycode % 64);
  if (pressed)
    state[keycode / 64].fetch_or(mask, std::memory_order_relaxed);
  else
    state[keycode / 64].fetch_and(~mask, std::memory_order_relaxed);
}

// Maps a Linux button code to the index X11 would use for the button (zero indexed), so that
// mappings made on X11 keep their meaning.
u32 GetButtonIndex(u32 button)
{
  switch (button)
  {
  case BTN_LEFT:
    return 0;
  case BTN_MIDDLE:
    return 1;
  case BTN_RIGHT:
    return 2;
  default:
    // X11 uses buttons 4-7 for scrolling, so the side buttons start at 8.
    return button >= BTN_SIDE ? 7 + (button - BTN_SIDE) : 32;
  }
}

class KeyboardMouse final : public Core::Device
{
private:
  class Key final : public Input
  {
  public:
    Key(std::string name, u32 keycode, const KeyState* state)
        : m_name(std::move(name)), m_keycodes{keycode}, m_state(state)
    {
    }

    std::string GetName() const override { return m_name; }
    ControlState GetState() const override
    {
      return ControlState(std::ranges::any_of(m_keycodes, [this](u32 keycode) {
        return ((*m_state)[keycode / 64].load(std::memory_order_relaxed) >> (keycode % 64)) & 1;
      }));
    }

    void AddKeyCode(u32 keycode) { m_keycodes.push_back(keycode); }

  private:
    const std::string m_name;
    std::vector<u32> m_keycodes;
    const KeyState* m_state;
  };

  class Button final : public Input
  {
  public:
    Button(u32 index, const std::atomic<u32>* buttons)
        : m_name(fmt::format("Click {}", index + 1)), m_index(index), m_buttons(buttons)
    {
    }

    std::string GetName() const override { return m_name; }
    ControlState GetState() const override
    {
      return ControlState((m_buttons->load(std::memory_order_relaxed) >> m_index) & 1);
    }

  private:
    const std::string m_name;
    const u32 m_index;
    const std::atomic<u32>* m_buttons;
  };

  // Shared by the controls which are a direction of a float.
  class DirectionalInput : public Input
  {
  public:
    DirectionalInput(const char* prefix, u8 index, bool positive, const std::atomic<float>* value,
                     float scale)
        : m_name(
              fmt::format("{} {}{}", prefix, static_cast<char>('X' + index), positive ? '+' : '-')),
          m_positive(positive), m_value(value), m_scale(scale)
    {
    }

    std::string GetName() const override { return m_name; }
    bool IsDetectable() const override { return false; }
    ControlState GetState() const override
    {
      return std::max(0.0f,
                      m_value->load(std::memory_order_relaxed) / (m_positive ? m_scale : -m_scale));
    }

  private:
    const std::string m_name;
    const bool m_positive;
    const std::atomic<float>* m_value;
    const float m_scale;
  };

  struct Vec3
  {
    std::atomic<float> x{};
    std::atomic<float> y{};
    std::atomic<float> z{};
  };

public:
  static std::shared_ptr<KeyboardMouse>
  Create(std::shared_ptr<Common::Wayland::Connection> connection,
         const Common::Wayland::Connection::Global& seat, wl_surface* render_surface)
  {
    // The constructor is private so that the object is never used before Initialize().
    std::shared_ptr<KeyboardMouse> device(new KeyboardMouse(std::move(connection), render_surface));
    return device->Initialize(seat) ? device : nullptr;
  }

  ~KeyboardMouse() override
  {
    // The proxies must be destroyed before the connection and their listeners' data.
    if (m_relative_pointer)
      zwp_relative_pointer_v1_destroy(m_relative_pointer);
    if (m_relative_pointer_manager)
      zwp_relative_pointer_manager_v1_destroy(m_relative_pointer_manager);
    if (m_warp)
      wp_pointer_warp_v1_destroy(m_warp);
    if (m_pointer)
    {
      if (wl_pointer_get_version(m_pointer) >= WL_POINTER_RELEASE_SINCE_VERSION)
        wl_pointer_release(m_pointer);
      else
        wl_pointer_destroy(m_pointer);
    }
    if (m_keyboard)
    {
      if (wl_keyboard_get_version(m_keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
        wl_keyboard_release(m_keyboard);
      else
        wl_keyboard_destroy(m_keyboard);
    }
    if (m_seat)
    {
      if (wl_seat_get_version(m_seat) >= WL_SEAT_RELEASE_SINCE_VERSION)
        wl_seat_release(m_seat);
      else
        wl_seat_destroy(m_seat);
    }
    m_connection->Flush();

    if (m_keymap)
      xkb_keymap_unref(m_keymap);
    if (m_xkb_context)
      xkb_context_unref(m_xkb_context);
  }

  std::string GetName() const override { return std::string(DEVICE_NAME); }
  std::string GetSource() const override { return std::string(SOURCE_NAME); }
  int GetSortPriority() const override { return DEFAULT_DEVICE_SORT_PRIORITY; }

  Core::DeviceRemoval UpdateInput() override
  {
    m_connection->DispatchPending();

    m_state.relative_mouse.x = m_delta.x;
    m_state.relative_mouse.y = m_delta.y;
    m_state.relative_mouse.z = m_delta.z;

    // Apply axis smoothing.
    const auto smooth = [](std::atomic<float>& axis, float delta) {
      axis = (axis * MOUSE_AXIS_SMOOTHING + delta) / (MOUSE_AXIS_SMOOTHING + 1.0f);
    };
    smooth(m_state.axis.x, m_delta.x);
    smooth(m_state.axis.y, m_delta.y);
    m_state.axis.z = (m_state.axis.z + m_delta.z) / SCROLL_AXIS_DECAY;
    m_delta = {};

    const bool should_center_mouse = g_controller_interface.IsMouseCenteringRequested() &&
                                     (Host_RendererHasFocus() || Host_TASInputHasFocus());
    if (should_center_mouse)
      CenterMouse();

    // When a TAS Input window has focus and "Enable Controller Input" is checked most types of
    // input should be read normally as if the render window had focus instead. The cursor is an
    // exception, as otherwise using the mouse to set any control in the TAS Input window will also
    // update the Wii IR value (or any other input controlled by the cursor).
    if (!Host_TASInputHasFocus())
    {
      m_state.buttons = m_buttons;

      const Common::Vec2 size = g_controller_interface.GetRenderSurfaceLogicalSize();
      const Common::Vec2 window_scale = g_controller_interface.GetWindowInputScale();

      // the mouse position as a range from -1 to 1
      m_state.cursor.x =
          static_cast<float>(((m_pointer_x - m_origin_x) / size.x * 2 - 1) * window_scale.x);
      m_state.cursor.y =
          static_cast<float>(((m_pointer_y - m_origin_y) / size.y * 2 - 1) * window_scale.y);
    }

    return Core::DeviceRemoval::Keep;
  }

private:
  struct State
  {
    KeyState keyboard{};
    std::atomic<u32> buttons{};
    Vec3 axis;
    Vec3 relative_mouse;
    struct
    {
      std::atomic<float> x{};
      std::atomic<float> y{};
    } cursor;
  };

  KeyboardMouse(std::shared_ptr<Common::Wayland::Connection> connection, wl_surface* render_surface)
      : m_connection(std::move(connection)), m_render_surface(render_surface)
  {
  }

  bool Initialize(const Common::Wayland::Connection::Global& global)
  {
    m_seat = static_cast<wl_seat*>(m_connection->Bind(global, wl_seat_interface, 8));
    if (!m_seat)
      return false;

    static constexpr wl_seat_listener seat_listener = {
        .capabilities =
            [](void* data, wl_seat*, u32 capabilities) {
              static_cast<KeyboardMouse*>(data)->m_capabilities = capabilities;
            },
        .name = [](void*, wl_seat*, const char*) {},
    };
    wl_seat_add_listener(m_seat, &seat_listener, this);
    if (!m_connection->Roundtrip())
      return false;

    if (m_capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
      InitializeKeyboard();
    if (m_capabilities & WL_SEAT_CAPABILITY_POINTER)
      InitializePointer();

    // Wait for the keymap, and for the enter events telling us what has focus right now.
    if ((!m_keyboard && !m_pointer) || !m_connection->Roundtrip())
      return false;

    AddInputs();
    return true;
  }

  void InitializeKeyboard()
  {
    m_xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!m_xkb_context)
      return;

    m_keyboard = wl_seat_get_keyboard(m_seat);
    static constexpr wl_keyboard_listener listener = {
        .keymap = [](void* data, wl_keyboard*, u32 format, int fd,
                     u32 size) { static_cast<KeyboardMouse*>(data)->OnKeymap(format, fd, size); },
        .enter =
            [](void* data, wl_keyboard*, u32, wl_surface*, wl_array* keys) {
              // The keys which are already held down when we get focus
              auto* self = static_cast<KeyboardMouse*>(data);
              const auto* begin = static_cast<const u32*>(keys->data);
              for (const u32* key = begin; key < begin + keys->size / sizeof(u32); ++key)
                SetKey(self->m_state.keyboard, *key + 8, true);
            },
        .leave =
            [](void* data, wl_keyboard*, u32, wl_surface*) {
              // We won't be told about the keys being released
              for (auto& word : static_cast<KeyboardMouse*>(data)->m_state.keyboard)
                word.store(0, std::memory_order_relaxed);
            },
        .key =
            [](void* data, wl_keyboard*, u32, u32, u32 key, u32 state) {
              SetKey(static_cast<KeyboardMouse*>(data)->m_state.keyboard, key + 8,
                     state == WL_KEYBOARD_KEY_STATE_PRESSED);
            },
        .modifiers = [](void*, wl_keyboard*, u32, u32, u32, u32, u32) {},
        .repeat_info = [](void*, wl_keyboard*, s32, s32) {},
    };
    wl_keyboard_add_listener(m_keyboard, &listener, this);
  }

  void InitializePointer()
  {
    m_pointer = wl_seat_get_pointer(m_seat);
    static constexpr wl_pointer_listener listener = {
        .enter =
            [](void* data, wl_pointer*, u32 serial, wl_surface* surface, wl_fixed_t x,
               wl_fixed_t y) {
              auto* self = static_cast<KeyboardMouse*>(data);
              self->m_enter_serial = serial;
              self->m_pointer_over = surface == self->m_render_surface;
              if (self->m_pointer_over)
                self->OnPointerPosition(x, y);
            },
        .leave =
            [](void* data, wl_pointer*, u32, wl_surface*) {
              auto* self = static_cast<KeyboardMouse*>(data);
              self->m_pointer_over = false;
              // We won't be told about the buttons being released
              self->m_buttons = 0;
            },
        .motion =
            [](void* data, wl_pointer*, u32, wl_fixed_t x, wl_fixed_t y) {
              auto* self = static_cast<KeyboardMouse*>(data);
              if (self->m_pointer_over)
                self->OnPointerPosition(x, y);
            },
        .button =
            [](void* data, wl_pointer*, u32, u32, u32 button, u32 state) {
              auto* self = static_cast<KeyboardMouse*>(data);
              const u32 index = GetButtonIndex(button);
              if (index >= 32)
                return;
              if (state == WL_POINTER_BUTTON_STATE_PRESSED)
                self->m_buttons |= u32(1) << index;
              else
                self->m_buttons &= ~(u32(1) << index);
            },
        .axis =
            [](void* data, wl_pointer*, u32, u32 axis, wl_fixed_t value) {
              if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
              {
                static_cast<KeyboardMouse*>(data)->m_scroll_continuous += wl_fixed_to_double(value);
              }
            },
        .frame =
            [](void* data, wl_pointer*) {
              // Scroll wheels report whole clicks, other devices report distances. A frame holds
              // only one of the two kinds, with the clicks taking precedence if both are there.
              auto* self = static_cast<KeyboardMouse*>(data);
              double clicks = self->m_scroll_clicks;
              if (clicks == 0.0)
                clicks = self->m_scroll_continuous / CONTINUOUS_SCROLL_UNITS_PER_CLICK;
              self->m_delta.z += static_cast<float>(clicks) * SCROLL_UNITS_PER_CLICK;
              self->m_scroll_clicks = 0.0;
              self->m_scroll_continuous = 0.0;
            },
        .axis_source = [](void*, wl_pointer*, u32) {},
        .axis_stop = [](void*, wl_pointer*, u32, u32) {},
        .axis_discrete = [](void*, wl_pointer*, u32, s32) {},
        .axis_value120 =
            [](void* data, wl_pointer*, u32 axis, s32 value120) {
              if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
                static_cast<KeyboardMouse*>(data)->m_scroll_clicks += value120 / 120.0;
            },
        .axis_relative_direction = [](void*, wl_pointer*, u32, u32) {},
    };
    wl_pointer_add_listener(m_pointer, &listener, this);

    m_warp = static_cast<wp_pointer_warp_v1*>(m_connection->Bind(wp_pointer_warp_v1_interface, 1));

    m_relative_pointer_manager = static_cast<zwp_relative_pointer_manager_v1*>(
        m_connection->Bind(zwp_relative_pointer_manager_v1_interface, 1));
    if (m_relative_pointer_manager)
    {
      m_relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
          m_relative_pointer_manager, m_pointer);
      static constexpr zwp_relative_pointer_v1_listener relative_listener = {
          .relative_motion =
              [](void* data, zwp_relative_pointer_v1*, u32, u32, wl_fixed_t, wl_fixed_t,
                 wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel) {
                auto* self = static_cast<KeyboardMouse*>(data);
                // Don't react to the mouse being used in the other windows
                if (!self->m_pointer_over)
                  return;
                self->m_delta.x += static_cast<float>(wl_fixed_to_double(dx_unaccel));
                self->m_delta.y += static_cast<float>(wl_fixed_to_double(dy_unaccel));
              },
      };
      zwp_relative_pointer_v1_add_listener(m_relative_pointer, &relative_listener, this);
    }
    else
    {
      INFO_LOG_FMT(CONTROLLERINTERFACE,
                   "The Wayland compositor does not support relative pointer motion. The mouse "
                   "axis controls will not work.");
    }
  }

  void OnPointerPosition(wl_fixed_t x, wl_fixed_t y)
  {
    m_pointer_x = wl_fixed_to_double(x);
    m_pointer_y = wl_fixed_to_double(y);
  }

  void OnKeymap(u32 format, int fd, u32 size)
  {
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || m_keymap)
    {
      // Keys are identified by their keycodes, so the first keymap names them for good. Changing
      // layouts only changes what Qt delivers as text.
      close(fd);
      return;
    }

    void* const map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED)
      return;

    m_keymap = xkb_keymap_new_from_string(m_xkb_context, static_cast<const char*>(map),
                                          XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, size);
    if (!m_keymap)
      WARN_LOG_FMT(CONTROLLERINTERFACE, "Failed to compile the Wayland keymap");
  }

  void CenterMouse()
  {
    const Common::Vec2 size = g_controller_interface.GetRenderSurfaceLogicalSize();
    const double center_x = size.x / 2;
    const double center_y = size.y / 2;

    if (m_warp && m_pointer_over)
    {
      // The compositor may refuse, in which case the pointer stays where it is. We keep going
      // with the position we asked for to stay consistent with the usual case.
      wp_pointer_warp_v1_warp_pointer(m_warp, m_render_surface, m_pointer,
                                      wl_fixed_from_double(center_x),
                                      wl_fixed_from_double(center_y), m_enter_serial);
      m_connection->Flush();
      m_origin_x = m_origin_y = 0.0;
      m_pointer_x = center_x;
      m_pointer_y = center_y;
    }
    else
    {
      // Make the controls read "centered" for the current pointer position.
      m_origin_x = m_pointer_over ? m_pointer_x - center_x : 0.0;
      m_origin_y = m_pointer_over ? m_pointer_y - center_y : 0.0;
      if (!m_pointer_over)
      {
        m_pointer_x = center_x;
        m_pointer_y = center_y;
      }
    }

    g_controller_interface.SetMouseCenteringRequested(false);
  }

  void AddInputs()
  {
    if (m_keymap)
    {
      // Temporary map to combine keycodes with the same name.
      std::vector<std::pair<std::string, Key*>> keys;

      const xkb_keycode_t min_keycode = xkb_keymap_min_keycode(m_keymap);
      const xkb_keycode_t max_keycode = std::min(xkb_keymap_max_keycode(m_keymap), MAX_KEYCODE - 1);
      for (xkb_keycode_t keycode = min_keycode; keycode <= max_keycode; ++keycode)
      {
        const std::string name = GetNameForKeycode(keycode);
        if (name.empty())
          continue;

        const auto it = std::ranges::find(keys, name, &decltype(keys)::value_type::first);
        if (it == keys.end())
        {
          auto* key = new Key(name, keycode, &m_state.keyboard);
          AddInput(key);
          keys.emplace_back(name, key);
        }
        else
        {
          it->second->AddKeyCode(keycode);
        }
      }

      // Add combined left/right modifiers with consistent naming across platforms.
      AddCombinedInput("Alt", {"Alt_L", "Alt_R"});
      AddCombinedInput("Shift", {"Shift_L", "Shift_R"});
      AddCombinedInput("Ctrl", {"Control_L", "Control_R"});
    }

    // Mouse Buttons
    for (u32 i = 0; i < 32; ++i)
      AddInput(new Button(i, &m_state.buttons));

    // Mouse Cursor, X-/+ and Y-/+
    for (int i = 0; i != 4; ++i)
    {
      AddInput(new DirectionalInput("Cursor", (i & 2) ? 1 : 0, i & 1,
                                    (i & 2) ? &m_state.cursor.y : &m_state.cursor.x, 1.0f));
    }

    // Mouse Axis, X-/+, Y-/+ and Z-/+
    AddInput(new DirectionalInput("Axis", 0, false, &m_state.axis.x, MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("Axis", 0, true, &m_state.axis.x, MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("Axis", 1, false, &m_state.axis.y, MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("Axis", 1, true, &m_state.axis.y, MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("Axis", 2, false, &m_state.axis.z, MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("Axis", 2, true, &m_state.axis.z, MOUSE_AXIS_SENSITIVITY));

    // Relative Mouse, X-/+, Y-/+ and Z-/+
    AddInput(new DirectionalInput("RelativeMouse", 0, false, &m_state.relative_mouse.x,
                                  MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("RelativeMouse", 0, true, &m_state.relative_mouse.x,
                                  MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("RelativeMouse", 1, false, &m_state.relative_mouse.y,
                                  MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("RelativeMouse", 1, true, &m_state.relative_mouse.y,
                                  MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("RelativeMouse", 2, false, &m_state.relative_mouse.z,
                                  MOUSE_AXIS_SENSITIVITY));
    AddInput(new DirectionalInput("RelativeMouse", 2, true, &m_state.relative_mouse.z,
                                  MOUSE_AXIS_SENSITIVITY));
  }

  // Names keys like the XInput2 backend: the first keysym of the key in the first layout, with
  // letters in upper case.
  std::string GetNameForKeycode(xkb_keycode_t keycode) const
  {
    const xkb_level_index_t num_levels = xkb_keymap_num_levels_for_key(m_keymap, keycode, 0);
    xkb_keysym_t keysym = XKB_KEY_NoSymbol;
    for (xkb_level_index_t level = 0; level < std::min<xkb_level_index_t>(num_levels, 8); ++level)
    {
      const xkb_keysym_t* syms = nullptr;
      if (xkb_keymap_key_get_syms_by_level(m_keymap, keycode, 0, level, &syms) > 0 &&
          syms[0] != XKB_KEY_NoSymbol)
      {
        keysym = syms[0];
        break;
      }
    }

    if (keysym >= 'a' && keysym <= 'z')
      keysym -= 'a' - 'A';

    // 0x0110ffff is the top of the unicode character range according to keysymdef.h, although it
    // is probably more than we need.
    if (keysym == XKB_KEY_NoSymbol || keysym > 0x0110ffff)
      return {};

    std::array<char, 64> name;
    if (xkb_keysym_get_name(keysym, name.data(), name.size()) <= 0)
      return {};
    return name.data();
  }

  const std::shared_ptr<Common::Wayland::Connection> m_connection;
  wl_surface* const m_render_surface;

  wl_seat* m_seat = nullptr;
  wl_keyboard* m_keyboard = nullptr;
  wl_pointer* m_pointer = nullptr;
  wp_pointer_warp_v1* m_warp = nullptr;
  zwp_relative_pointer_manager_v1* m_relative_pointer_manager = nullptr;
  zwp_relative_pointer_v1* m_relative_pointer = nullptr;

  xkb_context* m_xkb_context = nullptr;
  xkb_keymap* m_keymap = nullptr;

  u32 m_capabilities = 0;

  // Everything below is written by the event listeners and so only used while updating input.
  bool m_pointer_over = false;
  u32 m_enter_serial = 0;
  u32 m_buttons = 0;
  double m_pointer_x = 0.0;
  double m_pointer_y = 0.0;
  // The position which reads as the top left of the cursor controls, see CenterMouse().
  double m_origin_x = 0.0;
  double m_origin_y = 0.0;
  double m_scroll_clicks = 0.0;
  double m_scroll_continuous = 0.0;
  // Relative motion since the last update
  struct
  {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
  } m_delta;

  // This is read from other threads.
  State m_state;
};

class InputBackend final : public ciface::InputBackend
{
public:
  using ciface::InputBackend::InputBackend;

  void PopulateDevices() override
  {
    const WindowSystemInfo wsi = GetControllerInterface().GetWindowSystemInfo();
    if (wsi.type != WindowSystemType::Wayland || !wsi.display_connection || !wsi.render_window)
      return;

    std::shared_ptr<Common::Wayland::Connection> connection =
        Common::Wayland::Connection::Create(static_cast<wl_display*>(wsi.display_connection));
    if (!connection)
      return;

    // Qt makes its own seat the one with the keyboard focus, but a seat is cheap, so use all.
    // Copy because the connection's list grows when we roundtrip.
    const std::vector<Common::Wayland::Connection::Global> globals = connection->GetGlobals();
    for (const auto& global : globals)
    {
      if (global.interface != wl_seat_interface.name)
        continue;

      if (auto device = KeyboardMouse::Create(connection, global,
                                              static_cast<wl_surface*>(wsi.render_window)))
        GetControllerInterface().AddDevice(std::move(device));
    }
  }

  void HandleWindowChange() override
  {
    GetControllerInterface().RemoveDevice(
        [](const auto* device) { return device->GetSource() == SOURCE_NAME; }, true);

    PopulateDevices();
  }
};
}  // namespace

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}
}  // namespace ciface::Wayland
