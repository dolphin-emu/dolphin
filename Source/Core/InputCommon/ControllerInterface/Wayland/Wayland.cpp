// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>

#include "InputCommon/ControllerInterface/Wayland/Wayland.h"

#include "Common/Assert.h"
#include "Common/StringUtil.h"

// Mouse axis control output is simply divided by this number. In practice,
// that just means you can use a smaller "dead zone" if you bind axis controls
// to a joystick. No real need to make this customizable.
#define MOUSE_AXIS_SENSITIVITY 8.0f

// The mouse axis controls use a weighted running average. Each frame, the new
// value is the average of the old value and the amount of relative mouse
// motion during that frame. The old value is weighted by a ratio of
// MOUSE_AXIS_SMOOTHING:1 compared to the new value. Increasing
// MOUSE_AXIS_SMOOTHING makes the controls smoother, decreasing it makes them
// more responsive. This might be useful as a user-customizable option.
#define MOUSE_AXIS_SMOOTHING 1.5f

namespace ciface
{
namespace Wayland
{
void PopulateDevices(const WindowSystemInfo& wsi)
{
  std::shared_ptr<KeyboardMouse> dev = std::make_shared<KeyboardMouse>();
  if (!dev->Initialize(wsi))
    return;

  g_controller_interface.AddDevice(dev);
}

KeyboardMouse::KeyboardMouse() = default;

KeyboardMouse::~KeyboardMouse()
{
  if (m_xkb_state)
    xkb_state_unref(m_xkb_state);
  if (m_xkb_keymap)
    xkb_keymap_unref(m_xkb_keymap);
  if (m_wl_keyboard)
    wl_keyboard_destroy(m_wl_keyboard);
  if (m_wl_pointer)
    wl_pointer_destroy(m_wl_pointer);
  if (m_wl_seat)
    wl_seat_destroy(m_wl_seat);
  if (m_wl_registry)
    wl_registry_destroy(m_wl_registry);
  if (m_xkb_context)
    xkb_context_unref(m_xkb_context);
  if (m_display_proxy)
    wl_proxy_wrapper_destroy(m_display_proxy);
  if (m_event_queue)
    wl_event_queue_destroy(m_event_queue);
}

bool KeyboardMouse::Initialize(const WindowSystemInfo& wsi)
{
  m_display = static_cast<wl_display*>(wsi.display_connection);
  m_window_width = wsi.render_surface_width;
  m_window_height = wsi.render_surface_height;
  m_event_queue = wl_display_create_queue(m_display);
  if (!m_event_queue)
    return false;

  // As UpdateInput() can be called from the CPU, we don't want to clash with the main (UI) thread.
  // Therefore, we create a second event queue for retrieving keyboard/mouse events.
  m_display_proxy = static_cast<wl_display*>(wl_proxy_create_wrapper(m_display));
  if (!m_display_proxy)
    return false;

  wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(m_display_proxy), m_event_queue);
  m_xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!m_xkb_context)
    return false;

  static const wl_registry_listener registry_listener = {GlobalRegistryHandler,
                                                         GlobalRegistryRemover};
  m_wl_registry = wl_display_get_registry(m_display_proxy);
  wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(m_display_proxy), m_event_queue);
  wl_registry_add_listener(m_wl_registry, &registry_listener, this);

  // Call back to registry listener to get compositor/shell.
  wl_display_dispatch_queue(m_display, m_event_queue);
  wl_display_roundtrip_queue(m_display, m_event_queue);
  // We need to do this twice so the seat listener is called.
  wl_display_dispatch_queue(m_display, m_event_queue);

  // If we have a keyboard, keep dispatching until we get the keymap.
  if (m_wl_keyboard)
  {
    while (!m_xkb_keymap)
    {
      if (wl_display_dispatch_queue(m_display, m_event_queue) == -1)
        break;
    }
  }

  // Keyboard Keys
  if (m_xkb_keymap)
    AddKeyInputs();

  // Mouse Buttons
  if (m_wl_pointer)
  {
    for (int i = 0; i < 32; i++)
      AddInput(new Button(i, &m_state.buttons));

    // Mouse Cursor, X-/+ and Y-/+
    for (int i = 0; i != 4; ++i)
      AddInput(new Cursor(!!(i & 2), !!(i & 1), (i & 2) ? &m_state.cursor.y : &m_state.cursor.x));

    // Mouse Axis, X-/+ and Y-/+
    for (int i = 0; i != 4; ++i)
      AddInput(new Axis(!!(i & 2), !!(i & 1), (i & 2) ? &m_state.axis.y : &m_state.axis.x));
  }

  return true;
}

