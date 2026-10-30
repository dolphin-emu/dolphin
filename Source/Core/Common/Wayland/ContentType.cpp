// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Wayland/ContentType.h"

#include <memory>

#include <content-type-v1-client-protocol.h>

#include "Common/Logging/Log.h"
#include "Common/Wayland/Connection.h"

namespace Common::Wayland
{
namespace
{
struct State
{
  ~State()
  {
    // The proxies have to go before the connection that owns their queue.
    Reset();
    if (manager)
      wp_content_type_manager_v1_destroy(manager);
  }

  void Reset()
  {
    if (content_type)
      wp_content_type_v1_destroy(content_type);
    content_type = nullptr;
    surface = nullptr;
  }

  std::unique_ptr<Connection> connection;
  wl_display* display = nullptr;
  wp_content_type_manager_v1* manager = nullptr;
  wp_content_type_v1* content_type = nullptr;
  wl_surface* surface = nullptr;
};

std::unique_ptr<State> s_state;
}  // namespace

void SetSurfaceContentTypeGame(wl_display* display, wl_surface* surface)
{
  if (!display || !surface)
    return;

  if (s_state && s_state->display != display)
    s_state.reset();

  if (!s_state)
  {
    auto state = std::make_unique<State>();
    state->display = display;
    state->connection = Connection::Create(display);
    if (!state->connection)
      return;
    state->manager = static_cast<wp_content_type_manager_v1*>(
        state->connection->Bind(wp_content_type_manager_v1_interface, 1));
    if (!state->manager)
    {
      INFO_LOG_FMT(COMMON, "The Wayland compositor does not support content type hints");
      // Keep the state so that we don't enumerate the globals again for every surface.
    }
    s_state = std::move(state);
  }

  if (!s_state->manager || s_state->surface == surface)
    return;

  // A surface can only have one content type object. The old surface may already be gone, in
  // which case the old object is inert but still has to be destroyed.
  s_state->Reset();

  s_state->content_type =
      wp_content_type_manager_v1_get_surface_content_type(s_state->manager, surface);
  wp_content_type_v1_set_content_type(s_state->content_type, WP_CONTENT_TYPE_V1_TYPE_GAME);
  s_state->surface = surface;
  s_state->connection->Flush();
}
}  // namespace Common::Wayland
