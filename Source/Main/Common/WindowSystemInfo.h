// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

enum class WindowSystemType
{
  Headless,
  Windows,
  MacOS,
  Android,
  X11,
  Wayland
};

struct WindowSystemInfo
{
  WindowSystemInfo() = default;
  WindowSystemInfo(WindowSystemType type_, void* display_connection_, void* render_surface_)
      : type(type_), display_connection(display_connection_), render_surface(render_surface_)
  {
  }

  // Window system type. Determines which GL context or Vulkan WSI is used.
  WindowSystemType type = WindowSystemType::Headless;

  // Connection to a display server. This is used on X11 and Wayland platforms.
  void* display_connection = nullptr;

  // Render surface. This is a pointer to the native window handle, which depends
  // on the platform. e.g. HWND for Windows, Window for X11. If the surface is
  // set to nullptr, the video backend will run in headless mode.
  void* render_surface = nullptr;
};
