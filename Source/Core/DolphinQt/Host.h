// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include <QObject>

// Singleton that talks to the Core via the interface defined in Core/Host.h.
// Because Host_* calls might come from different threads than the MainWindow,
// the Host class communicates with it via signals/slots only.

// Many of the Host_* functions are ignored, and some shouldn't exist.
class Host final : public QObject
{
  Q_OBJECT

public:
  ~Host();

  static Host* GetInstance();

  void DeclareAsHostThread();
  bool IsHostThread();

  bool GetRenderFocus();
  bool GetRenderFullFocus();
  bool GetRenderFullscreen();
  bool GetGBAFocus();

  void SetMainWindowHandle(void* handle);
  void SetRenderHandle(void* handle);
  void SetRenderFocus(bool focus);
  void SetRenderFullFocus(bool focus);
  void SetRenderFullscreen(bool fullscreen);
  void ResizeSurface(int new_width, int new_height);
  void RequestNotifyMapLoaded();

signals:
  void RequestTitle(const QString& title);
  void RequestStop();
  void RequestRenderSize(int w, int h);
  void UpdateDisasmDialog();
  void NotifyMapLoaded();

private:
  Host();

  std::atomic<void*> m_render_handle{nullptr};
  std::atomic<void*> m_main_window_handle{nullptr};
  std::atomic<bool> m_render_to_main{false};
  std::atomic<bool> m_render_focus{false};
  std::atomic<bool> m_render_full_focus{false};
  std::atomic<bool> m_render_fullscreen{false};
};
