// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/InputDisplayWidget.h"

#include <QCloseEvent>
#include <QPainter>

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

InputDisplayWidget::InputDisplayWidget(const GCSkin& skin, int port, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::WindowStaysOnTopHint), m_skin(skin), m_port(port)
{
  setFixedSize(skin.Width(), skin.Height());
  setWindowTitle(tr("Input Display - Controller %1").arg(m_port + 1));

  connect(&m_timer, &QTimer::timeout, this, &InputDisplayWidget::Poll);
  m_timer.setInterval(16);
  m_timer.start();
}

void InputDisplayWidget::closeEvent(QCloseEvent* event)
{
  emit closed();
  QWidget::closeEvent(event);
}

const QPixmap& InputDisplayWidget::LoadPixmap(const QString& path)
{
  auto it = m_pixmaps.find(path);
  if (it == m_pixmaps.end())
    it = m_pixmaps.insert(path, QPixmap(path));
  return *it;
}

// Multiply-tints a texture: black stays black, white becomes the tint. Mirrors ScriptCanvasWidget.
const QPixmap& InputDisplayWidget::TintedPixmap(const QString& path, u32 argb)
{
  const QString key = path + QLatin1Char('|') + QString::number(argb, 16);
  auto it = m_tinted.find(key);
  if (it != m_tinted.end())
    return *it;

  QPixmap tinted = LoadPixmap(path);
  if (!tinted.isNull())
  {
    QPainter p(&tinted);
    p.setCompositionMode(QPainter::CompositionMode_Multiply);
    p.fillRect(tinted.rect(), ArgbToColor(argb | 0xFF000000u));
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.drawPixmap(0, 0, LoadPixmap(path));
  }
  return *m_tinted.insert(key, tinted);
}

void InputDisplayWidget::Poll()
{
  const auto status = Core::System::GetInstance().GetMovie().GetDisplayedPadStatus(m_port);
  if (status.has_value())
  {
    m_pad = *status;
    m_connected = m_skin.gba_mode ? !(m_pad.button & PAD_BUTTON_Y) : m_pad.isConnected;
  }
  // If nullopt (no game running yet), keep the previous pad state rather than
  // showing "No controller" — the neutral default shows an idle controller.
  update();
}

void InputDisplayWidget::paintEvent(QPaintEvent*)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  DrawSkin(painter);
}

void InputDisplayWidget::DrawSkin(QPainter& p)
{
  const float s = m_skin.scale;
  const float ox = m_skin.offset_x, oy = m_skin.offset_y;
  const float ts = m_skin.tex_size * s;

  auto sx = [&](float v) { return (v + ox) * s; };
  auto sy = [&](float v) { return (v + oy) * s; };
  auto col = [&](const QString& key) -> u32 { return m_skin.colors.value(key, 0); };
  auto drawImg = [&](const QString& name, float x, float y, float w, float h, u32 tint = 0) {
    if (name.isEmpty())
      return;
    const QString path = m_skin.tex_dir + QLatin1Char('/') + name;
    const QPixmap& pix = (tint == 0) ? LoadPixmap(path) : TintedPixmap(path, tint);
    if (!pix.isNull())
      p.drawPixmap(QRectF(x, y, w, h), pix, QRectF(pix.rect()));
  };

  p.fillRect(rect(), ArgbToColor(m_skin.background));

  for (const GCSkinStick& st : m_skin.sticks)
  {
    drawImg(st.gate, sx(st.x), sy(st.y), ts, ts, col(st.gate_color));
    const float nx = (PadAxis(m_pad, st.key_x) - 128) / 128.0f;
    const float ny = (PadAxis(m_pad, st.key_y) - 128) / 128.0f;
    drawImg(st.knob, sx(st.x + nx * st.travel), sy(st.y - ny * st.travel),
            ts, ts, col(st.knob_color));
  }

  if (m_skin.has_dpad)
  {
    const GCSkinDpad& d = m_skin.dpad;
    drawImg(d.gate, sx(d.x), sy(d.y), ts, ts, col(d.gate_color));
    for (auto it = d.pressed.begin(); it != d.pressed.end(); ++it)
    {
      if (PadButton(m_pad, it.key()))
        drawImg(it.value(), sx(d.x), sy(d.y), ts, ts, 0);
    }
  }

  for (const GCSkinButton& btn : m_skin.buttons)
  {
    const QString& name = PadButton(m_pad, btn.key) ? btn.pressed : btn.filled;
    drawImg(name, sx(btn.x), sy(btn.y), ts, ts, col(btn.color_key));
  }

  for (const GCSkinTrigger& t : m_skin.triggers)
  {
    drawImg(t.base, sx(t.bx), sy(t.by), t.bw * s, t.bh * s, 0);

    const bool digital = PadButton(m_pad, t.key);
    const float val = digital ? 1.0f : (PadAxis(m_pad, t.axis) / 255.0f);
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
    if (!OverlayActive(ov, m_pad, m_connected))
      continue;

    p.setOpacity(ov.opacity);
    if ((ov.fill >> 24) != 0)
      p.fillRect(rect(), ArgbToColor(ov.fill));

    if (!ov.image.isEmpty())
    {
      const QString path = m_skin.tex_dir + QLatin1Char('/') + ov.image;
      const QPixmap& pix = (ov.tint == 0) ? LoadPixmap(path) : TintedPixmap(path, ov.tint);
      if (!pix.isNull())
      {
        if (ov.align == QStringLiteral("fill"))
        {
          p.drawPixmap(rect(), pix);
        }
        else
        {
          const float iw = pix.width() * ov.scale, ih = pix.height() * ov.scale;
          p.drawPixmap(QRectF((width() - iw) / 2.0f, (height() - ih) / 2.0f, iw, ih), pix,
                       QRectF(pix.rect()));
        }
      }
    }
    p.setOpacity(1.0);
  }
}
