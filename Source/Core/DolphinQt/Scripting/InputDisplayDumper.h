// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <mutex>

#include <QHash>
#include <QImage>
#include <QString>

#include "Common/CommonTypes.h"
#include "Core/API/Events.h"
#include "DolphinQt/Scripting/InputDisplaySkin.h"
#include "InputCommon/GCPadStatus.h"

#if defined(HAVE_FFMPEG)
#include "VideoCommon/FrameDumpFFMpeg.h"
#endif

// Captures the input-display skin to a frame-locked sidecar video, one frame per emulation frame.
// Hooks API::Events::FrameAdvance and renders the skin into an off-widget QImage.
class InputDisplayDumper
{
public:
  explicit InputDisplayDumper(const GCSkin& skin, int port = 0);
  ~InputDisplayDumper();

  void Start();
  void Stop();

private:
  void OnFrameAdvance();
  QImage RenderSkin(const GCPadStatus& pad, bool connected);
  const QImage& LoadImage(const QString& path);
  const QImage& TintedImage(const QString& path, u32 argb);

  const GCSkin& m_skin;
  int m_port;
  std::atomic<bool> m_active{false};
  API::ListenerID<API::Events::FrameAdvance> m_listener;

  std::mutex m_dump_mutex;  // guards the encoder against a concurrent Stop()
#if defined(HAVE_FFMPEG)
  FFMpegFrameDump m_dump;
  u32 m_frame_count = 0;
#endif

  QHash<QString, QImage> m_images;
  QHash<QString, QImage> m_tinted;
};
