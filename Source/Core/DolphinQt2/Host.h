// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>
#include <atomic>

#include "Common/CommonTypes.h"

// Singleton that talks to the Core via the interface defined in Core/Host.h.
// Because Host_* calls might come from different threads than the MainWindow,
// the Host class communicates with it via signals/slots only.

// Many of the Host_* functions are ignored, and some shouldn't exist.
class Host final : public QObject
{
  Q_OBJECT

public:
  static Host* GetInstance();

  void* GetRenderHandle();
  bool GetRenderFocus();
  bool GetRenderFullscreen();

  void SetRenderHandle(void* handle);
  void SetRenderFocus(bool focus);
  void SetRenderFullscreen(bool fullscreen);
  void ResizeSurface(int new_width, int new_height);
  void MoveSurface(int x, int y);

  double GetCursorX() const;
  double GetCursorY() const;
signals:
  void RequestTitle(const QString& title);
  void RequestStop();
  void RequestRenderSize(int w, int h);
  void UpdateProgressDialog(QString label, int position, int maximum);

private:
  Host();

  std::atomic<void*> m_render_handle;
  std::atomic<bool> m_render_focus;
  std::atomic<bool> m_render_fullscreen;
  std::atomic<u32> m_surface_x;
  std::atomic<u32> m_surface_y;
  std::atomic<u32> m_surface_width;
  std::atomic<u32> m_surface_height;
};