void KeyboardMouse::UpdateInput()
{
  while (wl_display_prepare_read_queue(m_display, m_event_queue) != 0)
    wl_display_dispatch_queue_pending(m_display, m_event_queue);

  wl_display_read_events(m_display);
  wl_display_dispatch_queue_pending(m_display, m_event_queue);
}

void KeyboardMouse::OnWindowResized(int width, int height)
{
  m_window_width = width;
  m_window_height = height;
}

void KeyboardMouse::PointerEnter(void* data, wl_pointer* pointer, uint32_t serial,
                                 wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
}

void KeyboardMouse::PointerLeave(void* data, wl_pointer* pointer, uint32_t serial,
                                 wl_surface* surface)
{
}

void KeyboardMouse::PointerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t x,
                                  wl_fixed_t y)
{
  KeyboardMouse* kbm = static_cast<KeyboardMouse*>(data);
  const float pos_x = static_cast<float>(wl_fixed_to_double(x));
  const float pos_y = static_cast<float>(wl_fixed_to_double(y));
  const float delta_x = pos_x - kbm->m_state.cursor.x;
  const float delta_y = pos_y - kbm->m_state.cursor.y;

  // apply axis smoothing
  kbm->m_state.axis.x *= MOUSE_AXIS_SMOOTHING;
  kbm->m_state.axis.x += delta_x;
  kbm->m_state.axis.x /= MOUSE_AXIS_SMOOTHING + 1.0f;
  kbm->m_state.axis.y *= MOUSE_AXIS_SMOOTHING;
  kbm->m_state.axis.y += delta_y;
  kbm->m_state.axis.y /= MOUSE_AXIS_SMOOTHING + 1.0f;

  kbm->m_state.cursor.x = (pos_x / static_cast<float>(kbm->m_window_width) * 2.0f) - 1.0f;
  kbm->m_state.cursor.y = (pos_y / static_cast<float>(kbm->m_window_height) * 2.0f) - 1.0f;
}

void KeyboardMouse::PointerButton(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time,
                                  uint32_t button, uint32_t state)
{
  KeyboardMouse* kbm = static_cast<KeyboardMouse*>(data);
  if (button < BTN_MOUSE || (button - BTN_MOUSE) >= 32)
    return;

  const u32 mask = 1u << (button - BTN_MOUSE);
  if (state == WL_POINTER_BUTTON_STATE_PRESSED)
    kbm->m_state.buttons |= mask;
  else if (state == WL_POINTER_BUTTON_STATE_RELEASED)
    kbm->m_state.buttons &= ~mask;
}

void KeyboardMouse::PointerAxis(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis,
                                wl_fixed_t value)
{
  // TODO: Wheel support.
  // KeyboardMouse* kbm = static_cast<KeyboardMouse*>(data);
  // printf("pointer axis %u %f\n", axis, wl_fixed_to_double(value));
}

