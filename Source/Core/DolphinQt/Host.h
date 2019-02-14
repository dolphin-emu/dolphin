// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  static Host* GetInstance();

  bool GetRenderFocus() const;
  bool IsFullscreenEnabled() const;
  bool IsFullscreenActive() const;

  void SetRenderHandle(void* handle);
  void SetRenderFocus(bool focus);

  // changes the enabled state
  void SetFullscreenEnabled(bool enabled);

  // synchronizes the enabled state with the real state
  void UpdateFullscreen(bool disable_temporarily = false);

  void ResizeSurface(int new_width, int new_height);
  void RequestNotifyMapLoaded();

signals:
  void RequestTitle(const QString& title);
  void RequestStop();
  void RequestRenderSize(int w, int h);
  void RequestFullscreen(bool enabled, float target_refresh_rate);
  void UpdateProgressDialog(QString label, int position, int maximum);
  void UpdateDisasmDialog();
  void NotifyMapLoaded();

private:
  Host();

  std::atomic<void*> m_render_handle{nullptr};
  std::atomic_bool m_render_focus{false};

  // Remains true if fullscreen is enabled, but focus is lost.
  std::atomic_bool m_fullscreen_enabled{false};
};
