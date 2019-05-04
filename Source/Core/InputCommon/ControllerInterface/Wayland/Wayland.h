// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>

#include "Common/CommonTypes.h"
#include "Common/WindowSystemInfo.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface
{
namespace Wayland
{
void PopulateDevices(const WindowSystemInfo& wsi);

class KeyboardMouse : public Core::Device
{
private:
  struct State
  {
    std::vector<u32> keyboard;
    unsigned int buttons;
    struct
    {
      float x, y;
    } cursor, axis;
  };

  class Key : public Input
  {
    friend class KeyboardMouse;

  public:
    std::string GetName() const override { return m_keyname; }
    Key(const std::string& name, const xkb_keycode_t keycode, const State& state, const u32 index,
        const u32 mask);
    ControlState GetState() const override;

  private:
    const std::string m_keyname;
    const xkb_keycode_t m_keycode;
    const State& m_state;
    const u32 m_index;
    const u32 m_mask;
  };

  class Button : public Input
  {
  public:
    std::string GetName() const override { return m_name; }
    Button(unsigned int index, unsigned int* buttons);
    ControlState GetState() const override;

  private:
    const unsigned int* m_buttons;
    const unsigned int m_index;
    std::string m_name;
  };

  class Cursor : public Input
  {
  public:
    std::string GetName() const override { return m_name; }
    bool IsDetectable() const override { return false; }
    Cursor(u8 index, bool positive, const float* cursor);
    ControlState GetState() const override;

  private:
    const float* m_cursor;
    const u8 m_index;
    const bool m_positive;
    std::string m_name;
  };

  class Axis : public Input
  {
  public:
    std::string GetName() const override { return m_name; }
    bool IsDetectable() const override { return false; }
    Axis(u8 index, bool positive, const float* axis);
    ControlState GetState() const override;

  private:
    const float* m_axis;
    const u8 m_index;
    const bool m_positive;
    std::string m_name;
  };

public:
  KeyboardMouse();
  ~KeyboardMouse();

  bool Initialize(const WindowSystemInfo& wsi);

  std::string GetName() const override;
  std::string GetSource() const override;
  void UpdateInput() override;
  void OnWindowResized(int width, int height) override;

private:
  void AddKeyInputs();

  static void PointerEnter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface,
                           wl_fixed_t surface_x, wl_fixed_t surface_y);
  static void PointerLeave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface);
  static void PointerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t x,
                            wl_fixed_t y);
  static void PointerButton(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time,
                            uint32_t button, uint32_t state);
  static void PointerAxis(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis,
                          wl_fixed_t value);
  static void KeyboardKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int32_t fd,
                             uint32_t size);
  static void KeyboardEnter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface,
                            wl_array* keys);
  static void KeyboardLeave(void* data, wl_keyboard* keyboard, uint32_t serial,
                            wl_surface* surface);
  static void KeyboardKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time,
                          uint32_t key, uint32_t state);
  static void KeyboardModifiers(void* data, wl_keyboard* keyboard, uint32_t serial,
                                uint32_t mods_depressed, uint32_t mods_latched,
                                uint32_t mods_locked, uint32_t group);
  static void SeatCapabilities(void* data, wl_seat* seat, uint32_t capabilities);
  static void GlobalRegistryHandler(void* data, wl_registry* registry, uint32_t id,
                                    const char* interface, uint32_t version);
  static void GlobalRegistryRemover(void* data, wl_registry* registry, uint32_t id);

  wl_display* m_display = nullptr;
  wl_display* m_display_proxy = nullptr;
  wl_event_queue* m_event_queue = nullptr;
  wl_registry* m_wl_registry = nullptr;
  wl_seat* m_wl_seat = nullptr;
  wl_keyboard* m_wl_keyboard = nullptr;
  wl_pointer* m_wl_pointer = nullptr;
  xkb_context* m_xkb_context = nullptr;
  xkb_keymap* m_xkb_keymap = nullptr;
  xkb_state* m_xkb_state = nullptr;
  State m_state = {};
  xkb_keycode_t m_min_keycode = {};
  xkb_keycode_t m_max_keycode = {};
  int m_window_width = 1;
  int m_window_height = 1;
};
}  // namespace Wayland
}  // namespace ciface