void KeyboardMouse::KeyboardKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int32_t fd,
                                   uint32_t size)
{
  KeyboardMouse* kbm = static_cast<KeyboardMouse*>(data);
  char* keymap_string = static_cast<char*>(mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0));
  if (kbm->m_xkb_keymap)
    xkb_keymap_unref(kbm->m_xkb_keymap);
  kbm->m_xkb_keymap = xkb_keymap_new_from_string(
      kbm->m_xkb_context, keymap_string, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(keymap_string, size);
  close(fd);
  if (kbm->m_xkb_state)
    xkb_state_unref(kbm->m_xkb_state);
  kbm->m_xkb_state = xkb_state_new(kbm->m_xkb_keymap);
}

void KeyboardMouse::KeyboardEnter(void* data, wl_keyboard* keyboard, uint32_t serial,
                                  wl_surface* surface, wl_array* keys)
{
}

void KeyboardMouse::KeyboardLeave(void* data, wl_keyboard* keyboard, uint32_t serial,
                                  wl_surface* surface)
{
}

void KeyboardMouse::KeyboardKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time,
                                uint32_t key, uint32_t state)
{
  KeyboardMouse* kbm = static_cast<KeyboardMouse*>(data);

  const xkb_keycode_t keycode = static_cast<xkb_keycode_t>(key + 8);
  if (keycode < kbm->m_min_keycode && keycode > kbm->m_max_keycode)
    return;

  const u32 bit_index = static_cast<u32>(keycode - kbm->m_min_keycode);
  const u32 array_index = bit_index / 32;
  const u32 mask = 1u << (bit_index % 32);
  if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
    kbm->m_state.keyboard[array_index] |= mask;
  else if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
    kbm->m_state.keyboard[array_index] &= ~mask;
}

