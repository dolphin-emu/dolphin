// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

struct wl_display;
struct wl_surface;

namespace Common::Wayland
{
// Tells the compositor that the surface shows a game (wp_content_type_v1). Compositors use this
// to enable adaptive sync only for games, to pick low latency presentation, and so on.
//
// The hint is applied by the surface's next commit. It stays set for as long as the surface lives
// or until this is called with a different surface. Does nothing if the compositor does not
// support the protocol. Must only be called from one thread (Dolphin calls it from the Qt thread).
void SetSurfaceContentTypeGame(wl_display* display, wl_surface* surface);

// Releases everything SetSurfaceContentTypeGame() created. This has to be called while the
// wl_display is still alive, i.e. before Qt shuts down, as it can't be left to static destruction.
void ShutdownContentType();
}  // namespace Common::Wayland
