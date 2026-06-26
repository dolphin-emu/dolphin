// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/InputDisplayDumper.h"

#include <QPainter>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/System.h"
#include "InputCommon/GCPadStatus.h"

static QColor ArgbToColor(u32 argb)
{
  return QColor((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
}

static bool PadButton(const GCPadStatus& pad, const QString& key)
{
  if (key == QStringLiteral("A"))     return pad.button & PAD_BUTTON_A;
  if (key == QStringLiteral("B"))     return pad.button & PAD_BUTTON_B;
  if (key == QStringLiteral("X"))     return pad.button & PAD_BUTTON_X;
  if (key == QStringLiteral("Y"))     return pad.button & PAD_BUTTON_Y;
  if (key == QStringLiteral("Z"))     return pad.button & PAD_TRIGGER_Z;
  if (key == QStringLiteral("L"))     return pad.button & PAD_TRIGGER_L;
  if (key == QStringLiteral("R"))     return pad.button & PAD_TRIGGER_R;
  if (key == QStringLiteral("Start")) return pad.button & PAD_BUTTON_START;
  if (key == QStringLiteral("Left"))  return pad.button & PAD_BUTTON_LEFT;
  if (key == QStringLiteral("Right")) return pad.button & PAD_BUTTON_RIGHT;
  if (key == QStringLiteral("Up"))    return pad.button & PAD_BUTTON_UP;
  if (key == QStringLiteral("Down"))  return pad.button & PAD_BUTTON_DOWN;
  return false;
}

static u8 PadAxis(const GCPadStatus& pad, const QString& key)
{
  if (key.isEmpty())
    return 0;
  if (key == QStringLiteral("StickX"))       return pad.stickX;
  if (key == QStringLiteral("StickY"))       return pad.stickY;
  if (key == QStringLiteral("CStickX"))      return pad.substickX;
  if (key == QStringLiteral("CStickY"))      return pad.substickY;
  if (key == QStringLiteral("TriggerLeft"))  return pad.triggerLeft;
  if (key == QStringLiteral("TriggerRight")) return pad.triggerRight;
  return 128;
}

static bool OverlayActive(const GCSkinOverlay& ov, const GCPadStatus& pad, bool connected)
{
  if (ov.when == QStringLiteral("disconnected"))
    return !connected;
  if (ov.when == QStringLiteral("connected"))
    return connected;
  return PadButton(pad, ov.when);
}

InputDisplayDumper::InputDisplayDumper(const GCSkin& skin, int port) : m_skin(skin), m_port(port)
{
  m_listener = API::GetEventHub().ListenEvent<API::Events::FrameAdvance>(
      [this](const API::Events::FrameAdvance&) { OnFrameAdvance(); });
}

InputDisplayDumper::~InputDisplayDumper()
{
  Stop();
  API::GetEventHub().UnlistenEvent(m_listener);
}

void InputDisplayDumper::Start()
{
  m_active = true;
}

void InputDisplayDumper::Stop()
{
  m_active = false;
  std::lock_guard lock(m_dump_mutex);
#if defined(HAVE_FFMPEG)
  if (m_dump.IsStarted())
    m_dump.Stop();
  m_frame_count = 0;
#endif
}

const QImage& InputDisplayDumper::LoadImage(const QString& path)
{
  auto it = m_images.find(path);
  if (it == m_images.end())
    it = m_images.insert(path, QImage(path));
  return *it;
}

// Multiply-tints a texture: black stays black, white becomes the tint. Mirrors ScriptCanvasWidget,
// but on QImage so it can run off the GUI thread (QPixmap can't).
const QImage& InputDisplayDumper::TintedImage(const QString& path, u32 argb)
{
  const QString key = path + QLatin1Char('|') + QString::number(argb, 16);
  auto it = m_tinted.find(key);
  if (it != m_tinted.end())
    return *it;

  QImage tinted = LoadImage(path);
  if (!tinted.isNull())
  {
    tinted = tinted.convertToFormat(QImage::Format_ARGB32);
    QPainter p(&tinted);
    p.setCompositionMode(QPainter::CompositionMode_Multiply);
    p.fillRect(tinted.rect(), ArgbToColor(argb | 0xFF000000u));
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.drawImage(0, 0, LoadImage(path));
  }
  return *m_tinted.insert(key, tinted);
}

QImage InputDisplayDumper::RenderSkin(const GCPadStatus& pad, bool connected)
{
  const int w = m_skin.Width(), h = m_skin.Height();
  QImage image(w, h, QImage::Format_RGBA8888);
  image.fill(Qt::transparent);

  QPainter p(&image);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const float s = m_skin.scale;
  const float ox = m_skin.offset_x, oy = m_skin.offset_y;
  const float ts = m_skin.tex_size * s;

  auto sx = [&](float v) { return (v + ox) * s; };
  auto sy = [&](float v) { return (v + oy) * s; };
  auto col = [&](const QString& key) -> u32 { return m_skin.colors.value(key, 0); };
  auto drawImg = [&](const QString& name, float x, float y, float fw, float fh, u32 tint = 0) {
    if (name.isEmpty())
      return;
    const QString path = m_skin.tex_dir + QLatin1Char('/') + name;
    const QImage& img = (tint == 0) ? LoadImage(path) : TintedImage(path, tint);
    if (!img.isNull())
      p.drawImage(QRectF(x, y, fw, fh), img, QRectF(img.rect()));
  };

  p.fillRect(image.rect(), ArgbToColor(m_skin.background));

  for (const GCSkinStick& st : m_skin.sticks)
  {
    drawImg(st.gate, sx(st.x), sy(st.y), ts, ts, col(st.gate_color));
    const float nx = (PadAxis(pad, st.key_x) - 128) / 128.0f;
    const float ny = (PadAxis(pad, st.key_y) - 128) / 128.0f;
    drawImg(st.knob, sx(st.x + nx * st.travel), sy(st.y - ny * st.travel),
            ts, ts, col(st.knob_color));
  }

  if (m_skin.has_dpad)
  {
    const GCSkinDpad& d = m_skin.dpad;
    drawImg(d.gate, sx(d.x), sy(d.y), ts, ts, col(d.gate_color));
    for (auto it = d.pressed.begin(); it != d.pressed.end(); ++it)
    {
      if (PadButton(pad, it.key()))
        drawImg(it.value(), sx(d.x), sy(d.y), ts, ts, 0);
    }
  }

  for (const GCSkinButton& btn : m_skin.buttons)
  {
    const QString& name = PadButton(pad, btn.key) ? btn.pressed : btn.filled;
    drawImg(name, sx(btn.x), sy(btn.y), ts, ts, col(btn.color_key));
  }

  for (const GCSkinTrigger& t : m_skin.triggers)
  {
    drawImg(t.base, sx(t.bx), sy(t.by), t.bw * s, t.bh * s, 0);

    const bool digital = PadButton(pad, t.key);
    const float val = digital ? 1.0f : (PadAxis(pad, t.axis) / 255.0f);
    const u32 white = m_skin.colors.value(QStringLiteral("white"), 0xFFFFFFFFu);
    const float fy = sy(t.fill_y);
    const float fh = sy(t.fill_y + t.fill_h) - fy;

    p.setPen(Qt::NoPen);
    p.setBrush(ArgbToColor(white));
    if (t.dir_right)
    {
      const float right_x = sx(t.analog_x + t.analog_w);
      p.drawRect(QRectF(right_x - t.analog_w * s * val, fy, t.analog_w * s * val, fh));
    }
    else
    {
      p.drawRect(QRectF(sx(t.analog_x), fy, t.analog_w * s * val, fh));
    }
    if (t.digital_w > 0)
    {
      if (digital)
        p.drawRect(QRectF(sx(t.digital_x), fy, t.digital_w * s, fh));
      p.setPen(QPen(ArgbToColor(white), 2.0));
      p.setBrush(Qt::NoBrush);
      p.drawLine(QPointF(sx(t.divider_x), fy), QPointF(sx(t.divider_x), fy + fh));
    }
  }

  // Generic conditional overlays: a full-window scrim and/or a tinted image, per `when` state.
  for (const GCSkinOverlay& ov : m_skin.overlays)
  {
    if (!OverlayActive(ov, pad, connected))
      continue;

    p.setOpacity(ov.opacity);
    if ((ov.fill >> 24) != 0)
      p.fillRect(image.rect(), ArgbToColor(ov.fill));

    if (!ov.image.isEmpty())
    {
      const QString path = m_skin.tex_dir + QLatin1Char('/') + ov.image;
      const QImage& img = (ov.tint == 0) ? LoadImage(path) : TintedImage(path, ov.tint);
      if (!img.isNull())
      {
        if (ov.align == QStringLiteral("fill"))
        {
          p.drawImage(image.rect(), img);
        }
        else
        {
          const float iw = img.width() * ov.scale, ih = img.height() * ov.scale;
          p.drawImage(QRectF((w - iw) / 2.0f, (h - ih) / 2.0f, iw, ih), img, QRectF(img.rect()));
        }
      }
    }
    p.setOpacity(1.0);
  }

  return image;
}

void InputDisplayDumper::OnFrameAdvance()
{
#if defined(HAVE_FFMPEG)
  if (!m_active)
    return;

  auto& movie = Core::System::GetInstance().GetMovie();
  const auto status = movie.GetDisplayedPadStatus(m_port);
  const GCPadStatus pad = status.value_or(GCPadStatus{});
  const bool connected = status.has_value() &&
                         (m_skin.gba_mode ? !(pad.button & PAD_BUTTON_Y) : pad.isConnected);

  const QImage image = RenderSkin(pad, connected);
  const u64 ticks = Core::System::GetInstance().GetCoreTiming().GetTicks();

  std::lock_guard lock(m_dump_mutex);
  if (!m_active)
    return;

  if (!m_dump.IsStarted())
  {
    if (!m_dump.Start(m_skin.Width(), m_skin.Height(), ticks, "InputDisplay" DIR_SEP "input"))
    {
      m_active = false;
      return;
    }
  }

  FrameData frame;
  frame.data = image.constBits();
  frame.width = m_skin.Width();
  frame.height = m_skin.Height();
  frame.stride = static_cast<int>(image.bytesPerLine());
  frame.state = m_dump.FetchState(ticks, static_cast<int>(m_frame_count));
  m_dump.AddFrame(frame);
  ++m_frame_count;
#endif
}