void KeyboardMouse::KeyboardModifiers(void* data, wl_keyboard* keyboard, uint32_t serial,
                                      uint32_t mods_depressed, uint32_t mods_latched,
                                      uint32_t mods_locked, uint32_t group)
{
  KeyboardMouse* kbm = static_cast<KeyboardMouse*>(data);
  xkb_state_update_mask(kbm->m_xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void KeyboardMouse::AddKeyInputs()
{
  m_min_keycode = xkb_keymap_min_keycode(m_xkb_keymap);
  m_max_keycode = xkb_keymap_max_keycode(m_xkb_keymap);
  ASSERT(m_max_keycode >= m_min_keycode);

  // Allocate bitmask for key state.
  const u32 num_keycodes = static_cast<u32>(m_max_keycode - m_min_keycode) + 1;
  m_state.keyboard.resize((num_keycodes + 31) / 32);

  for (xkb_keycode_t keycode = m_min_keycode; keycode <= m_max_keycode; keycode++)
  {
    const xkb_layout_index_t num_layouts = xkb_keymap_num_layouts_for_key(m_xkb_keymap, keycode);
    if (num_layouts == 0)
      continue;

    // Take the first layout which we find a valid keysym for.
    Key* key = nullptr;
    for (xkb_layout_index_t layout = 0; layout < num_layouts && !key; layout++)
    {
      const xkb_level_index_t num_levels =
          xkb_keymap_num_levels_for_key(m_xkb_keymap, keycode, layout);
      if (num_levels == 0)
        continue;

      // Take the first level which we find a valid keysym for.
      for (xkb_level_index_t level = 0; level < num_levels; level++)
      {
        const xkb_keysym_t* keysyms;
        int num_syms =
            xkb_keymap_key_get_syms_by_level(m_xkb_keymap, keycode, layout, level, &keysyms);
        if (num_syms == 0)
          continue;

        // Just take the first. Should only be one in most cases anyway.
        const xkb_keysym_t keysym = xkb_keysym_to_upper(keysyms[0]);

        char keysym_name_buf[64];
        if (xkb_keysym_get_name(keysym, keysym_name_buf, sizeof(keysym_name_buf)) <= 0)
          continue;

        const u32 bit_index = static_cast<u32>(keycode - m_min_keycode);
        const u32 array_index = bit_index / 32;
        const u32 mask = 1u << (bit_index % 32);
        key = new Key(keysym_name_buf, keycode, m_state, array_index, mask);
        AddInput(key);
        break;
      }
    }
  }
}

void KeyboardMouse::SeatCapabilities(void* data, wl_seat* seat, uint32_t capabilities)
{
  KeyboardMouse* kbm = static_cast<KeyboardMouse*>(data);
  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
  {
    static const wl_keyboard_listener keyboard_listener = {
        &KeyboardMouse::KeyboardKeymap, &KeyboardMouse::KeyboardEnter,
        &KeyboardMouse::KeyboardLeave, &KeyboardMouse::KeyboardKey,
        &KeyboardMouse::KeyboardModifiers};
    kbm->m_wl_keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(kbm->m_wl_keyboard, &keyboard_listener, kbm);
  }
  if (capabilities & WL_SEAT_CAPABILITY_POINTER)
  {
    static const wl_pointer_listener pointer_listener = {
        &KeyboardMouse::PointerEnter, &KeyboardMouse::PointerLeave, &KeyboardMouse::PointerMotion,
        &KeyboardMouse::PointerButton, &KeyboardMouse::PointerAxis};
    kbm->m_wl_pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(kbm->m_wl_pointer, &pointer_listener, kbm);
  }
}

void KeyboardMouse::GlobalRegistryHandler(void* data, wl_registry* registry, uint32_t id,
                                          const char* interface, uint32_t version)
{
  KeyboardMouse* kbm = static_cast<KeyboardMouse*>(data);
  if (std::strcmp(interface, "wl_seat") == 0)
  {
    static const wl_seat_listener seat_listener = {&KeyboardMouse::SeatCapabilities};
    kbm->m_wl_seat = static_cast<wl_seat*>(wl_registry_bind(registry, id, &wl_seat_interface, 1));
    wl_seat_add_listener(kbm->m_wl_seat, &seat_listener, kbm);
  }
}

void KeyboardMouse::GlobalRegistryRemover(void* data, wl_registry* registry, uint32_t id)
{
}

std::string KeyboardMouse::GetName() const
{
  return "Keyboard and Mouse";
}

std::string KeyboardMouse::GetSource() const
{
  return "Wayland";
}

KeyboardMouse::Key::Key(const std::string& name, const xkb_keycode_t keycode, const State& state,
                        const u32 index, const u32 mask)
    : m_keyname(name), m_keycode(keycode), m_state(state), m_index(index), m_mask(mask)
{
}

ControlState KeyboardMouse::Key::GetState() const
{
  return (m_state.keyboard[m_index] & m_mask) != 0;
}

KeyboardMouse::Button::Button(unsigned int index, unsigned int* buttons)
    : m_buttons(buttons), m_index(index)
{
  m_name = StringFromFormat("Click %d", m_index + 1);
}

ControlState KeyboardMouse::Button::GetState() const
{
  return ((*m_buttons & (1 << m_index)) != 0);
}

KeyboardMouse::Cursor::Cursor(u8 index, bool positive, const float* cursor)
    : m_cursor(cursor), m_index(index), m_positive(positive)
{
  m_name = std::string("Cursor ") + (char)('X' + m_index) + (m_positive ? '+' : '-');
}

ControlState KeyboardMouse::Cursor::GetState() const
{
  return std::max(0.0f, *m_cursor / (m_positive ? 1.0f : -1.0f));
}

KeyboardMouse::Axis::Axis(u8 index, bool positive, const float* axis)
    : m_axis(axis), m_index(index), m_positive(positive)
{
  m_name = std::string("Axis ") + (char)('X' + m_index) + (m_positive ? '+' : '-');
}

ControlState KeyboardMouse::Axis::GetState() const
{
  return std::max(0.0f, *m_axis / (m_positive ? MOUSE_AXIS_SENSITIVITY : -MOUSE_AXIS_SENSITIVITY));
}
}  // namespace Wayland
}  // namespace ciface
